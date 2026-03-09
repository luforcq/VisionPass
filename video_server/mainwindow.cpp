/******************************************************************
* @projectName   video_server
* @brief         mainwindow.cpp - 嵌入式端主窗口实现
* @description   本文件实现了智能门禁系统嵌入式端的主窗口，负责：
*                1. 硬件设备管理（舵机、RFID、EEPROM、MPU6050）
*                2. 网络通信（TCP 服务器、客户端命令处理）
*                3. 视频采集与推流
*                4. ADC 传感器数据处理（门锁状态检测）
*                5. 用户交互（门铃、密码开锁、通知显示）
* @hardware      I.MX6U ARM Cortex-A7, RC522 RFID, 舵机，AT24C64, MPU6050
* @protocol     TCP 控制命令 + UDP 视频流
* @author        Lu Yaolong
*******************************************************************/
#include "mainwindow.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <QDebug>
#include <QFile>
#include "rc522.h"
#include <QMetaType>
#include "aes.c"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

/**
 * @brief 构造函数 - 系统初始化入口
 * @param parent 父窗口指针
 * @description 初始化嵌入式端主窗口，执行以下步骤：
 *              1. 创建中央控件和布局（垂直 + 水平布局）
 *              2. 初始化 UI 组件（initWidgets）
 *              3. 设置布局（initLayout）
 *              4. 连接信号槽（initConnections）
 *              5. 设置窗口几何属性（800x480）和样式（渐变背景）
 *              6. 初始化硬件管理器
 *              7. 启动 TCP 服务器（端口 8878）
 *              8. 启动 ADC 采集线程
 *              9. 初始化 MPU6050 振动检测线程
 *              10. 读取 EEPROM 中的加密密钥和密码
 *              11. 如果首次使用，设置默认密码"123456"
 * @note 使用 AES-128 加密存储密码（SHA256 哈希 + 盐）
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tcpManager(nullptr)
    , m_hardwareManager(nullptr)
    , m_notificationService(nullptr)
    , m_captureThread(nullptr)
    , m_adccaptureThread(nullptr)
    , m_pwdDialog(nullptr)
    , m_mpu6050Thread(nullptr)
    , m_eep24c64(nullptr)
    , m_isUnlockedState(false)
    , m_allowControl(false)
    , m_allowControl1(false)
    , m_isStaticState(true)
    , m_isAllowControlReset(false)
    , m_channel1Detected(false)
{
    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);

    QVBoxLayout *rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(15);

    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout *rightWidgetLayout = new QVBoxLayout();
    rightWidgetLayout->setSpacing(20);
    rightWidgetLayout->setAlignment(Qt::AlignCenter);

    initWidgets();
    initLayout();
    initConnections();

    this->setGeometry(0, 0, 800, 480);
    centralWidget->setStyleSheet(
        "QWidget {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "                stop:0 #2c3e50, stop:1 #1a252f);"
        "}"
        "QLabel {"
        "    color: #ecf0f1;"
        "}"
    );

    if (m_hardwareManager->initialize()) {
        qDebug() << "硬件管理器初始化成功";
    } else {
        qWarning() << "硬件管理器初始化失败";
    }

    if (m_tcpManager->startServer(8878)) {
        qDebug() << "TCP 服务器启动成功";
    }

    m_adccaptureThread->setThreadStart(1);

    m_mpu6050Thread = new Mpu6050Thread(this);
    m_mpu6050Thread->setVibrationThreshold(12.5f, 500.0f);
    connect(m_mpu6050Thread, &Mpu6050Thread::vibrationAlarm, 
            this, &MainWindow::handleVibrationAlarm);
    m_mpu6050Thread->setDoorLockedState(true);
    m_mpu6050Thread->setThreadStart(true);

    m_eep24c64 = m_hardwareManager->getEEPROM();
    if (m_eep24c64) {
        m_eep24c64->readKey(&m_key);
        
        EEP_Pwd storedPwd;
        if (m_eep24c64->readPwd(&storedPwd)) {
            static const uint8_t emptyHash[32] = {0};
            if (memcmp(storedPwd.pwd_hash, emptyHash, 32) == 0) {
                QString defaultPwd = "123456";
                EEP_Pwd defaultEncPwd;
                hashPwd(defaultPwd, &defaultEncPwd);
                m_eep24c64->writePwd(&defaultEncPwd);
                qDebug() << "首次使用，设置默认密码：" << defaultPwd;
            } else {
                qDebug() << "读取到存储的开锁密码";
            }
        }
    }
}

/**
 * @brief 析构函数
 * @description 清理资源（Qt 对象树自动管理大部分子对象）
 */
