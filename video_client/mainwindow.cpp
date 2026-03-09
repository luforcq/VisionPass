/******************************************************************
* @projectName   video_client
* @brief         mainwindow.cpp - 智能门禁系统客户端主窗口实现
* @author        Lu Yaolong
* 
* @description   本文件实现了客户端的主窗口逻辑，包括：
*                - 系统模块初始化（UI、网络、人脸识别）
*                - 信号槽连接管理
*                - 用户交互事件处理（开锁、视频、修改密码等）
*                - 网络通信事件处理
*                - 人脸识别结果处理
*                - 视频帧处理与显示
* 
* @architecture  MVC 模式：
*                - Model: FaceService(人脸识别服务)、NetworkManager(网络管理)
*                - View: UIManager(UI 管理)
*                - Controller: MainWindow(主窗口，协调各模块)
*******************************************************************/
#include "mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QDateTime>
#include <QResizeEvent>
#include <QMetaType>
#include "aes.c"

using namespace dlib;
using namespace cv;

/**
 * @brief MainWindow 构造函数
 * 
 * @param parent 父窗口指针，默认为 nullptr
 * 
 * @description 构造函数完成以下工作：
 *              1. 注册自定义元类型，用于跨线程信号槽通信
 *                 - FaceLandmarkData: 人脸关键点数据结构
 *                 - std::vector<FaceLandmarkData>: 人脸关键点数据列表
 *                 - RecognizeResult: 人脸识别结果结构
 *              2. 初始化系统模块（UI、网络、人脸识别）
 *              3. 建立模块间信号槽连接
 * 
 * @note Qt 元类型系统使得自定义类型可以在信号槽中传递
 *       必须在构造函数中注册，且在第一个信号发射之前
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_uiManager(nullptr)
    , m_networkManager(nullptr)
    , m_faceService(nullptr)
    , m_isVideoRunning(false)
{
    qRegisterMetaType<FaceLandmarkData>("FaceLandmarkData");
    qRegisterMetaType<std::vector<FaceLandmarkData>>("std::vector<FaceLandmarkData>");
    qRegisterMetaType<RecognizeResult>("RecognizeResult");
    
    initializeModules();
    setupConnections();
}

/**
 * @brief MainWindow 析构函数
 * 
 * @description 清理资源
 * @note Qt 的对象树机制会自动删除有父对象的子对象
 *       本类中的成员对象（m_uiManager、m_networkManager、m_faceService）
 *       在创建时指定了 this 为父对象，因此会自动释放
 */
MainWindow::~MainWindow()
{
}

/**
 * @brief 初始化系统模块
 * 
 * @description 按顺序初始化三个核心模块：
 *              1. UIManager: UI 管理器，负责界面布局和控件管理
 *                 - 设置主窗口
 *                 - 创建和布局所有 UI 控件
 *              
 *              2. NetworkManager: 网络管理器，负责网络通信
 *                 - 初始化 TCP 连接（用于控制命令和数据传输）
 *                 - 初始化 UDP 连接（用于视频流接收）
 *                 - 从配置文件读取服务器 IP 和端口
 *              
 *              3. FaceService: 人脸识别服务，负责人脸检测和识别
 *                 - 加载人脸关键点模型（shape_predictor_68_face_landmarks.dat）
 *                 - 加载人脸识别模型（dlib_face_recognition_resnet_model_v1.dat）
 *                 - 在独立线程中运行，避免阻塞 UI
 * 
 * @note 模块初始化顺序很重要：UI -> Network -> FaceService
 *       因为 FaceService 依赖于网络配置，UI 需要最先创建
 */
void MainWindow::initializeModules()
{
    m_uiManager = new UIManager(this);
    m_uiManager->setupMainWindow(this);
    
    m_networkManager = new NetworkManager(this);
    m_networkManager->initTCP(ConfigManager::instance().getServerIp(),
                             ConfigManager::instance().getTcpPort());
    m_networkManager->initUDP(8888);
    
    m_faceService = new FaceService(this);
    m_faceService->initialize(ConfigManager::instance().getFaceLandmarkModelPath(),
                             ConfigManager::instance().getFaceRecognitionModelPath());
}

