/******************************************************************
* @projectName   video_server
* @brief         tcpservermanager.cpp - TCP 服务器管理器实现
* @description   本文件实现了 TCP 服务器管理模块，负责处理客户端连接和消息收发
* @features      1. TCP 服务器监听（QTcpServer）
*                2. 多客户端并发连接管理
*                3. 消息广播功能（发送到所有客户端）
*                4. 连接状态信号（连接/断开）
*                5. 消息接收信号（转发给 MainWindow 处理）
*                6. 自动清理断开的客户端
* @protocol     TCP 长连接，端口 8878
* @thread_safe  是（使用 Qt 的信号槽机制）
* @author        Lu Yaolong
*******************************************************************/
#include "tcpservermanager.h"

/**
 * @brief 构造函数
 * @param parent 父对象指针
 * @description 初始化 TCP 服务器管理器，设置成员变量为 nullptr
 */
TcpServerManager::TcpServerManager(QObject *parent)
    : QObject(parent)
    , m_tcpServer(nullptr)
{
}

/**
 * @brief 析构函数
 * @description 调用 stopServer() 清理资源
 */
TcpServerManager::~TcpServerManager()
{
    stopServer();
}

/**
 * @brief 启动 TCP 服务器
 * @param port 监听端口号
 * @return bool 成功返回 true，失败返回 false
 * @description 启动 TCP 服务器监听：
 *              1. 检查服务器是否已在运行
 *              2. 创建 QTcpServer 对象
 *              3. 调用 listen() 开始监听（QHostAddress::Any）
 *              4. 连接 newConnection 信号到 onNewConnection 槽
 *              5. 返回成功状态
 * @note 监听地址：0.0.0.0（接受所有网卡连接）
 */
bool TcpServerManager::startServer(quint16 port)
{
    if (m_tcpServer) {
        qWarning() << "服务器已在运行中";
        return false;
    }
    
    m_tcpServer = new QTcpServer(this);
    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        qWarning() << "TCP 服务器启动失败：" << m_tcpServer->errorString();
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }
    
    qDebug() << "TCP 服务器启动成功，监听端口" << port;
    
    connect(m_tcpServer, &QTcpServer::newConnection, 
            this, &TcpServerManager::onNewConnection);
    
    return true;
}

/**
 * @brief 停止 TCP 服务器
 * @description 关闭服务器并清理所有客户端连接：
 *              1. 检查服务器是否运行
 *              2. 关闭服务器（停止监听）
 *              3. 遍历所有客户端 Socket：
 *                 - 断开连接
 *                 - 调用 deleteLater() 延迟删除
 *              4. 清空客户端列表
 *              5. 删除服务器对象
 * @note 使用 deleteLater() 避免内存泄漏
 */
void TcpServerManager::stopServer()
{
    if (!m_tcpServer) {
        return;
    }
    
    m_tcpServer->close();
    
    for (QTcpSocket* socket : m_clientSockets) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    m_clientSockets.clear();
    
    delete m_tcpServer;
    m_tcpServer = nullptr;
    
    qDebug() << "TCP 服务器已停止";
}

/**
 * @brief 广播消息到所有客户端
 * @param data 要发送的二进制数据
 * @description 向所有已连接的客户端发送数据：
 *              1. 遍历客户端 Socket 列表
 *              2. 检查 Socket 状态（ConnectedState）
 *              3. 调用 write() 发送数据
 * @note 不检查发送结果，失败时静默忽略
 */
void TcpServerManager::broadcastMessage(const QByteArray& data)
{
    for (QTcpSocket* socket : m_clientSockets) {
        if (socket->state() == QTcpSocket::ConnectedState) {
            socket->write(data);
        }
    }
}

/**
 * @brief 发送字符串消息到所有客户端
 * @param message 要发送的字符串消息
 * @description 将字符串转为 UTF-8 后广播
 * @note 内部调用 broadcastMessage()
 */
void TcpServerManager::sendMessageToAll(const QString& message)
{
    broadcastMessage(message.toUtf8());
}

/**
 * @brief 获取已连接的客户端数量
 * @return int 客户端数量
 * @description 返回当前已连接的客户端总数
 */
int TcpServerManager::getConnectedClientCount() const
{
    return m_clientSockets.count();
}

/**
 * @brief 检查服务器是否正在运行
 * @return bool 运行中返回 true，否则返回 false
 * @description 检查 TCP 服务器是否正在监听
 */
bool TcpServerManager::isServerRunning() const
{
    return m_tcpServer && m_tcpServer->isListening();
}

/**
 * @brief 新客户端连接槽函数
 * @description 处理新客户端连接请求：
 *              1. 检查服务器有效性
 *              2. 调用 nextPendingConnection() 获取客户端 Socket
 *              3. 打印客户端 IP 地址
 *              4. 将 Socket 添加到客户端列表
 *              5. 连接信号：
 *                 - readyRead → onReadyRead（接收数据）
 *                 - disconnected → onClientDisconnected（断开连接）
 *              6. 发射 clientConnected 信号
 * @note 每个客户端都有独立的 QTcpSocket
 */
void TcpServerManager::onNewConnection()
{
    if (!m_tcpServer) {
        return;
    }
    
    QTcpSocket* clientSocket = m_tcpServer->nextPendingConnection();
    qDebug() << "新客户端连接：" << clientSocket->peerAddress().toString();
    
    m_clientSockets.append(clientSocket);
    
    connect(clientSocket, &QTcpSocket::readyRead, 
            this, &TcpServerManager::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, 
            this, &TcpServerManager::onClientDisconnected);
    
    emit clientConnected(clientSocket);
}

/**
 * @brief 客户端数据就绪槽函数
 * @description 读取客户端发送的数据并转发：
 *              1. 使用 sender() 获取发送者 Socket
 *              2. 调用 readAll() 读取所有数据
 *              3. 转为 QString（UTF-8 解码）
 *              4. 打印接收到的命令
 *              5. 发射 messageReceived 信号（转发给 MainWindow）
 * @note 数据格式由应用层协议定义
 */
void TcpServerManager::onReadyRead()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        return;
    }
    
    QByteArray data = clientSocket->readAll();
    QString recvStr = QString::fromUtf8(data);
    qDebug() << "收到客户端命令：" << recvStr;
    
    emit messageReceived(clientSocket, recvStr);
}

/**
 * @brief 客户端断开连接槽函数
 * @description 处理客户端断开连接：
 *              1. 使用 sender() 获取发送者 Socket
 *              2. 打印客户端 IP 地址
 *              3. 从客户端列表中移除
 *              4. 调用 deleteLater() 延迟删除
 *              5. 发射 clientDisconnected 信号
 * @note 客户端可能异常断开（网络故障）
 */
void TcpServerManager::onClientDisconnected()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        return;
    }
    
    qDebug() << "客户端断开连接：" << clientSocket->peerAddress().toString();
    
    m_clientSockets.removeOne(clientSocket);
    clientSocket->deleteLater();
    
    emit clientDisconnected(clientSocket);
}
