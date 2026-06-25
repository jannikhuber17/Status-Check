#include "supabase_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QDebug>

static constexpr int kHeartbeatIntervalMs = 25000;

SupabaseClient::SupabaseClient(QObject* parent) : QObject(parent) {
    connect(&m_ws, &QWebSocket::connected,           this, &SupabaseClient::onWsConnected);
    connect(&m_ws, &QWebSocket::disconnected,        this, &SupabaseClient::onWsDisconnected);
    connect(&m_ws, &QWebSocket::textMessageReceived, this, &SupabaseClient::onWsTextMessage);
    connect(&m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &SupabaseClient::onWsError);
}

void SupabaseClient::configure(const QString& url, const QString& anonKey) {
    m_url     = url;
    m_anonKey = anonKey;
}

void SupabaseClient::connectRealtime(const QString& channelName) {
    m_channelName = channelName;

    QString wsUrl = m_url;
    wsUrl.replace("https://", "wss://").replace("http://", "ws://");
    wsUrl += "/realtime/v1/websocket?apikey=" + m_anonKey + "&vsn=1.0.0";

    QUrl url(wsUrl);
    QNetworkRequest req(url);
    req.setRawHeader("apikey", m_anonKey.toUtf8());
    m_ws.open(req);
}

void SupabaseClient::publishStatus(const QString& uid, const QString& nickname, ReadyStatus status, uint64_t ts3ChannelId) {
    // REST UPSERT — Supabase then broadcasts the row change via Postgres Changes to all WS subscribers
    QJsonObject body{
        {"uid",            uid},
        {"nickname",       nickname},
        {"status",         static_cast<int>(status)},
        {"channel_name",   m_channelName},
        {"ts3_channel_id", static_cast<qint64>(ts3ChannelId)}
    };

    QNetworkRequest req = buildRestRequest("/rest/v1/ready_check");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Prefer", "resolution=merge-duplicates");  // upsert on uid PK

    QNetworkReply* reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, [reply]{
        if (reply->error() != QNetworkReply::NoError)
            qWarning() << "[ViperRC] publishStatus error:" << reply->errorString();
        reply->deleteLater();
    });
}

void SupabaseClient::fetchAllMembers() {
    // GET all rows for this channel — used on initial connect to populate HUD
    QNetworkRequest req = buildRestRequest(
        "/rest/v1/ready_check?channel_name=eq." + m_channelName);

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[ViperRC] fetchAllMembers error:" << reply->errorString();
            reply->deleteLater();
            return;
        }

        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(reply->readAll(), &err);
        reply->deleteLater();

        if (err.error != QJsonParseError::NoError || !doc.isArray()) return;

        for (const QJsonValue& val : doc.array()) {
            QJsonObject row = val.toObject();
            MemberStatus ms;
            ms.uid          = row["uid"].toString();
            ms.nickname     = row["nickname"].toString();
            ms.status       = static_cast<ReadyStatus>(row["status"].toInt());
            ms.ts3ChannelId = static_cast<uint64_t>(row["ts3_channel_id"].toVariant().toLongLong());
            if (!ms.uid.isEmpty())
                Q_EMIT memberUpdated(ms);
        }
    });
}

void SupabaseClient::removeEntry(const QString& uid) {
    QNetworkRequest req = buildRestRequest(
        "/rest/v1/ready_check?uid=eq." + uid + "&channel_name=eq." + m_channelName);
    QNetworkReply* reply = m_nam.sendCustomRequest(req, "DELETE");
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void SupabaseClient::disconnect() {
    m_ws.close();
}

// ---- WebSocket slots ----

void SupabaseClient::onWsConnected() {
    m_connected = true;
    Q_EMIT connectionStateChanged(true);
    sendSubscribeMessage();

    auto* heartbeat = new QTimer(this);
    heartbeat->setInterval(kHeartbeatIntervalMs);
    connect(heartbeat, &QTimer::timeout, this, &SupabaseClient::sendHeartbeat);
    heartbeat->start();
}

void SupabaseClient::onWsDisconnected() {
    m_connected = false;
    Q_EMIT connectionStateChanged(false);
}

void SupabaseClient::onWsTextMessage(const QString& message) {
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) return;

    auto root  = doc.object();
    QString event = root["event"].toString();

    if (event == "postgres_changes") {
        handlePostgresChange(root["payload"].toObject()["data"].toObject());
    }
}

void SupabaseClient::onWsError(QAbstractSocket::SocketError error) {
    qWarning() << "[ViperRC] WebSocket error:" << error << m_ws.errorString();
}

// ---- Private helpers ----

void SupabaseClient::sendSubscribeMessage() {
    // Subscribe to all Postgres Changes (*) on the ready_check table
    QJsonObject pgChange{
        {"event",  "*"},
        {"schema", "public"},
        {"table",  "ready_check"}
    };

    QJsonObject config{
        {"broadcast",       QJsonObject{{"self", false}}},
        {"presence",        QJsonObject{{"key",  ""}}},
        {"postgres_changes", QJsonArray{pgChange}}
    };

    QJsonObject msg{
        {"event",   "phx_join"},
        {"topic",   "realtime:public:ready_check"},
        {"payload", QJsonObject{{"config", config}}},
        {"ref",     "1"}
    };
    m_ws.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void SupabaseClient::sendHeartbeat() {
    QJsonObject msg{
        {"event",   "heartbeat"},
        {"topic",   "phoenix"},
        {"payload", QJsonObject{}},
        {"ref",     QJsonValue()}
    };
    m_ws.sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void SupabaseClient::handlePostgresChange(const QJsonObject& data) {
    QString type = data["type"].toString();  // INSERT, UPDATE, DELETE

    if (type == "DELETE") {
        QString uid = data["old_record"].toObject()["uid"].toString();
        if (!uid.isEmpty())
            Q_EMIT memberRemoved(uid);
        return;
    }

    // INSERT or UPDATE
    QJsonObject record = data["record"].toObject();
    MemberStatus ms;
    ms.uid          = record["uid"].toString();
    ms.nickname     = record["nickname"].toString();
    ms.status       = static_cast<ReadyStatus>(record["status"].toInt());
    ms.ts3ChannelId = static_cast<uint64_t>(record["ts3_channel_id"].toVariant().toLongLong());
    if (!ms.uid.isEmpty())
        Q_EMIT memberUpdated(ms);
}

QNetworkRequest SupabaseClient::buildRestRequest(const QString& path) const {
    QNetworkRequest req(QUrl(m_url + path));
    req.setRawHeader("apikey",        m_anonKey.toUtf8());
    req.setRawHeader("Authorization", ("Bearer " + m_anonKey).toUtf8());
    return req;
}
