/******************************************************************
* @projectName   video_client
* @brief         networkmanager.cpp - 网络通信管理器实现
* @author        Lu Yaolong
* 
* @description   本文件实现了客户端的网络通信管理模块，包括：
*                - TCP 连接管理（控制命令、数据传输）
*                - UDP 连接管理（视频流接收）
*                - 自动重连机制
*                - 错误处理和状态通知
* 
* @architecture  双 Socket 设计：
*                - TCP Socket: 用于可靠数据传输（命令、响应）
*                - UDP Socket: 用于视频流接收（低延迟）
*******************************************************************/
#include "networkmanager.h"
#include <QDebug>

/**
 * @brief NetworkManager 构造函数
 * 
 * @param parent 父对象指针
 * 
 * @description 初始化网络管理器的基本成员：
 *              - m_tcpSocket: TCP 套接字指针（初始为 nullptr）
 *              - m_udpSocket: UDP 套接字指针（初始为 nullptr）
 *              - m_reconnectTimer: 重连定时器指针（初始为 nullptr）
 *              - m_tcpPort: TCP 端口号（默认 8878）
 *              - m_udpPort: UDP 端口号（默认 8888）
 * 
 * @note 构造函数只进行成员初始化，不建立实际连接
 *       实际连接在 initTCP() 中进行
 */
NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
    , m_tcpSocket(nullptr)
    , m_udpSocket(nullptr)
    , m_reconnectTimer(nullptr)
    , m_tcpPort(8878)
    , m_udpPort(8888)
{
}

/**
 * @brief NetworkManager 析构函数
 * 
 * @description 清理网络资源：
 *              1. 停止重连定时器（如果正在运行）
 *              2. 断开 TCP 连接（如果已连接）
 * 
 * @note Qt 的对象树机制会自动删除 m_tcpSocket 和 m_udpSocket
 *       因为它们创建时指定了 this 作为父对象
 */
NetworkManager::~NetworkManager()
{
    if (m_reconnectTimer && m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
    
    if (m_tcpSocket && m_tcpSocket->state() == QTcpSocket::ConnectedState) {
        m_tcpSocket->disconnectFromHost();
    }
}

/**
 * @brief 初始化 TCP 连接
 * 
 * @param serverIp 服务器 IP 地址
 * @param port TCP 端口号
 * 
 * @description 建立可靠的 TCP 控制通道：
 *              1. 保存服务器配置信息
 *              2. 创建 QTcpSocket 对象
 *              3. 连接信号槽（4 个核心事件）：
 *                 - connected: 连接成功 -> onConnected()
 *                 - disconnected: 断开连接 -> onDisconnected()
 *                 - error: 发生错误 -> onErrorOccurred()
 *                 - readyRead: 数据到达 -> onTcpDataReceived()
 *              4. 创建重连定时器：
 *                 - 间隔：5000ms
 *                 - 超时处理：检查连接状态，尝试重连
 *              5. 发起首次连接
 * 
 * @note 重连机制：
 *       - 连接失败或断开时，定时器自动启动
 *       - 连接成功后，定时器停止
 *       - 每 5 秒检查一次连接状态
 *       
 * @note 使用 QOverload 解决 error 信号重载歧义
 */
void NetworkManager::initTCP(const QString& serverIp, quint16 port)
{
    m_serverIp = serverIp;
    m_tcpPort = port;
    
    m_tcpSocket = new QTcpSocket(this);
    
    connect(m_tcpSocket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &NetworkManager::onErrorOccurred);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &NetworkManager::onTcpDataReceived);
    
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(5000);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (!isConnected()) {
            connectToServer();
            qDebug() << "尝试重连 TCP 服务器...";
        }
    });
    
    connectToServer();
    qDebug() << "网络管理器初始化，尝试连接 TCP 服务器：" << m_serverIp << ":" << m_tcpPort;
}

/**
 * @brief 初始化 UDP 连接
 * 
 * @param port UDP 端口号（用于接收视频流）
 * 
 * @description 建立 UDP 视频接收通道：
 *              1. 保存 UDP 端口号
 *              2. 创建 QUdpSocket 对象
 *              3. 绑定到指定端口（监听所有网卡）
 *              4. 连接 readyRead 信号到数据接收槽
 * 
 * @note UDP 用于视频流传输的优势：
 *       - 无连接，低延迟
 *       - 适合实时视频流
 *       - 允许少量丢包（视频压缩有冗余）
 *       
 * @note 绑定到 QHostAddress::Any 监听所有网络接口
 */
void NetworkManager::initUDP(quint16 port)
{
    m_udpPort = port;
    m_udpSocket = new QUdpSocket(this);
    m_udpSocket->bind(QHostAddress::Any, m_udpPort);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkManager::onUdpDataReceived);
}

/**
 * @brief 发送命令到服务器
 * 
 * @param cmd 命令数据（QByteArray）
 * 
 * @description 通过 TCP 发送控制命令：
 *              1. 检查连接状态
 *                 - 未连接：打印警告并返回
 *              2. 调用 write() 发送数据
 * 
 * @note 常用命令：
 *       - "StartVideo": 开启视频推流
 *       - "CloseVideo": 关闭视频推流
 *       - "unlock": 远程开锁
 *       
 * @note 错误处理：
 *       由 MainWindow 通过 errorOccurred 信号统一处理
 */
void NetworkManager::sendCommand(const QByteArray& cmd)
{
    if (!isConnected()) {
        qWarning() << "未连接到服务器，无法发送命令";
        return;
    }
    m_tcpSocket->write(cmd);
}

