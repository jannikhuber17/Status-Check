#pragma once
#include <QString>
#include <QColor>
#include <cstdint>

enum class ReadyStatus {
    NotReady,
    Standby,
    Ready
};

struct MemberStatus {
    uint64_t clientId     = 0;
    QString  nickname;
    QString  uid;
    uint64_t ts3ChannelId = 0;
    ReadyStatus status = ReadyStatus::NotReady;

    QColor statusColor() const {
        switch (status) {
            case ReadyStatus::Ready:   return QColor(0x4C, 0xAF, 0x50); // green
            case ReadyStatus::Standby: return QColor(0xFF, 0xC1, 0x07); // yellow
            case ReadyStatus::NotReady:
            default:                   return QColor(0xF4, 0x43, 0x36); // red
        }
    }

    QString statusLabel() const {
        switch (status) {
            case ReadyStatus::Ready:    return "READY";
            case ReadyStatus::Standby:  return "STANDBY";
            case ReadyStatus::NotReady:
            default:                    return "NOT READY";
        }
    }
};