/**
 * @brief 建立模块间信号槽连接
 * 
 * @description 使用 Lambda 表达式和现代 Qt 连接语法建立所有信号槽连接
 *              连接关系按功能分组：
 *              
 *              1. UI 按钮事件连接（5 个）
 *                 - 开锁按钮 -> onUnlockBtnClicked()
 *                 - 视频开关按钮 -> onStartVideoBtnClicked()
 *                 - 修改密码按钮 -> onChangePwdBtnClicked()
 *                 - 发送通知按钮 -> onSendNotifyBtnClicked()
 *                 - 人脸库按钮 -> onOpenFaceDbBtnClicked()
 *              
 *              2. 网络事件连接（5 个）
 *                 - 连接成功 -> onNetworkConnected()
 *                 - 断开连接 -> onNetworkDisconnected()
 *                 - 发生错误 -> onNetworkError()
 *                 - 收到数据 -> onTcpDataReceived()
 *                 - 收到视频帧 -> onVideoFrameReceived()
 *              
 *              3. 人脸识别事件连接（3 个）
 *                 - 识别成功 -> onFaceRecognized()
 *                 - 人脸数据处理完成 -> onFaceDataProcessed()
 *                 - 模型加载完成 -> onModelsLoaded()
 * 
 * @note 使用 Qt5 的新连接语法（&Class::signal）比字符串方式更安全
 *       编译时检查，避免拼写错误
 */
void MainWindow::setupConnections()
{
    connect(m_uiManager, &UIManager::unlockButtonClicked,
            this, &MainWindow::onUnlockBtnClicked);
    
    connect(m_uiManager, &UIManager::startVideoButtonClicked,
            this, &MainWindow::onStartVideoBtnClicked);
    
    connect(m_uiManager, &UIManager::changePwdButtonClicked,
            this, &MainWindow::onChangePwdBtnClicked);
    
    connect(m_uiManager, &UIManager::sendNotifyButtonClicked,
            this, &MainWindow::onSendNotifyBtnClicked);
    
    connect(m_uiManager, &UIManager::openFaceDbButtonClicked,
            this, &MainWindow::onOpenFaceDbBtnClicked);
    
    connect(m_networkManager, &NetworkManager::connected,
            this, &MainWindow::onNetworkConnected);
    
    connect(m_networkManager, &NetworkManager::disconnected,
            this, &MainWindow::onNetworkDisconnected);
    
    connect(m_networkManager, &NetworkManager::errorOccurred,
            this, &MainWindow::onNetworkError);
    
    connect(m_networkManager, &NetworkManager::dataReceived,
            this, &MainWindow::onTcpDataReceived);
    
    connect(m_networkManager, &NetworkManager::videoFrameReceived,
            this, &MainWindow::onVideoFrameReceived);
    
    connect(m_faceService, &FaceService::faceRecognizedSuccess,
            this, &MainWindow::onFaceRecognized);
    
    connect(m_faceService, &FaceService::faceDataProcessed,
            this, &MainWindow::onFaceDataProcessed);
    
    connect(m_faceService, &FaceService::modelsLoaded,
            this, &MainWindow::onModelsLoaded);
}

/**
 * @brief 开锁按钮点击事件处理
 * 
 * @description 处理用户点击开锁按钮的逻辑：
 *              1. 检查网络连接状态
 *                 - 未连接：显示警告提示，并尝试重连
 *                 - 已连接：继续执行开锁流程
 *              2. 禁用开锁按钮，防止重复点击
 *              3. 发送开锁命令到服务器
 * 
 * @note 开锁命令通过 TCP 发送，服务器响应后重新启用按钮
 *       使用 sendUnlockCommand() 发送实际命令，该函数包含超时处理
 */
void MainWindow::onUnlockBtnClicked()
{
    if (!m_networkManager->isConnected()) {
        QMessageBox::warning(this, "提示", "未连接到服务器，请等待重连完成！");
        m_networkManager->connectToServer();
        return;
    }
    
    m_uiManager->getUnlockBtn()->setEnabled(false);
    sendUnlockCommand();
}