/**
 * @brief 发送开锁命令
 * 
 * @description 封装开锁命令的发送：
 *              1. 构造命令："unlock"
 *              2. 通过 TCP 发送
 *              3. 打印调试日志
 * 
 * @note 开锁流程：
 *       客户端发送 -> 服务器控制舵机 -> 返回响应 -> 客户端处理
 */
void NetworkManager::sendUnlockCommand()
{
    QByteArray cmd = "unlock";
    qint64 sentBytes = m_tcpSocket->write(cmd);
    if (sentBytes > 0) {
        qDebug() << "开锁命令发送成功，字节数：" << sentBytes;
    } else {
        qDebug() << "开锁命令发送失败";
    }
}

/**
 * @brief 发送通知消息
 * 
 * @param msg 通知内容
 * @param duration 显示时长（秒）
 * 
 * @description 向服务器发送通知消息：
 *              1. 构造命令格式："notify_msg:消息：时长"
 *              2. 转换为 UTF-8 编码
 *              3. 通过 TCP 发送
 * 
 * @note 通知消息会在服务端显示屏上显示
 *       用于远程通知门口人员（如"快递已放门口"）
 */
void NetworkManager::sendNotifyMessage(const QString& msg, int duration)
{
    QString sendData = QString("notify_msg:%1:%2").arg(msg).arg(duration);
    QByteArray sendBytes = sendData.toUtf8();
    m_tcpSocket->write(sendBytes);
}

/**
 * @brief 连接到服务器
 * 
 * @description 发起 TCP 连接请求：
 *              调用 connectToHost() 异步连接服务器
 * 
 * @note 连接结果通过信号通知：
 *       - 成功：emit connected()
 *       - 失败：emit errorOccurred()
 */
void NetworkManager::connectToServer()
{
    m_tcpSocket->connectToHost(m_serverIp, m_tcpPort);
}

/**
 * @brief 检查连接状态
 * 
 * @return bool 是否已连接
 * 
 * @description 检查 TCP 连接状态：
 *              - 检查 socket 指针是否有效
 *              - 检查状态是否为 ConnectedState
 * 
 * @return true 已连接，false 未连接
 */
bool NetworkManager::isConnected() const
{
    return m_tcpSocket && m_tcpSocket->state() == QTcpSocket::ConnectedState;
}

/**
 * @brief TCP 连接成功槽函数
 * 
 * @description 处理连接成功事件：
 *              1. 打印调试日志
 *              2. 发射 connected 信号（通知 MainWindow）
 *              3. 停止重连定时器
 * 
 * @note 连接成功后，重连定时器停止工作
 *       直到下次断开连接时重新启动
 */
void NetworkManager::onConnected()
{
    qDebug() << "TCP 连接服务器成功！";
    emit connected();
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
}

/**
 * @brief TCP 断开连接槽函数
 * 
 * @description 处理断开连接事件：
 *              1. 打印调试日志
 *              2. 发射 disconnected 信号
 *              3. 启动重连定时器
 * 
 * @note 断开连接后，定时器每 5 秒尝试重连一次
 */
void NetworkManager::onDisconnected()
{
    qDebug() << "TCP 与服务器断开连接";
    emit disconnected();
    if (m_reconnectTimer) {
        m_reconnectTimer->start();
    }
}

/**
 * @brief TCP 错误处理槽函数
 * 
 * @param error QAbstractSocket::SocketError 错误类型
 * 
 * @description 处理网络错误事件：
 *              1. 忽略未使用的 error 参数（避免警告）
 *              2. 打印错误描述信息
 *              3. 发射 errorOccurred 信号
 *              4. 启动重连定时器
 * 
 * @note 常见错误类型：
 *       - ConnectionRefusedError: 连接被拒绝
 *       - HostNotFoundError: 主机未找到
 *       - SocketTimeoutError: 连接超时
 */
void NetworkManager::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qDebug() << "TCP 错误：" << m_tcpSocket->errorString();
    emit errorOccurred(error);
    if (m_reconnectTimer) {
        m_reconnectTimer->start();
    }
}

/**
 * @brief TCP 数据接收槽函数
 * 
 * @description 处理 TCP 数据到达事件：
 *              1. 读取所有可用数据（readAll）
 *              2. 打印调试日志
 *              3. 发射 dataReceived 信号（带数据）
 * 
 * @note readAll() 会清空接收缓冲区
 *       确保每次都能读取完整数据
 */
void NetworkManager::onTcpDataReceived()
{
    QByteArray recvData = m_tcpSocket->readAll();
    qDebug() << "收到服务端 TCP 消息：" << recvData;
    emit dataReceived(recvData);
}

/**
 * @brief UDP 数据接收槽函数
 * 
 * @description 处理 UDP 视频帧到达事件：
 *              1. 创建接收缓冲区
 *              2. 调整大小为待接收的数据包大小
 *              3. 读取数据报（readDatagram）
 *              4. 发射 videoFrameReceived 信号
 * 
 * @note UDP 数据报特点：
 *       - 可能乱序到达
 *       - 可能丢失
 *       - 每个数据报是一个完整的视频帧
 *       
 * @note pendingDatagramSize() 返回下一个数据报的大小
 */
void NetworkManager::onUdpDataReceived()
{
    QByteArray datagram;
    datagram.resize(m_udpSocket->pendingDatagramSize());
    m_udpSocket->readDatagram(datagram.data(), datagram.size());
    emit videoFrameReceived(datagram);
}
