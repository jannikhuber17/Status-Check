#include "plugin.h"
#include "ts3_functions.h"
#include "status_manager.h"
#include "supabase_client.h"
#include "hotkey_manager.h"
#include "hud_window.h"
#include "settings_dialog.h"
#include "debug_log.h"

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QPixmap>
#include <QPainter>
#include <QFile>
#include <cstring>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#  include <limits.h>
#  include <dlfcn.h>
#endif

// ---- Supabase config ----
static const char* kSupabaseUrl = "https://siadwqodyglyvhljcoyo.supabase.co";
static const char* kSupabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InNpYWR3cW9keWdseXZobGpjb3lvIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODIzMzA2MjMsImV4cCI6MjA5NzkwNjYyM30.wZsbP14LJGP18JghMi4rZBHtNyuEL9BsQge7FGO2jYA";
static const char* kChannelName = "viper_readycheck";

// ---- IDs ----
static constexpr int kHkCycle      = 1;  // WinAPI fallback (Ctrl+Shift+R)
static constexpr int kMenuSettings = 1;
static constexpr int kMenuHudShow  = 2;
static constexpr int kMenuHudHide  = 3;

// ---- Plugin state (file-scope, internal linkage) ----
static TS3Functions    s_funcs{};
static char            s_pluginID[512] = {};   // set by ts3plugin_setPluginID — use this, NOT the DLL name
static QApplication*   s_app         = nullptr;
static int             s_fakeArgc    = 1;
static char*           s_fakeArgv[]  = { const_cast<char*>("staffel_viper_readycheck"), nullptr };

static StatusManager*  s_statusMgr   = nullptr;
static SupabaseClient* s_supabase    = nullptr;
static HotkeyManager*  s_hotkey      = nullptr;
static HudWindow*      s_hud         = nullptr;
static SettingsDialog* s_settingsDlg = nullptr;
static QString         s_menuIconPath;

static QElapsedTimer   s_hotkeyDebounce;
static uint64_t        s_serverHandle  = 0;
static QString         s_localNickname;

// ---- Internal helpers ----

static bool debounceOk() {
    if (s_hotkeyDebounce.isValid() && s_hotkeyDebounce.elapsed() < 150) return false;
    s_hotkeyDebounce.restart();
    return true;
}

static void triggerAction(const char* keyword) {
    if (!s_statusMgr || !debounceOk()) return;
    if      (strcmp(keyword, "cycle")     == 0) s_statusMgr->cycleLocalStatus();
    else if (strcmp(keyword, "not_ready") == 0) s_statusMgr->setLocalStatus(ReadyStatus::NotReady);
    else if (strcmp(keyword, "standby")   == 0) s_statusMgr->setLocalStatus(ReadyStatus::Standby);
    else if (strcmp(keyword, "ready")     == 0) s_statusMgr->setLocalStatus(ReadyStatus::Ready);
}

static void applyHotkeys() {
    if (!s_hotkey) return;
    s_hotkey->unregisterHotkey(kHkCycle);
    bool ok = s_hotkey->registerHotkey(kHkCycle, Qt::Key_R, Qt::ControlModifier | Qt::ShiftModifier);
    viperPre(ok ? "applyHotkeys: Ctrl+Shift+R registered OK\n"
                : "applyHotkeys: RegisterHotKey FAILED (another app may own it)\n");
}

static QString getDllDir() {
#ifdef _WIN32
    HMODULE hMod = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&getDllDir), &hMod);
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(hMod, path, MAX_PATH);
    return QFileInfo(QString::fromWCharArray(path)).absolutePath();
#else
    Dl_info info{};
    dladdr(reinterpret_cast<void*>(&getDllDir), &info);
    return info.dli_fname ? QFileInfo(QString::fromUtf8(info.dli_fname)).absolutePath()
                          : QString();
#endif
}