/**
 * @brief 视频开关按钮点击事件处理
 * 
 * @param checked 按钮选中状态（true=开启，false=关闭）
 * 
 * @description 处理视频推流的开启和关闭：
 *              1. 检查网络连接状态
 *                 - 未连接：显示警告提示，并尝试重连
 *              2. 更新视频运行状态标志
 *              3. 根据 checked 状态执行：
 *                 - 开启：
 *                   a. 更改按钮文本为"关闭视频"
 *                   b. 发送"StartVideo"命令到服务器
 *                   c. 显示成功提示消息（绿色，4 秒）
 *                 - 关闭：
 *                   a. 更改按钮文本为"开启视频"
 *                   b. 发送"CloseVideo"命令到服务器
 *                   c. 显示关闭提示消息
 *                   d. 清空视频显示区域
 * 
 * @note 视频流通过 UDP 协议传输，控制命令通过 TCP 发送
 *       服务器收到 StartVideo 命令后启动摄像头采集和广播
 */
void MainWindow::onStartVideoBtnClicked(bool checked)
{
    if (!m_networkManager->isConnected()) {
        QMessageBox::warning(this, "提示", "未连接到服务器，请等待重连完成！");
        m_networkManager->connectToServer();
        return;
    }
    
    m_isVideoRunning = checked;
    
    if (checked) {
        m_uiManager->getStartVideoBtn()->setText("关闭视频");
        m_networkManager->sendCommand("StartVideo");
        m_uiManager->showTipMessage("开启视频推流成功！", 4000, QColor(0, 255, 0, 178), Qt::white);
    } else {
        m_uiManager->getStartVideoBtn()->setText("开启视频");
        m_networkManager->sendCommand("CloseVideo");
        m_uiManager->showTipMessage("视频推流已关闭！", 4000, QColor(0, 255, 0, 178), Qt::white);
        m_uiManager->getVideoLabel()->clear();
    }
}

/**
 * @brief 修改密码按钮点击事件处理
 * 
 * @description 处理用户修改管理员密码的完整流程：
 *              1. 检查网络连接状态
 *              2. 创建并显示密码输入对话框（模态）
 *              3. 如果用户确认（Accepted）：
 *                 a. 获取用户输入的新密码
 *                 b. 验证密码长度（6-16 位）
 *                 c. AES-128-ECB 加密密码：
 *                    - 使用硬编码密钥（16 字节）
 *                    - PKCS#7 填充到 16 字节
 *                    - 加密后得到 16 字节密文
 *                 d. 构造命令："set_pwd:" + 密文
 *                 e. 通过 TCP 发送到服务器
 *                 f. 处理发送结果：
 *                    - 成功：显示成功提示
 *                    - 失败：显示错误提示
 *              4. 清理对话框资源（deleteLater）
 * 
 * @note 密码加密流程：
 *       明文密码 -> UTF8 编码 -> PKCS#7 填充 -> AES-128-ECB 加密 -> Base64 编码（网络传输）
 *       
 * @note 安全提示：
 *       当前使用硬编码密钥和 ECB 模式，生产环境应使用：
 *       - 安全密钥派生（PBKDF2、bcrypt 等）
 *       - CBC 或 GCM 模式
 *       - 随机 IV
 */
void MainWindow::onChangePwdBtnClicked()
{
    if (!m_networkManager->isConnected()) {
        QMessageBox::warning(this, "提示", "未连接到服务器，请等待重连完成！");
        m_networkManager->connectToServer();
        return;
    }
    
    ChangePwdDialog *dialog = new ChangePwdDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        QString pwd = dialog->getPassword();
        
        if (pwd.length() < 6 || pwd.length() > 16) {
            QMessageBox::warning(this, "提示", "密码长度必须在 6-16 位之间！");
            dialog->deleteLater();
            return;
        }
        
        uint8_t key[16] = {0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,
                           0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x00};
        uint8_t len = 0;
        uint8_t pwdRaw[16] = {0};
        uint8_t pwdCrypt[16] = {0};
        
        QByteArray pwdBytes = pwd.toUtf8();
        len = pwdBytes.length();
        if (len > 16) {
            len = 16;
        }
        memcpy(pwdRaw, pwdBytes.data(), len);
        len = pkcs7_pad(pwdRaw, len);
        aes128_ecb_encrypt(key, pwdRaw, pwdCrypt);
        
        QByteArray cmd = "set_pwd:";
        cmd.append((const char*)pwdCrypt, 16);
        
        qint64 sentBytes = m_networkManager->tcpSocket()->write(cmd);
        if (sentBytes > 0) {
            qDebug() << "修改密码命令发送成功，字节数：" << sentBytes;
            m_uiManager->showTipMessage("密码修改成功！", 4000, QColor(0, 255, 0, 178), Qt::white);
        } else {
            qDebug() << "修改密码命令发送失败：" << m_networkManager->tcpSocket()->errorString();
            QMessageBox::warning(this, "提示", "密码修改失败，请重试！");
        }
    }
    
    dialog->deleteLater();
}

