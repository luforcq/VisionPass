#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QLabel>
#include <QTimer>

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    void initTCP(const QString& serverIp, quint16 port);
    void initUDP(quint16 port);
    void sendCommand(const QByteArray& cmd);
    void sendUnlockCommand();
    void sendNotifyMessage(const QString& msg, int duration);
    void connectToServer();

    QTcpSocket* tcpSocket() const { return m_tcpSocket; }
    QUdpSocket* udpSocket() const { return m_udpSocket; }
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError error);
    void dataReceived(const QByteArray& data);
    void videoFrameReceived(const QByteArray& datagram);

public slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onTcpDataReceived();
    void onUdpDataReceived();

private:
    QTcpSocket *m_tcpSocket;
    QUdpSocket *m_udpSocket;
    QTimer *m_reconnectTimer;
    QString m_serverIp;
    quint16 m_tcpPort;
    quint16 m_udpPort;
};

#endif // NETWORKMANAGER_H