static QString getLocalUid() {
#ifdef _WIN32
    wchar_t comp[256] = {}, user[256] = {};
    DWORD cl = 256, ul = 256;
    GetComputerNameW(comp, &cl);
    GetUserNameW(user, &ul);
    return QString::fromWCharArray(comp) + "_" + QString::fromWCharArray(user);
#else
    char host[HOST_NAME_MAX] = {};
    gethostname(host, sizeof(host));
    const char* user = getlogin();
    return QString::fromUtf8(host) + "_" + QString::fromUtf8(user ? user : "user");
#endif
}

static QString getLocalNickname() {
    if (!s_localNickname.isEmpty()) return s_localNickname;
#ifdef _WIN32
    wchar_t user[256] = {};
    DWORD ul = 256;
    return GetUserNameW(user, &ul) ? QString::fromWCharArray(user) : QStringLiteral("Unknown");
#else
    const char* user = getlogin();
    return user ? QString::fromUtf8(user) : QStringLiteral("Unknown");
#endif
}

// ---- Update check ----

static bool isNewerVersion(const QString& remote, const QString& local) {
    auto parse = [](const QString& v) {
        QList<int> p;
        for (const QString& s : v.split('.')) p << s.toInt();
        while (p.size() < 3) p << 0;
        return p;
    };
    auto r = parse(remote), l = parse(local);
    for (int i = 0; i < 3; ++i) {
        if (r[i] > l[i]) return true;
        if (r[i] < l[i]) return false;
    }
    return false;
}

static void showUpdateDialog(const QString& newVersion) {
    auto* dlg = new QDialog(nullptr);
    dlg->setWindowTitle("Status Check \xe2\x80\x93 Update verf\xc3\xbcgbar");
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowFlags((dlg->windowFlags() | Qt::WindowStaysOnTopHint)
                        & ~Qt::WindowContextHelpButtonHint);
    dlg->setMinimumWidth(340);

    auto* layout = new QVBoxLayout(dlg);
    auto* lbl = new QLabel(
        QString("Eine neue Version ist verf\xc3\xbcgbar: <b>v%1</b><br>"
                "Installierte Version: v%2")
        .arg(newVersion, QString(PLUGIN_VERSION)));
    lbl->setWordWrap(true);
    layout->addWidget(lbl);
    layout->addSpacing(8);

    auto* btns = new QHBoxLayout();
    auto* btnDl     = new QPushButton("Jetzt herunterladen");
    auto* btnLater  = new QPushButton("Sp\xc3\xa4ter");
    btns->addWidget(btnDl);
    btns->addWidget(btnLater);
    layout->addLayout(btns);

    QObject::connect(btnDl, &QPushButton::clicked, dlg, [dlg]() {
        QDesktopServices::openUrl(QUrl("https://github.com/jannikhuber17/Status-Check/releases/latest"));
        dlg->close();
    });
    QObject::connect(btnLater, &QPushButton::clicked, dlg, &QDialog::close);
    dlg->show();
}

static void checkForUpdate() {
    auto* nam = new QNetworkAccessManager();
    QNetworkRequest req(QUrl("https://api.github.com/repos/jannikhuber17/Status-Check/releases/latest"));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QString("StatusCheck-TS3/%1").arg(PLUGIN_VERSION));
    req.setRawHeader("Accept", "application/vnd.github+json");

    auto* reply = nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString tag = doc.object().value("tag_name").toString();
        if (tag.startsWith('v')) tag = tag.mid(1);
        if (!tag.isEmpty() && isNewerVersion(tag, QString(PLUGIN_VERSION)))
            showUpdateDialog(tag);
    });
}

// ---- namespace Plugin — API for SettingsDialog ----

namespace Plugin {

void openHotkeyDialog(const char* keyword) {
    viperPre(("openHotkeyDialog: kw=" + std::string(keyword)
              + " id=" + std::string(s_pluginID) + "\n").c_str());
    if (s_funcs.requestHotkeyInputDialog)
        s_funcs.requestHotkeyInputDialog(s_pluginID, keyword, 1, nullptr);
    else
        viperPre("openHotkeyDialog: requestHotkeyInputDialog is NULL\n");
}

QString getCurrentHotkey(const char* keyword) {
    if (!s_funcs.getHotkeyFromKeyword) return {};
    const char* kws[1]   = { keyword };
    char        buf[512] = {};
    char*       bufs[1]  = { buf };
    if (s_funcs.getHotkeyFromKeyword(s_pluginID, kws, bufs, 1, 511) == 0)
        return QString::fromUtf8(buf);
    return {};
}

} // namespace Plugin