/**
 * @brief 发送通知按钮点击事件处理
 * 
 * @description 处理向服务器发送通知消息的逻辑：
 *              1. 创建并显示通知对话框（模态）
 *              2. 获取用户选择的通知消息和显示时长
 *              3. 构造命令格式："notify_msg:消息内容：显示时长（秒）"
 *              4. 转换为 UTF-8 编码
 *              5. 通过 TCP 发送到服务器
 *              6. 处理发送结果：
 *                 - 成功：显示发送成功提示
 *                 - 失败：显示错误提示
 *              7. 清理对话框资源
 * 
 * @note 通知消息会在服务器端显示，用于远程通知门口人员
 *       支持的消息类型在 NotifyDialog 中预定义（如"快递已放门口"等）
 */
void MainWindow::onSendNotifyBtnClicked()
{
    NotifyDialog *dialog = new NotifyDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        QString notifyMsg = dialog->getSelectedMessage();
        int showTime = dialog->getSelectedTime();
        
        QString sendData = QString("notify_msg:%1:%2").arg(notifyMsg).arg(showTime);
        QByteArray sendBytes = sendData.toUtf8();
        
        qint64 ret = m_networkManager->tcpSocket()->write(sendBytes);
        if (ret > 0) {
            qDebug() << "通知发送成功：" << sendData;
            m_uiManager->showTipMessage("发送成功，通知消息已发送至服务器！", 3000);
        } else {
            qDebug() << "通知发送失败：" << m_networkManager->tcpSocket()->errorString();
            QMessageBox::warning(this, "发送失败", "消息发送失败，请重试！");
        }
    }
    
    dialog->deleteLater();
}

/**
 * @brief 打开人脸库管理对话框
 * 
 * @description 处理人脸库管理功能的入口：
 *              1. 创建 FaceDbDialog 对话框
 *                 - 传入 FaceRecognizerThread 指针用于直接操作人脸库
 *                 - 设置 this 为父对象
 *              2. 连接信号槽：
 *                 - writeCardRequested: 写卡请求 -> onWriteCardRequested()
 *                 - syncUsersRequested: 同步用户请求 -> onSyncUsersRequested()
 *              3. 执行对话框（模态）
 *              4. 对话框关闭后清理资源
 * 
 * @note FaceDbDialog 提供以下功能：
 *       - 添加人脸（拍照或图片导入）
 *       - 删除人脸
 *       - 清空人脸库
 *       - 绑定 IC 卡（RFID 写卡）
 *       - 同步用户到服务器
 *       
 * @note deleteLater() 调用时机：
 *       exec() 返回后对话框已经关闭，此时调用 deleteLater() 安全释放资源
 *       每个函数内的 dialog 局部变量互不影响，因为作用域不同
 */
void MainWindow::onOpenFaceDbBtnClicked()
{
    FaceDbDialog* dialog = new FaceDbDialog(m_faceService->getFaceThread(), this);
    connect(dialog, &FaceDbDialog::writeCardRequested,
            this, &MainWindow::onWriteCardRequested);
    connect(dialog, &FaceDbDialog::syncUsersRequested,
            this, &MainWindow::onSyncUsersRequested);
    
    dialog->exec();
    dialog->deleteLater();
}

/**
 * @brief 网络连接成功事件处理
 * 
 * @description 当 TCP 成功连接到服务器时调用：
 *              1. 打印调试日志
 *              2. 更新 UI 连接状态指示器（绿色）
 *              3. 启用开锁按钮
 * 
 * @note 网络重连由 NetworkManager 内部的定时器自动处理
 */
void MainWindow::onNetworkConnected()
{
    qDebug() << "TCP 连接服务器成功！";
    m_uiManager->updateConnectionStatus(true);
    m_uiManager->getUnlockBtn()->setEnabled(true);
}

/**
 * @brief 网络连接断开事件处理
 * 
 * @description 当 TCP 与服务器断开连接时调用：
 *              1. 打印调试日志
 *              2. 更新 UI 连接状态指示器（红色）
 *              3. 禁用开锁按钮，防止无效操作
 * 
 * @note NetworkManager 会自动尝试重连
 */