MainWindow::~MainWindow()
{
}

/**
 * @brief 初始化 UI 布局
 * @description 设置主窗口的控件布局：
 *              1. 获取中央控件和根布局
 *              2. 创建顶部布局（连接状态标签 + 状态指示灯）
 *              3. 创建内容布局（视频标签 + 右侧按钮区）
 *              4. 右侧按钮区包含：门铃按钮、密码开锁按钮
 *              5. 视频标签占据主要区域（Expanding 策略）
 * @note 使用嵌套布局（QVBoxLayout + QHBoxLayout）
 */
void MainWindow::initLayout()
{
    QWidget *centralWidget = this->centralWidget();
    if (!centralWidget) return;
    
    QVBoxLayout *rootLayout = qobject_cast<QVBoxLayout*>(centralWidget->layout());
    if (!rootLayout) return;

    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(15);

    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout *rightWidgetLayout = new QVBoxLayout();
    rightWidgetLayout->setSpacing(20);
    rightWidgetLayout->setAlignment(Qt::AlignCenter);

    topLayout->addWidget(m_connStatusLabel);
    topLayout->addStretch();
    topLayout->addWidget(m_statusIndicator);

    rightWidgetLayout->addStretch();
    rightWidgetLayout->addWidget(m_doorbellButton);
    rightWidgetLayout->addSpacing(30);
    rightWidgetLayout->addWidget(m_passwordUnlockBtn);
    rightWidgetLayout->addStretch();

    contentLayout->addWidget(m_videoLabel, 1);
    contentLayout->addLayout(rightWidgetLayout);

    rootLayout->addLayout(topLayout);
    rootLayout->addLayout(contentLayout, 1);
}

