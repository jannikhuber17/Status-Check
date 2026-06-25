#include "status_manager.h"

StatusManager::StatusManager(QObject* parent) : QObject(parent) {}

void StatusManager::setLocalUid(const QString& uid) {
    m_localUid = uid;
}

void StatusManager::updateMember(const MemberStatus& member) {
    if (member.uid.isEmpty()) return;
    m_members[member.uid] = member;
    Q_EMIT membersChanged();
}

void StatusManager::removeMember(const QString& uid) {
    if (m_members.remove(uid) > 0)
        Q_EMIT membersChanged();
}

void StatusManager::clearAll() {
    m_members.clear();
    Q_EMIT membersChanged();
}

void StatusManager::resetLocalStatus() {
    m_localStatus = ReadyStatus::NotReady;
    m_active      = false;
    // No signal emitted — intentionally silent, no Supabase publish.
}

void StatusManager::setLocalStatus(ReadyStatus status) {
    m_active      = true;
    m_localStatus = status;
    Q_EMIT localStatusChanged(m_localStatus);
}

void StatusManager::cycleLocalStatus() {
    if (!m_active) {
        // First press: activate and publish as NotReady (join the list visibly)
        m_active      = true;
        m_localStatus = ReadyStatus::NotReady;
    } else {
        // Subsequent presses: NotReady → Standby → Ready → NotReady → ...
        switch (m_localStatus) {
            case ReadyStatus::NotReady: m_localStatus = ReadyStatus::Standby;  break;
            case ReadyStatus::Standby:  m_localStatus = ReadyStatus::Ready;    break;
            case ReadyStatus::Ready:    m_localStatus = ReadyStatus::NotReady; break;
        }
    }
    Q_EMIT localStatusChanged(m_localStatus);
}

QList<MemberStatus> StatusManager::members() const {
    return m_members.values();
}