void MainWindow::onNetworkDisconnected()
{
    qDebug() << "TCP 与服务器断开连接";
    m_uiManager->updateConnectionStatus(false);
    m_uiManager->getUnlockBtn()->setDisabled(true);
}

/**
 * @brief 网络错误事件处理
 * 
 * @param error QAbstractSocket::SocketError 错误类型
 * 
 * @description 当网络发生错误时调用：
 *              1. 忽略未使用的 error 参数（避免编译器警告）
 *              2. 打印错误描述信息
 *              3. 更新 UI 连接状态为断开
 *              4. 重新启用开锁按钮（允许用户手动重试）
 * 
 * @note 使用 Q_UNUSED 宏或显式转换避免未使用参数警告
 */
void MainWindow::onNetworkError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qDebug() << "TCP 错误：" << m_networkManager->tcpSocket()->errorString();
    m_uiManager->updateConnectionStatus(false);
    m_uiManager->getUnlockBtn()->setEnabled(true);
}

/**
 * @brief TCP 数据接收事件处理
 * 
 * @param data 接收到的原始数据（QByteArray）
 * 
 * @description 处理从服务器收到的 TCP 数据：
 *              1. 打印接收到的原始数据（调试用）
 *              2. 根据数据前缀解析命令类型：
 *                 - "doorbell_notify:": 门铃通知
 *                   提取提示消息并显示
 *                 - "unlock_success": 开锁成功响应（目前无特殊处理）
 *                 - "unlock_failed": 开锁失败响应
 *                   弹出警告对话框提示用户
 * 
 * @note 数据格式采用简单的文本协议：
 *       命令类型：参数 1:参数 2...
 *       使用 QstartsWith() 进行前缀匹配提高解析效率
 */
void MainWindow::onTcpDataReceived(const QByteArray& data)
{
    qDebug() << "收到服务端 TCP 消息：" << data;
    
    if (data.startsWith("doorbell_notify:")) {
        QString tipMsg = QString(data).split(":").at(1);
        m_uiManager->showTipMessage(tipMsg);
    }
    else if (data == "unlock_success") {
        // 开锁成功响应：目前无特殊处理，按钮会在 sendUnlockCommand 中自动恢复
    } else if (data == "unlock_failed") {
        QMessageBox::warning(this, "提示", "开锁失败！");
    }
}

/**
 * @brief UDP 视频帧接收事件处理
 * 
 * @param datagram 接收到的 UDP 数据包（Base64 编码的加密图像数据）
 * 
 * @description 处理从服务器收到的视频帧：
 *              1. Base64 解码：将接收到的 Base64 字符串解码为二进制数据
 *              2. 图像解码：从二进制数据加载 QImage
 *                 - 失败：打印错误日志并返回
 *              3. 格式转换：QImage -> cv::Mat（OpenCV 格式）
 *                 - 失败：打印错误日志并返回
 *              4. 人脸识别处理：调用 FaceService 处理帧
 *                 - 检测人脸
 *                 - 提取特征
 *                 - 识别身份
 *              5. 绘制人脸数据（如果模型已加载）
 *                 - 使用互斥锁保护共享数据
 *                 - 绘制人脸框和关键点
 *              6. 格式转换：cv::Mat -> QImage（显示用）
 *              7. 更新 UI：将处理后的图像显示到视频标签
 * 
 * @note 视频处理流程：
 *       服务器采集 -> AES 加密 -> Base64 编码 -> UDP 广播 -> 客户端解码 -> 人脸识别 -> 显示
 *       
 * @note 性能优化：
 *       - 人脸识别在独立线程运行，不阻塞 UI
 *       - 使用帧跳过策略（每 N 帧处理一次）
 */
void MainWindow::onVideoFrameReceived(const QByteArray& datagram)
{
    QByteArray decryptedByte = QByteArray::fromBase64(datagram.data());
    QImage image;
    
    if (!image.loadFromData(decryptedByte)) {
        qDebug() << "图像解码失败";
        return;
    }
    
    cv::Mat frame = qImageToCvMat(image);
    if (frame.empty()) return;
    
    m_faceService->processFrame(frame);
    
    if (m_faceService->isModelLoaded()) {
        QMutexLocker locker(&m_faceDataMutex);
        m_faceService->drawFaceData(frame, m_cachedFaceData);
    }
    
    QImage result_img = cvMatToQImage(frame);
    m_uiManager->getVideoLabel()->setPixmap(QPixmap::fromImage(result_img));
}