// ============================================================
//  TS3 plugin exports — all at global scope, no namespace
// ============================================================

extern "C" const char* ts3plugin_name()        { return "Status Check"; }
extern "C" const char* ts3plugin_version()     { return PLUGIN_VERSION; }
extern "C" int         ts3plugin_apiVersion()  { return 26; }
extern "C" const char* ts3plugin_author()      { return "Staffel Viper"; }
extern "C" const char* ts3plugin_description() {
    return "Gaming overlay HUD showing channel member ready states.\n"
           "Toggle own status with Ctrl+Shift+R (configurable).";
}
extern "C" void ts3plugin_freeMemory(void* data)                      { free(data); }
extern "C" void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) { s_funcs = funcs; }

extern "C" void ts3plugin_registerPluginID(const char* id) {
    strncpy(s_pluginID, id ? id : "", sizeof(s_pluginID) - 1);
    viperPre(("registerPluginID: " + std::string(s_pluginID) + "\n").c_str());
}

// ---- Init ----

extern "C" int ts3plugin_init() {
#ifdef _WIN32
    { HANDLE f = CreateFileW(L"C:\\Users\\Public\\viperrc_init.txt",
          GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
      if (f != INVALID_HANDLE_VALUE) CloseHandle(f); }
#endif

    viperDbg("1 init start\n");

    if (!QApplication::instance()) {
        viperDbg("2 no QApp, creating\n");
        QCoreApplication::addLibraryPath(getDllDir());
        s_app = new QApplication(s_fakeArgc, s_fakeArgv);
        viperDbg("3 QApp created\n");
    } else {
        viperDbg("2 reusing existing QApp\n");
    }

    viperDbg("4 new StatusManager\n");
    s_statusMgr = new StatusManager();
    s_statusMgr->setLocalUid(getLocalUid());
    viperDbg("5 new SupabaseClient\n");
    s_supabase  = new SupabaseClient();
    viperDbg("6 new HotkeyManager\n");
    s_hotkey    = new HotkeyManager();
    viperDbg("7 new HudWindow\n");
    s_hud       = new HudWindow();
    s_hud->setLocalUid(getLocalUid());
    viperDbg("8 HudWindow created\n");

    viperDbg("9a connect memberUpdated\n");
    QObject::connect(s_supabase, &SupabaseClient::memberUpdated,
                     s_statusMgr, &StatusManager::updateMember);
    viperDbg("9b connect memberRemoved\n");
    QObject::connect(s_supabase, &SupabaseClient::memberRemoved,
                     s_statusMgr, &StatusManager::removeMember);

    viperDbg("9c connect membersChanged\n");
    QObject::connect(s_statusMgr, &StatusManager::membersChanged, s_statusMgr, []{
        s_hud->setMembers(s_statusMgr->members());
    });

    viperDbg("9d connect localStatusChanged\n");
    QObject::connect(s_statusMgr, &StatusManager::localStatusChanged, s_statusMgr,
                     [](ReadyStatus newStatus){
        if (s_serverHandle == 0) return;
        if (s_hud && !s_hud->isVisible()) s_hud->show();
        s_supabase->publishStatus(getLocalUid(), getLocalNickname(), newStatus);
    });

    viperDbg("9e applyHotkeys\n");
    applyHotkeys();
    QObject::connect(s_hotkey, &HotkeyManager::hotkeyPressed, s_hotkey, [](int id) {
        if (id == kHkCycle) triggerAction("cycle");
    });

    viperDbg("9g configure supabase\n");
    s_supabase->configure(QString::fromUtf8(kSupabaseUrl),
                          QString::fromUtf8(kSupabaseKey));
    viperDbg("9h connectRealtime\n");
    s_supabase->connectRealtime(QString::fromUtf8(kChannelName));

    viperDbg("10 position HUD\n");
    s_hud->setFixedWidth(260);
    s_hud->move(50, 50);

    {
        QString iconPath = getDllDir() + "/staffel_viper_readycheck_menuicon.png";
        if (!QFile::exists(iconPath)) {
            QPixmap pm(16, 16);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(Qt::white);
            p.setPen(Qt::NoPen);
            p.drawEllipse(1, 1, 14, 14);
            p.end();
            pm.save(iconPath, "PNG");
        }
        s_menuIconPath = iconPath;
    }

    viperDbg("11 init complete\n");
    QTimer::singleShot(5000, &checkForUpdate);
    return 0;
}

// ---- Shutdown ----

extern "C" void ts3plugin_shutdown() {
    if (s_settingsDlg) { s_settingsDlg->close(); s_settingsDlg = nullptr; }
    if (s_hotkey)  { delete s_hotkey;  s_hotkey  = nullptr; }
    if (s_supabase) {
        s_supabase->removeEntry(getLocalUid());
        for (int i = 0; i < 4; ++i) QApplication::processEvents(QEventLoop::AllEvents, 100);
        s_supabase->disconnect();
        delete s_supabase; s_supabase = nullptr;
    }
    if (s_hud)      { s_hud->hide();  delete s_hud;      s_hud      = nullptr; }
    if (s_statusMgr){ delete s_statusMgr; s_statusMgr = nullptr; }
    if (s_app)      { delete s_app;       s_app       = nullptr; }
}

// ---- Menu ----

extern "C" void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon) {
    viperMenu("M1 called\n");
    if (!s_menuIconPath.isEmpty()) {
        QByteArray ba = s_menuIconPath.toUtf8();
        *menuIcon = static_cast<char*>(malloc(ba.size() + 1));
        memcpy(*menuIcon, ba.constData(), ba.size() + 1);
    } else {
        *menuIcon = nullptr;
    }

    *menuItems = static_cast<PluginMenuItem**>(malloc(4 * sizeof(PluginMenuItem*)));

    auto makeItem = [](int id, const char* text) -> PluginMenuItem* {
        auto* m = static_cast<PluginMenuItem*>(malloc(sizeof(PluginMenuItem)));
        m->type = PLUGIN_MENU_TYPE_GLOBAL;
        m->id   = id;
        strncpy(m->text, text, PLUGIN_MENU_BUFSZ - 1);
        m->text[PLUGIN_MENU_BUFSZ - 1] = '\0';
        m->icon[0] = '\0';
        return m;
    };

    (*menuItems)[0] = makeItem(kMenuSettings, "Einstellungen (Tastenzuweisung)");
    (*menuItems)[1] = makeItem(kMenuHudShow,  "HUD an");
    (*menuItems)[2] = makeItem(kMenuHudHide,  "HUD aus");
    (*menuItems)[3] = nullptr;
}

