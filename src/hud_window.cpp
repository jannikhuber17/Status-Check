#include "hud_window.h"
#include "debug_log.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QDateTime>

HudWindow::HudWindow(QWidget* parent)
    : QWidget(parent)
{
    viperDbg("HW1 setWindowFlags\n");
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool
                 | Qt::NoDropShadowWindowHint);
    viperDbg("HW2 WA_TranslucentBackground\n");
    setAttribute(Qt::WA_TranslucentBackground);
    viperDbg("HW3 WA_ShowWithoutActivating\n");
    setAttribute(Qt::WA_ShowWithoutActivating);
    viperDbg("HW4 WA_NoSystemBackground\n");
    setAttribute(Qt::WA_NoSystemBackground);
    viperDbg("HW5 setMinimumWidth\n");
    setMinimumWidth(220);
    setFixedHeight(kHeaderHeight + kPadding);

    m_glowTimer = new QTimer(this);
    m_glowTimer->setInterval(30);
    connect(m_glowTimer, &QTimer::timeout, this, &HudWindow::onGlowTick);

    viperDbg("HW6 ctor done\n");
}

void HudWindow::setMembers(const QList<MemberStatus>& members) {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool anyNewGlow = false;

    for (const auto& member : members) {
        // Only glow on status CHANGE (not on initial appearance)
        if (m_prevStatuses.contains(member.uid) &&
            m_prevStatuses.value(member.uid) != member.status) {
            m_glowStart[member.uid] = now;
            anyNewGlow = true;
        }
        m_prevStatuses[member.uid] = member.status;
    }

    // Remove entries for members who left
    for (const auto& uid : m_prevStatuses.keys()) {
        bool found = false;
        for (const auto& m : members) { if (m.uid == uid) { found = true; break; } }
        if (!found) { m_prevStatuses.remove(uid); m_glowStart.remove(uid); }
    }

    m_members = members;
    int h = members.isEmpty()
        ? kHeaderHeight + kPadding
        : kHeaderHeight + kPadding + members.size() * kRowHeight + kPadding;
    setFixedHeight(h);

    if (anyNewGlow && !m_glowTimer->isActive())
        m_glowTimer->start();
    update();
}

void HudWindow::onGlowTick() {
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool anyActive = false;
    for (auto it = m_glowStart.begin(); it != m_glowStart.end(); ++it) {
        if (now - it.value() < kGlowMs) { anyActive = true; break; }
    }
    if (!anyActive) m_glowTimer->stop();
    update();
}

void HudWindow::setLocalUid(const QString& uid) {
    m_localUid = uid;
}

void HudWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background panel
    QPainterPath bg;
    bg.addRoundedRect(rect(), 10, 10);
    p.fillPath(bg, QColor(15, 15, 20, 200));

    // Header
    QFont headerFont("Segoe UI", 9, QFont::Bold);
    p.setFont(headerFont);
    p.setPen(QColor(180, 180, 200));
    p.drawText(QRect(kPadding, 0, width() - kPadding * 2, kHeaderHeight),
               Qt::AlignVCenter | Qt::AlignLeft, "STATUS CHECK");

    // Separator
    p.setPen(QColor(60, 60, 80));
    p.drawLine(kPadding, kHeaderHeight, width() - kPadding, kHeaderHeight);

    QFont nameFont("Segoe UI", 9);
    p.setFont(nameFont);

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int y = kHeaderHeight + kPadding;

    for (const auto& member : m_members) {
        bool isLocal = (!m_localUid.isEmpty() && member.uid == m_localUid);

        // Glow effect: outer pulsing ring for 2 seconds after status change
        if (m_glowStart.contains(member.uid)) {
            qint64 elapsed = now - m_glowStart.value(member.uid);
            if (elapsed < kGlowMs) {
                float t = 1.0f - float(elapsed) / float(kGlowMs);  // 1→0
                int alpha = int(220 * t);
                QColor glow = member.statusColor();
                glow.setAlpha(alpha);
                p.setBrush(glow);
                p.setPen(Qt::NoPen);
                int gr = kDotRadius + 5;
                int cx = kPadding + kDotRadius;
                int cy = y + kRowHeight / 2;
                p.drawEllipse(cx - gr, cy - gr, gr * 2, gr * 2);
            }
        }

        // Status dot (drawn on top of glow)
        QColor dotColor = member.statusColor();
        // Brighten dot while glowing
        if (m_glowStart.contains(member.uid)) {
            qint64 elapsed = now - m_glowStart.value(member.uid);
            if (elapsed < kGlowMs) {
                float t = 1.0f - float(elapsed) / float(kGlowMs);
                dotColor = dotColor.lighter(100 + int(60 * t));
            }
        }
        p.setBrush(dotColor);
        p.setPen(Qt::NoPen);
        p.drawEllipse(kPadding, y + (kRowHeight - kDotRadius * 2) / 2,
                      kDotRadius * 2, kDotRadius * 2);

        // Nickname
        QFont nf = nameFont;
        nf.setBold(isLocal);
        p.setFont(nf);
        p.setPen(isLocal ? QColor(255, 255, 255) : QColor(200, 200, 210));

        QString name = member.nickname;
        if (isLocal) name += " \xe2\x98\x85";

        const int nameX   = kPadding + kDotRadius * 2 + 8;
        const int statusW = 76;
        const int nameW   = width() - nameX - kPadding - statusW - 4;

        QFontMetrics fm(nf);
        name = fm.elidedText(name, Qt::ElideRight, nameW);
        p.drawText(nameX, y, nameW, kRowHeight, Qt::AlignVCenter | Qt::AlignLeft, name);

        // Status label
        p.setFont(QFont("Segoe UI", 7));
        p.setPen(member.statusColor().lighter(130));
        p.drawText(width() - statusW - kPadding, y, statusW, kRowHeight,
                   Qt::AlignVCenter | Qt::AlignRight, member.statusLabel());

        y += kRowHeight;
    }
}

void HudWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_dragStart = e->globalPos() - frameGeometry().topLeft();
        m_dragging  = true;
    }
}

void HudWindow::mouseMoveEvent(QMouseEvent* e) {
    if (m_dragging && (e->buttons() & Qt::LeftButton))
        move(e->globalPos() - m_dragStart);
}

void HudWindow::mouseReleaseEvent(QMouseEvent*) {
    m_dragging = false;
}