/**
 * @brief 人脸识别成功事件处理
 * 
 * @param result 识别结果结构体（包含姓名、ID、身份、匹配距离）
 * 
 * @description 当人脸识别成功时调用：
 *              1. 构造提示消息："人脸识别成功已开锁，你好 XXX"
 *              2. 显示绿色成功提示（4 秒）
 *              3. 自动发送开锁命令到服务器
 * 
 * @note 这是无感通行的核心逻辑：
 *       检测到人脸 -> 识别成功 -> 自动开锁 -> 显示欢迎消息
 *       
 * @note 识别阈值：
 *       当特征向量距离 < 0.6 时认为识别成功
 *       可通过修改 FaceRecognizerThread::RECOGNITION_THRESHOLD 调整
 */
void MainWindow::onFaceRecognized(const RecognizeResult& result)
{
    QString tipMsg = QString("人脸识别成功已开锁，你好%1").arg(result.name);
    m_uiManager->showTipMessage(tipMsg, 4000, QColor(0, 255, 0, 178), Qt::white);
    sendUnlockCommand();
}

/**
 * @brief 人脸数据处理完成事件处理
 * 
 * @param faceDataList 人脸关键点数据列表（包含人脸框、68 个关键点、128 维特征向量）
 * 
 * @description 当人脸检测和处理完成后调用：
 *              1. 使用互斥锁保护共享数据（线程安全）
 *              2. 缓存人脸数据供绘制使用
 * 
 * @note 该槽函数在 FaceService 的独立线程中被调用
 *       使用 QMutexLocker 确保多线程访问 m_cachedFaceData 的安全性
 */
void MainWindow::onFaceDataProcessed(const std::vector<FaceLandmarkData>& faceDataList)
{
    QMutexLocker locker(&m_faceDataMutex);
    m_cachedFaceData = faceDataList;
}

/**
 * @brief 人脸模型加载完成事件处理
 * 
 * @param success 加载是否成功
 * @param errorMsg 错误信息（如果失败）
 * 
 * @description 当人脸识别模型加载完成后调用：
 *              - 成功：打印调试日志
 *              - 失败：打印错误日志并显示严重错误对话框
 * 
 * @note 模型加载是异步过程，在 FaceRecognizerThread 中执行
 *       加载完成后通过信号通知主线程
 *       
 * @note 模型文件：
 *       - shape_predictor_68_face_landmarks.dat (98.9MB): 68 个人脸关键点预测
 *       - dlib_face_recognition_resnet_model_v1.dat (29.7MB): ResNet 人脸识别模型
 */
void MainWindow::onModelsLoaded(bool success, const QString& errorMsg)
{
    if (success) {
        qDebug() << "人脸模型加载成功（子线程）";
    } else {
        qCritical() << "人脸模型加载失败：" << errorMsg;
        QMessageBox::critical(this, "初始化失败", QString("模型加载失败：%1").arg(errorMsg));
    }
}

/**
 * @brief 写卡请求处理
 * 
 * @param id 用户 ID
 * @param name 用户姓名
 * 
 * @description 处理来自人脸库对话框的写卡请求：
 *              1. 检查网络连接状态
 *              2. 构造命令格式："WriteCard:ID: 姓名"
 *              3. 转换为 UTF-8 编码
 *              4. 通过 TCP 发送到服务器
 *              5. 处理发送结果：
 *                 - 成功：显示提示消息
 *                 - 失败：显示错误对话框
 * 
 * @note 写卡流程：
 *       客户端发送写卡请求 -> 服务器准备写卡数据 -> 用户放置卡片 -> 服务器写卡 -> 返回结果
 *       
 * @note RFID 卡片数据存储：
 *       - 扇区 1：存储用户 ID（4 字节）
 *       - 扇区 2：存储用户姓名（16 字节）
 */