void MainWindow::initWidgets()
{
    m_statusIndicator = new QLabel(this);
    m_statusIndicator->setFixedSize(24, 24);
    m_statusIndicator->setStyleSheet(
        "QLabel {"
        "    background-color: #e74c3c;"
        "    border-radius: 12px;"
        "    border: 2px solid rgba(255, 255, 255, 0.3);"
        "}"
    );
    m_statusIndicator->setToolTip("系统状态指示灯");

    m_connStatusLabel = new QLabel(this);
    m_connStatusLabel->setFixedSize(240, 32);
    m_connStatusLabel->setStyleSheet(
        "QLabel {"
        "    color: #ecf0f1;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    background-color: rgba(52, 73, 94, 0.8);"
        "    padding: 5px 12px;"
        "    border-radius: 6px;"
        "    border: 1px solid rgba(255, 255, 255, 0.1);"
        "}"
    );

    m_videoLabel = new QLabel(this);
    m_videoLabel->setText("未获取到图像数据");
    m_videoLabel->setStyleSheet(
        "QLabel {"
        "    color: #bdc3c7;"
        "    background-color: rgba(0, 0, 0, 0.3);"
        "    border: 2px solid #34495e;"
        "    border-radius: 8px;"
        "    font-size: 16px;"
        "}"
    );
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumSize(480, 360);
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_doorbellButton = new QPushButton("门铃", this);
    m_doorbellButton->setFixedSize(130, 55);
    m_doorbellButton->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f39c12, stop:1 #e67e22);"
        "    border-radius: 12px;"
        "    color: white;"
        "    font-size: 18px;"
        "    font-weight: bold;"
        "    border: none;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e67e22, stop:1 #d35400);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d35400, stop:1 #e67e22);"
        "}"
    );

    m_passwordUnlockBtn = new QPushButton("密码开锁", this);
    m_passwordUnlockBtn->setFixedSize(130, 55);
    m_passwordUnlockBtn->setStyleSheet(
        "QPushButton {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #27ae60, stop:1 #229954);"
        "    border-radius: 12px;"
        "    color: white;"
        "    font-size: 18px;"
        "    font-weight: bold;"
        "    border: none;"
        "}"
        "QPushButton:hover {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #229954, stop:1 #1e8449);"
        "}"
        "QPushButton:pressed {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e8449, stop:1 #27ae60);"
        "}"
    );

    m_notifyMsgLabel = new QLabel(this);
    m_notifyMsgLabel->setFixedSize(480, 100);
    m_notifyMsgLabel->move(80, 190);
    m_notifyMsgLabel->setAlignment(Qt::AlignCenter);
    m_notifyMsgLabel->setStyleSheet(R"(
        QLabel{
            color: #FF0000;
            font-size: 24px;
            font-weight: bold;
            background-color: rgba(255,255,255,0.9);
            border-radius: 15px;
            border:2px solid red;
        }
    )");
    m_notifyMsgLabel->hide();

    m_notifyCloseBtn = new QPushButton("关闭提示", this);
    m_notifyCloseBtn->setFixedSize(120, 50);
    m_notifyCloseBtn->move(340, 300);
    m_notifyCloseBtn->setStyleSheet(R"(
        QPushButton{background-color: #FF5722;color:white;font-size:18px;border-radius:10px;}
        QPushButton:hover{background-color: #E64A19;}
    )");
    m_notifyCloseBtn->hide();

    m_notifyMsgTimer = new QTimer(this);
    m_notifyMsgTimer->setSingleShot(true);

    m_pwdDialog = new PasswordDialog(this);

    m_captureThread = new CaptureThread(this);
    m_adccaptureThread = new adccapture_thread(this);
}

void MainWindow::initConnections()
{
    connect(m_captureThread, SIGNAL(imageReady(QImage)), this, SLOT(showImage(QImage)));
    connect(m_adccaptureThread, SIGNAL(adcReady(int)), this, SLOT(processAdcValue(int)));
    connect(m_adccaptureThread, SIGNAL(adcChannel1Changed(int)), this, SLOT(handleChannel1(int)));
    connect(m_adccaptureThread, SIGNAL(adcChannel2Changed(int)), this, SLOT(handleChannel2(int)));
    connect(m_adccaptureThread, SIGNAL(adcChannel3Changed(int)), this, SLOT(handleChannel3(int)));

    connect(m_doorbellButton, &QPushButton::clicked, this, &MainWindow::onDoorbellClicked);
    connect(m_passwordUnlockBtn, &QPushButton::clicked, this, &MainWindow::onPasswordUnlockClicked);
    connect(m_pwdDialog, &PasswordDialog::passwordConfirmed, this, &MainWindow::onPasswordConfirmed);

    connect(m_notifyCloseBtn, &QPushButton::clicked, m_notificationService, &NotificationService::onCloseButtonClicked);
    connect(m_notifyMsgTimer, &QTimer::timeout, m_notificationService, &NotificationService::onTimeout);

    connect(m_tcpManager, &TcpServerManager::messageReceived, 
            this, &MainWindow::onTcpMessageReceived);
    connect(m_tcpManager, &TcpServerManager::clientConnected, 
            this, &MainWindow::onClientConnected);
    connect(m_tcpManager, &TcpServerManager::clientDisconnected, 
            this, &MainWindow::onClientDisconnected);

    connect(m_hardwareManager, &HardwareManager::cardDetected, 
            this, &MainWindow::onCardDetected);
    connect(m_hardwareManager, &HardwareManager::unauthorizedCard, 
            this, &MainWindow::onUnauthorizedCard);
    connect(m_hardwareManager, &HardwareManager::writeCardSuccess, 
            this, &MainWindow::onWriteCardSuccess);
    connect(m_hardwareManager, &HardwareManager::writeCardFailed, 
            this, &MainWindow::onWriteCardFailed);
    connect(m_hardwareManager, &HardwareManager::doorUnlocked, 
            this, &MainWindow::onDoorUnlocked);
    connect(m_hardwareManager, &HardwareManager::doorLocked, 
            this, &MainWindow::onDoorLocked);

    m_notificationService->initialize(m_notifyMsgLabel, m_notifyCloseBtn, m_notifyMsgTimer);
}

void MainWindow::setStatusIndicator(const QString &color)
{
    QString style = QString(
        "QLabel {"
        "    background-color: %1;"
        "    border-radius: 12px;"
        "    border: 2px solid rgba(255, 255, 255, 0.3);"
        "}"
    ).arg(color);
    m_statusIndicator->setStyleSheet(style);
}

void MainWindow::updateConnStatus()
{
    QString serverState = m_tcpManager->isServerRunning() ? "监听中" : "未监听";
    int connCount = m_tcpManager->getConnectedClientCount();
    QString statusText = QString("TCP 状态：%1 | 连接数：%2").arg(serverState).arg(connCount);

    QString color, borderColor;
    if (m_tcpManager->isServerRunning()) {
        color = (connCount > 0) ? "#2ecc71" : "#27ae60";
        borderColor = "#27ae60";
    } else {
        color = "#e74c3c";
        borderColor = "#c0392b";
    }

    m_connStatusLabel->setStyleSheet(QString(
        "QLabel {"
        "    color: %1;"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    background-color: rgba(52, 73, 94, 0.8);"
        "    padding: 5px 12px;"
        "    border-radius: 6px;"
        "    border: 1px solid %2;"
        "}"
    ).arg(color).arg(borderColor));

    m_connStatusLabel->setText(statusText);
    qDebug() << "连接状态更新：" << statusText;
}

void MainWindow::hashPwd(const QString &plainPwd, EEP_Pwd *hashedPwd) {
    memset(hashedPwd, 0, sizeof(EEP_Pwd));
    static const uint8_t PWD_SALT[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    QByteArray pwdWithSalt = plainPwd.toUtf8() + QByteArray((char*)PWD_SALT, sizeof(PWD_SALT));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(pwdWithSalt);
    QByteArray hashResult = hash.result();
    memcpy(hashedPwd->pwd_hash, hashResult.data(), 32);
}

void MainWindow::handleClientCommand(QTcpSocket* socket, const QString& command)
{
    qDebug() << "处理客户端命令：" << command;
    
    if (command == "unlock") {
        handleUnlockCommand(socket);
    }
    else if (command.startsWith("WriteCard:")) {
        handleWriteCardCommand(socket, command);
    }
    else if (command.startsWith("SyncUsers:")) {
        handleSyncUsersCommand(socket, command);
    }
    else if (command.startsWith("notify_msg:")) {
        handleNotifyCommand(command);
    }
    else if (command.startsWith("set_pwd:")) {
        handleSetPwdCommand(socket, command.toUtf8());
    }
    else if (command == "get_pwd_status") {
    }
    else {
        handleVideoCommand(socket, command);
    }
}

void MainWindow::handleUnlockCommand(QTcpSocket* socket)
{
    qDebug() << "执行开锁操作！";
    m_hardwareManager->unlockDoor();
    socket->write("unlock_ok");
}

void MainWindow::handleWriteCardCommand(QTcpSocket* socket, const QString& command)
{
    QStringList msgList = command.split(":");
    if(msgList.size() == 3) {
        QString cardId = msgList[1];
        QString name = msgList[2];
        
        m_hardwareManager->prepareWriteCard(cardId, name);
        
        socket->write("WriteCard_ok");
        m_notificationService->showMessage(QString("请放置卡片，正在写入：%1").arg(name), 5000);
    }
}

void MainWindow::handleSyncUsersCommand(QTcpSocket* socket, const QString& command)
{
    QStringList parts = command.split(":");
    if(parts.size() == 2) {
        QString userListStr = parts[1];
        QStringList userInfoList = userListStr.split(",", QString::SkipEmptyParts);

        QMap<uint32_t, QString> clientUsers;
        for(const QString& userInfo : userInfoList) {
            QStringList userParts = userInfo.split("|");
            if(userParts.size() == 2) {
                bool ok;
                uint32_t id = userParts[0].toUInt(&ok);
                if(ok) {
                    clientUsers[id] = userParts[1];
                }
            }
        }

        qDebug() << "收到用户同步请求，客户端用户数：" << clientUsers.size();

        AT24C64* eeprom = m_hardwareManager->getEEPROM();
        if (!eeprom) return;

        uint8_t serverUserCount = eeprom->getUserCount();
        QMap<uint32_t, int> serverUsers;
        for(int i = 0; i < serverUserCount; i++) {
            EEP_User user;
            if(eeprom->readUser(i, &user)) {
                serverUsers[user.id] = i;
            }
        }

        int addedCount = 0;
        for(auto it = clientUsers.begin(); it != clientUsers.end(); ++it) {
            if(!serverUsers.contains(it.key())) {
                EEP_User newUser;
                memset(&newUser, 0, sizeof(EEP_User));
                QByteArray nameBytes = it.value().toUtf8();
                memcpy(newUser.name, nameBytes.constData(), qMin(nameBytes.size(), 15));
                newUser.id = it.key();

                if(eeprom->addUser(&newUser)) {
                    addedCount++;
                }
            }
        }

        int deletedCount = 0;
        serverUserCount = eeprom->getUserCount();
        for(int i = serverUserCount - 1; i >= 0; i--) {
            EEP_User user;
            if(eeprom->readUser(i, &user)) {
                if(!clientUsers.contains(user.id)) {
                    if(eeprom->deleteUser(i)) {
                        deletedCount++;
                    }
                }
            }
        }

        uint8_t finalCount = eeprom->getUserCount();
        QString response = QString("SyncUsers_ok:同步完成，添加%1个，删除%2个，剩余%3个用户")
            .arg(addedCount).arg(deletedCount).arg(finalCount);
        socket->write(response.toUtf8());

        qDebug() << "用户同步完成，添加" << addedCount << "个，删除" << deletedCount
                 << "个，剩余" << finalCount << "个用户";
    }
}

void MainWindow::handleVideoCommand(QTcpSocket* socket, const QString& command)
{
    if (command == "StartVideo") {
        qDebug() << "执行开启视频推流操作！";
        m_captureThread->setBroadcast(1);
        m_captureThread->setThreadStart(1);
        socket->write("StartVideo_ok");
    }
    else if(command == "CloseVideo"){
        qDebug() << "执行关闭视频推流操作！";
        m_captureThread->setBroadcast(0);
        if(!m_channel1Detected)
            m_captureThread->setThreadStart(0);
        socket->write("CloseVideo_ok");
    }
    else {
        socket->write("error:未知指令");
    }
}

void MainWindow::handleNotifyCommand(const QString& command)
{
    QStringList msgList = command.split(":");
    if(msgList.size() == 3) {
        QString showMsg = msgList[1];
        int showTime = msgList[2].toInt();
        
        m_notificationService->showMessage(showMsg, showTime * 1000);
        qDebug() << "收到通知消息：" << showMsg << "，显示时长：" << showTime << "秒";
    }
}

void MainWindow::handleSetPwdCommand(QTcpSocket* socket, const QByteArray& data)
{
    uint8_t hou1[16] = {0};
    QByteArray jiamihou = data.mid(8, 16);
    uint8_t reve[16];
    memcpy(reve, jiamihou.constData(), 16);
    aes128_ecb_decrypt(m_key.key, reve, hou1);
    uint8_t len = pkcs7_unpad(hou1, 16);
    QString newPwd;
    for(int i = 0; i < len; i++) {
        newPwd.append(QChar(hou1[i]));
    }
    qDebug() << "newPwd:" << newPwd;
    
    EEP_Pwd encPwd;
    hashPwd(newPwd, &encPwd);
    if (m_eep24c64->writePwd(&encPwd)) {
        m_notificationService->showSuccess(QString("密码已修改为%1").arg(newPwd));
        socket->write(QString("success:密码已修改为%1").arg(newPwd).toUtf8());
    } else {
        socket->write("error:密码写入失败");
    }
}

void MainWindow::onDoorbellClicked()
{
    qDebug() << "触发门铃，向所有客户端发送提示消息";
    QByteArray doorbellMsg = "doorbell_notify:有人按门铃";
    m_tcpManager->broadcastMessage(doorbellMsg);
    setStatusIndicator("#f39c12");
    QTimer::singleShot(2000, this, [this]() {
        if (!m_hardwareManager->isDoorUnlocked()) {
            setStatusIndicator("#e74c3c");
        }
    });
}

void MainWindow::onPasswordUnlockClicked()
{
    qDebug() << "打开密码输入对话框";
    m_pwdDialog->exec();
}

void MainWindow::onPasswordConfirmed(const QString &password)
{
    qDebug() << "用户输入密码：" << password;

    EEP_Pwd inputEncPwd;
    hashPwd(password, &inputEncPwd);

    EEP_Pwd storedEncPwd;
    if (!m_eep24c64->readPwd(&storedEncPwd)) {
        m_notificationService->showError("读取密码失败！");
        return;
    }

    bool pwdMatch = (memcmp(inputEncPwd.pwd_hash, storedEncPwd.pwd_hash, 32) == 0);

    if (pwdMatch) {
        m_notificationService->showSuccess("密码正确，开锁成功！");
        m_hardwareManager->unlockDoor();
    } else {
        m_notificationService->showError("密码错误，开锁失败！");
    }
}

void MainWindow::onTcpMessageReceived(QTcpSocket* socket, const QString& message)
{
    handleClientCommand(socket, message);
}

void MainWindow::onClientConnected(QTcpSocket* socket)
{
    qDebug() << "客户端连接：" << socket->peerAddress().toString();
    updateConnStatus();
}

void MainWindow::onClientDisconnected(QTcpSocket* socket)
{
    qDebug() << "客户端断开连接：" << socket->peerAddress().toString();
    updateConnStatus();
}

void MainWindow::onCardDetected(const QString& cardId, const QString& userName)
{
    m_notificationService->showSuccess(QString("欢迎回家，%1！").arg(userName));
}

void MainWindow::onUnauthorizedCard(const QString& cardId)
{
    m_notificationService->showError("未授权的卡片！");
}

void MainWindow::onWriteCardSuccess(const QString& cardId, const QString& userName)
{
    m_notificationService->showSuccess(QString("%1 已绑定").arg(userName));
}

void MainWindow::onWriteCardFailed()
{
    m_notificationService->showError("写卡失败！");
}

void MainWindow::onDoorUnlocked()
{
    setStatusIndicator("#27ae60");
}

void MainWindow::onDoorLocked()
{
    if (!m_hardwareManager->isDoorUnlocked()) {
        setStatusIndicator("#e74c3c");
    }
}

void MainWindow::showImage(QImage image)
{
    m_videoLabel->setPixmap(QPixmap::fromImage(image));
}

void MainWindow::handleChannel1(int value)
{
    if (value > 20000) {
        if (!m_channel1Detected) {
            m_channel1Detected = true;
            m_captureThread->setThreadStart(true);
            m_captureThread->setLocalDisplay(true);
            qDebug() << "检测到有人靠近，已启动摄像头和本地显示";
            m_statusIndicator->setToolTip("设备状态：检测到有人");
        }
    } else {
        if (m_channel1Detected) {
            m_channel1Detected = false;

            if (m_captureThread->startBroadcast) {
                m_captureThread->setLocalDisplay(false);
                m_videoLabel->setText("未获取到图像数据");
                m_videoLabel->setStyleSheet("QLabel {color: white;}");
                qDebug() << "正在广播，仅关闭本地显示";
            } else {
                m_captureThread->setThreadStart(false);
                m_captureThread->setLocalDisplay(false);
                qDebug() << "检测到人已离开，已关闭摄像头采集和本地显示";
                m_videoLabel->setText("未获取到图像数据");
                m_videoLabel->setStyleSheet("QLabel {color: white;}");
            }

            if (!m_hardwareManager->isDoorUnlocked()) {
                setStatusIndicator("#e74c3c");
                m_statusIndicator->setToolTip("设备状态：锁住");
            }
        }
    }
}

void MainWindow::handleChannel2(int value)
{
    if (value > 20000) {
        m_allowControl1 = true;
        if(m_isUnlockedState && m_isAllowControlReset) {
            m_hardwareManager->lockDoor();
            m_isUnlockedState = false;
            qDebug() << "通道 2 检测到门已关上，ADC 值：" << value;
        }
        m_statusIndicator->setToolTip("设备状态：门已关闭，允许控制");
    } else {
        m_allowControl1 = false;
        m_statusIndicator->setToolTip("设备状态：门未关闭，禁止控制");
    }
}

void MainWindow::handleChannel3(int value)
{
}

void MainWindow::processAdcValue(int value)
{
    if (m_hardwareManager->isDoorUnlocked()) {
        return;
    }

    if (!m_allowControl) {
        if (value >= 0 && value <= 500) {
            m_allowControl = true;
        }
        return;
    }

    if (value >= 0 && value <= 500) {
        if(!m_isStaticState){
            m_isStaticState = true;
        }
        return;
    }
    else if (value > 500 && value <= 2500) {
        m_isStaticState = false;
        int angle = (int)(((value - 500.0) / (2500.0 - 1000.0)) * 90.0);
        angle = qBound(0, angle, 90);
        
        if (angle > 0) {
            setStatusIndicator("#f1c40f");
            m_statusIndicator->setToolTip("设备状态：部分开启");
        } else {
            setStatusIndicator("#e74c3c");
            m_statusIndicator->setToolTip("设备状态：锁住");
        }
    }
    else if (value > 2500) {
        m_hardwareManager->unlockDoor();
        setStatusIndicator("#27ae60");
        m_statusIndicator->setToolTip("设备状态：开锁");
    }
}

void MainWindow::handleVibrationAlarm(QString alarmMsg)
{
    qDebug() << "[振动告警]" << alarmMsg;
    m_notificationService->showMessage(alarmMsg, 3000);

    QByteArray vibrationMsg = QString("vibration_alarm:%1").arg(alarmMsg).toUtf8();
    m_tcpManager->broadcastMessage(vibrationMsg);

    setStatusIndicator("#9b59b6");
    QTimer::singleShot(5000, this, [this]() {
        if (!m_hardwareManager->isDoorUnlocked()) {
            setStatusIndicator("#e74c3c");
        }
    });
}

void MainWindow::showMpu6050Data(float ax, float ay, float az, float gx, float gy, float gz)
{
    qDebug() << QString("MPU6050 数据：ax=%1, ay=%2, az=%3, gx=%4, gy=%5, gz=%6")
                .arg(ax, 0, 'f', 2)
                .arg(ay, 0, 'f', 2)
                .arg(az, 0, 'f', 2)
                .arg(gx, 0, 'f', 2)
                .arg(gy, 0, 'f', 2)
                .arg(gz, 0, 'f', 2);
}