extern "C" void ts3plugin_onMenuItemEvent(uint64 /*scHandlerID*/, enum PluginMenuType /*type*/,
                                           int menuItemID, uint64 /*selectedItemID*/) {
    if (menuItemID == kMenuSettings) {
        if (!s_settingsDlg) {
            s_settingsDlg = new SettingsDialog(nullptr);
            QObject::connect(s_settingsDlg, &QDialog::finished, s_settingsDlg, [](int) {
                applyHotkeys();
                s_settingsDlg->deleteLater();
                s_settingsDlg = nullptr;
            });
        }
        s_settingsDlg->show();
        s_settingsDlg->raise();
        s_settingsDlg->activateWindow();
    } else if (menuItemID == kMenuHudShow && s_hud) {
        s_hud->show();
    } else if (menuItemID == kMenuHudHide && s_hud) {
        s_hud->hide();
    }
}

// ---- Configure ----

extern "C" int ts3plugin_offersConfigure() { return PLUGIN_OFFERS_CONFIGURE_QT_THREAD; }

extern "C" void ts3plugin_configure(void* /*handle*/, void* /*qParentWidget*/) {
    viperPre("configure called\n");
    if (!s_settingsDlg) {
        s_settingsDlg = new SettingsDialog(nullptr);
        QObject::connect(s_settingsDlg, &QDialog::finished, s_settingsDlg, [](int) {
            applyHotkeys();
            s_settingsDlg->deleteLater();
            s_settingsDlg = nullptr;
        });
    }
    s_settingsDlg->show();
    s_settingsDlg->raise();
    s_settingsDlg->activateWindow();
    viperPre("configure: dialog shown\n");
}

