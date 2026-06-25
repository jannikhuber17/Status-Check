#pragma once
#include "member_status.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QtWebSockets/QWebSocket>

class SupabaseClient : public QObject {
    Q_OBJECT

public:
    explicit SupabaseClient(QObject* parent = nullptr);

    void configure(const QString& url, const QString& anonKey);

    // Opens WebSocket and subscribes to Postgres Changes on ready_check table
    void connectRealtime(const QString& channelName);

    // REST UPSERT: persists own status; Realtime broadcasts the change to all subscribers
    void publishStatus(const QString& uid, const QString& nickname, ReadyStatus status, uint64_t ts3ChannelId);

    // REST GET: fetches current snapshot of all rows (call on channel join)
    void fetchAllMembers();

    // REST DELETE: removes own row from Supabase on shutdown
    void removeEntry(const QString& uid);

    void disconnect();

Q_SIGNALS:
    void memberUpdated(const MemberStatus& member);
    void memberRemoved(const QString& uid);
    void connectionStateChanged(bool connected);

private Q_SLOTS:
    void onWsConnected();
    void onWsDisconnected();
    void onWsTextMessage(const QString& message);
    void onWsError(QAbstractSocket::SocketError error);

private:
    void sendSubscribeMessage();
    void sendHeartbeat();
    void handlePostgresChange(const QJsonObject& data);
    QNetworkRequest buildRestRequest(const QString& path) const;

    QNetworkAccessManager m_nam;
    QWebSocket            m_ws;
    QString               m_url;
    QString               m_anonKey;
    QString               m_channelName;
    bool                  m_connected = false;
};
