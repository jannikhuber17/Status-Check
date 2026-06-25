#include "settings_dialog.h"
#include "plugin.h"
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Status Check \xe2\x80\x93 Einstellungen");
    setMinimumWidth(460);
    setWindowFlags((windowFlags() | Qt::WindowStaysOnTopHint)
                   & ~Qt::WindowContextHelpButtonHint);

    m_lblCycle    = new QLabel(this);
    m_lblNotReady = new QLabel(this);
    m_lblStandby  = new QLabel(this);
    m_lblReady    = new QLabel(this);

    struct Row { const char* label; const char* kw; QLabel** lbl; };
    Row rows[] = {
        { "Status cyclen:", "cycle",     &m_lblCycle    },
        { "Not Ready:",     "not_ready", &m_lblNotReady },
        { "Standby:",       "standby",   &m_lblStandby  },
        { "Ready:",         "ready",     &m_lblReady    },
    };

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    for (auto& r : rows) {
        auto* row = new QWidget(this);
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);

        (*r.lbl)->setMinimumWidth(170);
        lay->addWidget(*r.lbl, 1);

        auto* btn = new QPushButton("Zuweisen\xe2\x80\xa6", row);
        const char* kw = r.kw;
        connect(btn, &QPushButton::clicked, this, [kw]() {
            Plugin::openHotkeyDialog(kw);
        });
        lay->addWidget(btn);

        form->addRow(r.label, row);
    }

    auto* note = new QLabel(
        "<small><i>Unterst\xc3\xbctzt Tastatur, Joystick und Stream Deck.</i></small>");
    note->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addSpacing(8);
    layout->addWidget(note);
    layout->addSpacing(4);
    layout->addWidget(buttons);

    refreshHotkeyLabels();
}

void SettingsDialog::refreshHotkeyLabels() {
    struct { const char* kw; QLabel* lbl; } items[] = {
        { "cycle",     m_lblCycle    },
        { "not_ready", m_lblNotReady },
        { "standby",   m_lblStandby  },
        { "ready",     m_lblReady    },
    };
    for (auto& it : items) {
        QString hk = Plugin::getCurrentHotkey(it.kw);
        it.lbl->setText(hk.isEmpty()
            ? QString("<i style='color:#888;'>nicht zugewiesen</i>")
            : hk.toHtmlEscaped());
        it.lbl->setTextFormat(Qt::RichText);
    }
}
