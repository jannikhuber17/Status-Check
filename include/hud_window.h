#pragma once
#include "member_status.h"
#include <QWidget>
#include <QList>
#include <QMap>
#include <QTimer>

class HudWindow : public QWidget {
    Q_OBJECT

public:
    explicit HudWindow(QWidget* parent = nullptr);

    void setMembers(const QList<MemberStatus>& members);
    void setLocalUid(const QString& uid);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private Q_SLOTS:
    void onGlowTick();

private:
    QList<MemberStatus>        m_members;
    QString                    m_localUid;
    QPoint                     m_dragStart;
    bool                       m_dragging = false;

    // Glow effect: tracks previous statuses and glow start times (ms)
    QMap<QString, ReadyStatus> m_prevStatuses;
    QMap<QString, qint64>      m_glowStart;
    QTimer*                    m_glowTimer = nullptr;

    static constexpr int kRowHeight    = 28;
    static constexpr int kDotRadius    = 7;
    static constexpr int kPadding      = 12;
    static constexpr int kHeaderHeight = 24;
    static constexpr int kGlowMs       = 3000;
};