void MainWindow::onWriteCardRequested(const QString& id, const QString& name)
{
    if (!m_networkManager->isConnected()) {
        QMessageBox::warning(this, "Write Card Failed", "Not connected to the server. Please wait for reconnection.");
        m_networkManager->connectToServer();
        return;
    }
    
    QString sendData = QString("WriteCard:%1:%2").arg(id).arg(name);
    QByteArray cmd = sendData.toUtf8();
    qint64 ret = m_networkManager->tcpSocket()->write(cmd);
    if (ret > 0) {
        qDebug() << "Write card command sent" << sendData;
        m_uiManager->showTipMessage("Write card command sent.", 3000, QColor(0, 255, 0, 178), Qt::white);
    } else {
        qDebug() << "Write Card Failed" << m_networkManager->tcpSocket()->errorString();
        QMessageBox::warning(this, "Write Card Failed", "Write card command failed. Please retry.");
    }
}

/**
 * @brief 同步用户请求处理
 * 
 * @param userIds 需要同步的用户 ID 列表（QStringList）
 * 
 * @description 处理客户端人脸库与服务端 EEPROM 的用户同步：
 *              1. 检查网络连接状态
 *              2. 构造命令格式："SyncUsers:ID1,ID2,ID3,..."
 *              3. 发送同步命令到服务器
 *              4. 服务器处理逻辑：
 *                 - 读取客户端用户列表
 *                 - 对比 EEPROM 中的用户
 *                 - 添加缺失的用户
 *                 - 删除多余的用户
 *                 - 返回同步结果
 * 
 * @note 同步场景：
 *       当客户端添加/删除人脸后，需要同步到服务端 EEPROM，确保刷卡开锁功能正常
 *       
 * @note 同步策略：
 *       增量同步：只传输差异部分，提高效率
 */
void MainWindow::onSyncUsersRequested(const QStringList& userIds)
{
    if (!m_networkManager->isConnected()) {
        qDebug() << "Sync users failed: Not connected to server";
        return;
    }
    
    QString sendData = "SyncUsers:" + userIds.join(",");
    QByteArray cmd = sendData.toUtf8();
    qint64 ret = m_networkManager->tcpSocket()->write(cmd);
    if (ret > 0) {
        qDebug() << "Sync users command sent:" << sendData;
    } else {
        qDebug() << "Sync users command failed:" << m_networkManager->tcpSocket()->errorString();
    }
}

/**
 * @brief 发送开锁命令到服务器
 * 
 * @description 执行实际的开锁命令发送操作：
 *              1. 构造开锁命令："unlock"
 *              2. 通过 TCP 发送到服务器
 *              3. 检查发送结果：
 *                 - 成功：
 *                   a. 打印调试日志
 *                   b. 设置 1 秒定时器恢复开锁按钮（防抖）
 *                 - 失败：
 *                   a. 打印错误日志
 *                   b. 立即恢复开锁按钮
 * 
 * @note 防抖设计：
 *       开锁按钮按下后禁用 1 秒，防止重复发送命令
 *       使用 QTimer::singleShot 实现异步恢复
 *       
 * @note 服务器响应：
 *       服务器收到命令后控制舵机转动，实现开锁动作
 *       舵机延时 3 秒后自动复位（关门）
 */
void MainWindow::sendUnlockCommand()
{
    QByteArray cmd = "unlock";
    qint64 sentBytes = m_networkManager->tcpSocket()->write(cmd);
    if (sentBytes > 0) {
        qDebug() << "开锁命令发送成功，字节数：" << sentBytes;
        QTimer::singleShot(1000, [this]() {
            m_uiManager->getUnlockBtn()->setEnabled(true);
        });
    } else {
        qDebug() << "开锁命令发送失败";
        m_uiManager->getUnlockBtn()->setEnabled(true);
    }
}

/**
 * @brief QImage 转换为 cv::Mat（OpenCV 格式）
 * 
 * @param qimg Qt 图像对象
 * @return cv::Mat OpenCV 矩阵对象
 * 
 * @description 实现 Qt 图像格式到 OpenCV 图像格式的转换：
 *              1. 检查图像格式是否为 QImage::Format_RGB32
 *              2. 使用 cv::Mat 构造函数包装 QImage 数据
 *                 - 高度：qimg.height()
 *                 - 宽度：qimg.width()
 *                 - 类型：CV_8UC4 (4 通道 8 位无符号整数)
 *                 - 数据指针：qimg.bits()
 *                 - 行字节数：qimg.bytesPerLine()
 *              3. 克隆矩阵（深拷贝，避免数据共享）
 *              4. 颜色空间转换：BGRA -> BGR（移除 Alpha 通道）
 * 
 * @note 为什么要转换？
 *       Qt 的 QImage 适合 UI 显示，而 OpenCV 的 Mat 适合图像处理
 *       人脸识别算法基于 OpenCV，需要 Mat 格式
 *       
 * @note 内存管理：
 *       clone() 创建独立副本，避免 QImage 释放后 Mat 数据悬空
 */
