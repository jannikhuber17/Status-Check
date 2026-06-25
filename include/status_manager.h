#pragma once
#include "member_status.h"
#include <QObject>
#include <QMap>

class StatusManager : public QObject {
    Q_OBJECT

public:
    explicit StatusManager(QObject* parent = nullptr);

    void        setLocalUid(const QString& uid);
    void        updateMember(const MemberStatus& member);
    void        removeMember(const QString& uid);
    void        clearAll();
    void        cycleLocalStatus();
    void        setLocalStatus(ReadyStatus status);
    void        resetLocalStatus();
    ReadyStatus localStatus() const { return m_localStatus; }

    QList<MemberStatus> members() const;

Q_SIGNALS:
    void membersChanged();
    void localStatusChanged(ReadyStatus newStatus);

private:
    QString                     m_localUid;
    ReadyStatus                 m_localStatus = ReadyStatus::NotReady;
    bool                        m_active      = false;  // true after first hotkey press
    QMap<QString, MemberStatus> m_members;   // keyed by uid
};