// ---- Hotkeys ----

extern "C" void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) {
    viperPre("initHotkeys called\n");
    *hotkeys = static_cast<PluginHotkey**>(malloc(5 * sizeof(PluginHotkey*)));
    auto make = [](const char* kw, const char* desc) -> PluginHotkey* {
        auto* h = static_cast<PluginHotkey*>(malloc(sizeof(PluginHotkey)));
        strncpy(h->keyword,     kw,   PLUGIN_HOTKEY_BUFSZ - 1); h->keyword[PLUGIN_HOTKEY_BUFSZ - 1]     = '\0';
        strncpy(h->description, desc, PLUGIN_HOTKEY_BUFSZ - 1); h->description[PLUGIN_HOTKEY_BUFSZ - 1] = '\0';
        return h;
    };
    (*hotkeys)[0] = make("cycle",     "Status cyclen");
    (*hotkeys)[1] = make("not_ready", "Status: Not Ready");
    (*hotkeys)[2] = make("standby",   "Status: Standby");
    (*hotkeys)[3] = make("ready",     "Status: Ready");
    (*hotkeys)[4] = nullptr;
    viperPre("initHotkeys done\n");
}

extern "C" void ts3plugin_onHotkeyEvent(const char* keyword) {
    if (s_serverHandle == 0) return;
    triggerAction(keyword);
}

extern "C" void ts3plugin_onHotkeyRecordedEvent(const char* /*keyword*/, const char* /*key*/) {
    if (s_settingsDlg)
        s_settingsDlg->refreshHotkeyLabels();
}

// ---- Event callbacks ----

extern "C" void ts3plugin_onConnectStatusChangeEvent(
        uint64 serverConnectionHandlerID, int newStatus, unsigned int /*errorNumber*/)
{
    if (newStatus == STATUS_CONNECTION_ESTABLISHED) {
        s_serverHandle = serverConnectionHandlerID;

        anyID myClientId = 0;
        if (s_funcs.getClientID(s_serverHandle, &myClientId) == 0 && myClientId != 0) {
            char* nick = nullptr;
            if (s_funcs.getClientVariableAsString(s_serverHandle, myClientId,
                                                   CLIENT_NICKNAME, &nick) == 0 && nick) {
                s_localNickname = QString::fromUtf8(nick);
                s_funcs.freeMemory(nick);
            }
        }

        s_statusMgr->resetLocalStatus();
        s_supabase->removeEntry(getLocalUid());
        s_supabase->fetchAllMembers();

    } else if (newStatus == STATUS_DISCONNECTED) {
        s_serverHandle = 0;
        s_localNickname.clear();
        s_statusMgr->clearAll();
    }
}

extern "C" void ts3plugin_onClientMoveEvent(
        uint64 /*serverConnectionHandlerID*/, anyID /*clientID*/,
        uint64 /*oldChannelID*/, uint64 /*newChannelID*/,
        int /*visibility*/, const char* /*moveMessage*/) {}

extern "C" void ts3plugin_onPluginCommandEvent(
        uint64 /*serverConnectionHandlerID*/,
        const char* pluginName, const char* pluginCommand)
{
    if (strcmp(pluginName, "staffel_viper_readycheck") != 0) return;
    QString cmd = QString::fromUtf8(pluginCommand);
    QStringList parts = cmd.split(' ');
    if (parts.size() < 3 || parts[0] != "STATUS") return;
    qDebug() << "[ViperRC] Plugin command received:" << cmd;
}