cv::Mat MainWindow::qImageToCvMat(const QImage &qimg)
{
    cv::Mat mat;
    if (qimg.format() == QImage::Format_RGB32) {
        mat = cv::Mat(qimg.height(), qimg.width(), CV_8UC4,
                      const_cast<uchar*>(qimg.bits()), qimg.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR);
    }
    return mat;
}

/**
 * @brief cv::Mat 转换为 QImage（Qt 格式）
 * 
 * @param cvmat OpenCV 矩阵对象
 * @return QImage Qt 图像对象
 * 
 * @description 实现 OpenCV 图像格式到 Qt 图像格式的转换：
 *              1. 检查 Mat 是否为空
 *                 - 空：返回空 QImage 并打印日志
 *              2. 检查 Mat 类型是否为 16 (CV_8UC3，3 通道 BGR)
 *              3. 颜色空间转换：BGR -> BGRA（添加 Alpha 通道）
 *              4. 构造 QImage：
 *                 - 数据指针：mat.data
 *                 - 宽度：mat.cols
 *                 - 高度：mat.rows
 *                 - 行字节数：mat.step
 *                 - 格式：QImage::Format_RGB32
 *              5. 深拷贝 QImage（避免 Mat 释放后数据悬空）
 * 
 * @note 为什么要转换？
 *       OpenCV 处理后的图像需要转换回 Qt 格式才能在 QLabel 中显示
 *       
 * @note 性能考虑：
 *       copy() 创建深拷贝，确保 QImage 独立于 Mat 存在
 *       这是必要的，因为 Mat 可能在下一帧被修改
 */
QImage MainWindow::cvMatToQImage(const cv::Mat &cvmat)
{
    if (cvmat.empty()) {
        qDebug() << "空的 cvmat，返回空 QImage";
        return QImage();
    }
    
    QImage qimg;
    if (cvmat.type() == 16) {
        cv::Mat bgra_mat;
        cv::cvtColor(cvmat, bgra_mat, cv::COLOR_BGR2BGRA);
        qimg = QImage(bgra_mat.data, bgra_mat.cols, bgra_mat.rows,
                     bgra_mat.step, QImage::Format_RGB32).copy();
    }
    return qimg;
}

/**
 * @brief 窗口大小调整事件处理
 * 
 * @param event QResizeEvent 事件对象
 * 
 * @description 当窗口大小改变时重新定位视频标签：
 *              1. 忽略未使用的 event 参数（避免编译器警告）
 *              2. 检查 UIManager 和视频标签是否有效
 *              3. 计算居中位置：
 *                 - X 坐标：(窗口宽度 - 视频宽度) / 2
 *                 - Y 坐标：(窗口高度 - 视频高度) / 2
 *              4. 移动视频标签到新位置
 * 
 * @note 固定视频尺寸：
 *       视频标签固定为 640x480，不随窗口缩放
 *       仅调整位置保持居中
 *       
 * @note 改进建议：
 *       可以使用 QSizePolicy 和布局管理器实现自适应布局
 */
void MainWindow::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    if (m_uiManager && m_uiManager->getVideoLabel()) {
        m_uiManager->getVideoLabel()->move((this->width() - 640) / 2, (this->height() - 480) / 2);
    }
}

/**
 * @brief 显示提示消息（便捷函数）
 * 
 * @param tipText 提示文本内容
 * @param durationMs 显示时长（毫秒，默认 3000ms）
 * @param bgColor 背景颜色（默认半透明红色）
 * @param textColor 文字颜色（默认白色）
 * 
 * @description 封装 UIManager 的提示消息显示功能：
 *              直接调用 UIManager::showTipMessage() 实现
 * 
 * @note 便捷设计：
 *       提供简短的接口，避免重复书写 m_uiManager->
 *       可设置默认参数，简化常用场景调用
 */
void MainWindow::showTipMessage(const QString& tipText, int durationMs,
                                const QColor& bgColor, const QColor& textColor)
{
    m_uiManager->showTipMessage(tipText, durationMs, bgColor, textColor);
}
