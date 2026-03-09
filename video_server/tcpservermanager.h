/******************************************************************
* @projectName   video_server
* @brief         tcpservermanager.h
* @author        Lu Yaolong
*******************************************************************/
#ifndef TCPSERVERMANAGER_H
#define TCPSERVERMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QDebug>

class TcpServerManager : public QObject
{
    Q_OBJECT

public:
    explicit TcpServerManager(QObject *parent = nullptr);
    ~TcpServerManager();
    
    bool startServer(quint16 port);
    void stopServer();
    void broadcastMessage(const QByteArray& data);
    void sendMessageToAll(const QString& message);
    int getConnectedClientCount() const;
    bool isServerRunning() const;
    QList<QTcpSocket*> getClientSockets() const { return m_clientSockets; }

signals:
    void clientConnected(QTcpSocket* socket);
    void clientDisconnected(QTcpSocket* socket);
    void messageReceived(QTcpSocket* socket, const QString& message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    QTcpServer* m_tcpServer;
    QList<QTcpSocket*> m_clientSockets;
};

#endif // TCPSERVERMANAGER_H
