/******************************************************************
* @projectName   video_client
* @brief         configmanager.cpp - 配置管理器实现
* @author        Lu Yaolong
* 
* @description   本文件实现了配置管理器，负责读取和保存应用程序配置
* 
* @responsibilities
*                - 从 config.ini 读取配置
*                - 保存配置到文件
*                - 支持相对路径和绝对路径
*                - 提供全局访问接口（单例模式）
* 
* @design_pattern  单例模式（Singleton）
*                 - 全局唯一实例
*                 - 懒汉式创建
*                 - 线程安全
*******************************************************************/
#include "configmanager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

/**
 * @brief ConfigManager 构造函数
 * 
 * @description 初始化配置管理器：
 *              - 初始化 m_settings 为 nullptr
 *              - 设置默认 TCP 端口为 8878
 *              - 自动调用 load() 加载配置
 * 
 * @note 构造函数私有化，只能通过 instance() 访问
 *       使用静态局部变量实现线程安全的单例
 */
ConfigManager::ConfigManager()
    : m_settings(nullptr)
    , m_tcpPort(8878)
{
    load();
}

/**
 * @brief 获取单例实例
 * 
 * @return ConfigManager& 单例引用
 * 
 * @description 返回全局唯一的 ConfigManager 实例：
 *              - 使用静态局部变量
 *              - C++11 保证线程安全
 *              - 懒汉式创建（首次调用时创建）
 * 
 * @return 单例对象的引用
 * 
 * @note 使用方式：ConfigManager::instance().getServerIp()
 */
ConfigManager& ConfigManager::instance()
{
    static ConfigManager instance;
    return instance;
}

/**
 * @brief 获取配置文件路径
 * 
 * @return QString 配置文件的完整路径
 * 
 * @description 获取 config.ini 的完整路径：
 *              1. 获取应用程序目录
 *              2. 拼接配置文件名
 *              3. 转换为本地路径分隔符
 * 
 * @return 配置文件路径
 * 
 * @note 配置文件与可执行文件在同一目录
 */
QString ConfigManager::getConfigFilePath() const
{
    QString configPath = QCoreApplication::applicationDirPath();
    return QDir::toNativeSeparators(configPath + "/config.ini");
}

/**
 * @brief 获取人脸关键点模型路径
 * 
 * @return QString 模型文件路径
 * 
 * @description 返回 shape_predictor_68_face_landmarks.dat 的路径
 * 
 * @note 路径可能是绝对路径或相对于 config.ini 的路径
 */
QString ConfigManager::getFaceLandmarkModelPath() const
{
    return m_faceLandmarkModelPath;
}

/**
 * @brief 获取人脸识别模型路径
 * 
 * @return QString 模型文件路径
 * 
 * @description 返回 dlib_face_recognition_resnet_model_v1.dat 的路径
 * 
 * @note 路径可能是绝对路径或相对于 config.ini 的路径
 */
QString ConfigManager::getFaceRecognitionModelPath() const
{
    return m_faceRecognitionModelPath;
}

/**
 * @brief 获取测试图片路径
 * 
 * @return QString 测试图片路径
 * 
 * @description 返回测试图片的路径（用于调试）
 */
QString ConfigManager::getTestImagePath() const
{
    return m_testImagePath;
}

/**
 * @brief 获取测试输出路径
 * 
 * @return QString 测试输出路径
 * 
 * @description 返回测试输出目录的路径
 */
QString ConfigManager::getTestOutputPath() const
{
    return m_testOutputPath;
}

/**
 * @brief 获取服务器 IP 地址
 * 
 * @return QString 服务器 IP
 * 
 * @description 返回 TCP 服务器的 IP 地址
 * 
 * @note 默认值：192.168.3.15
 */
QString ConfigManager::getServerIp() const
{
    return m_serverIp;
}

/**
 * @brief 获取 TCP 端口号
 * 
 * @return quint16 TCP 端口号
 * 
 * @description 返回 TCP 连接的端口号
 * 
 * @note 默认值：8878
 */
quint16 ConfigManager::getTcpPort() const
{
    return m_tcpPort;
}

/**
 * @brief 设置人脸关键点模型路径
 * 
 * @param path 模型文件路径
 */
void ConfigManager::setFaceLandmarkModelPath(const QString& path)
{
    m_faceLandmarkModelPath = path;
}

/**
 * @brief 设置人脸识别模型路径
 * 
 * @param path 模型文件路径
 */
void ConfigManager::setFaceRecognitionModelPath(const QString& path)
{
    m_faceRecognitionModelPath = path;
}

/**
 * @brief 设置测试图片路径
 * 
 * @param path 测试图片路径
 */
void ConfigManager::setTestImagePath(const QString& path)
{
    m_testImagePath = path;
}

/**
 * @brief 设置测试输出路径
 * 
 * @param path 测试输出路径
 */
void ConfigManager::setTestOutputPath(const QString& path)
{
    m_testOutputPath = path;
}

/**
 * @brief 设置服务器 IP 地址
 * 
 * @param ip IP 地址
 */
void ConfigManager::setServerIp(const QString& ip)
{
    m_serverIp = ip;
}

/**
 * @brief 设置 TCP 端口号
 * 
 * @param port TCP 端口号
 */
void ConfigManager::setTcpPort(quint16 port)
{
    m_tcpPort = port;
}

/**
 * @brief 从配置文件加载配置
 * 
 * @description 从 config.ini 读取所有配置项：
 *              1. 获取配置文件路径
 *              2. 创建 QSettings 对象
 *              3. 读取 FaceRecognition 组：
 *                 - landmark_model: 人脸关键点模型
 *                 - recognition_model: 人脸识别模型
 *                 - test_image: 测试图片
 *                 - test_output: 测试输出
 *              4. 读取 Network 组：
 *                 - server_ip: 服务器 IP
 *                 - tcp_port: TCP 端口
 *              5. 处理相对路径（转换为绝对路径）
 *              6. 打印调试日志
 * 
 * @note 支持相对路径和绝对路径
 *       相对路径相对于 config.ini 所在目录
 */
void ConfigManager::load()
{
    QString configPath = getConfigFilePath();
    qDebug() << "Loading config from:" << configPath;

    m_settings = new QSettings(configPath, QSettings::IniFormat);

    // 获取 config.ini 所在目录，用于构建相对路径
    QFileInfo configFileInfo(configPath);
    QString configDir = configFileInfo.absolutePath();
    qDebug() << "配置文件所在目录:" << configDir;

    m_settings->beginGroup("FaceRecognition");
    QString landmarkModel = m_settings->value("landmark_model", "").toString();
    QString recognitionModel = m_settings->value("recognition_model", "").toString();
    
    // 如果是相对路径，转换为相对于 config.ini 的绝对路径
    if (!landmarkModel.isEmpty() && !QFileInfo(landmarkModel).isAbsolute()) {
        m_faceLandmarkModelPath = QDir(configDir).filePath(landmarkModel);
    } else {
        m_faceLandmarkModelPath = landmarkModel;
    }
    
    if (!recognitionModel.isEmpty() && !QFileInfo(recognitionModel).isAbsolute()) {
        m_faceRecognitionModelPath = QDir(configDir).filePath(recognitionModel);
    } else {
        m_faceRecognitionModelPath = recognitionModel;
    }
    
    m_testImagePath = m_settings->value("test_image", "").toString();
    m_testOutputPath = m_settings->value("test_output", "").toString();
    m_settings->endGroup();

    m_settings->beginGroup("Network");
    m_serverIp = m_settings->value("server_ip", "192.168.3.15").toString();
    m_tcpPort = m_settings->value("tcp_port", 8878).toUInt();
    m_settings->endGroup();

    qDebug() << "Config loaded:";
    qDebug() << "  Landmark model:" << m_faceLandmarkModelPath;
    qDebug() << "  Recognition model:" << m_faceRecognitionModelPath;
    qDebug() << "  Server IP:" << m_serverIp;
    qDebug() << "  TCP Port:" << m_tcpPort;
}

/**
 * @brief 保存配置到文件
 * 
 * @description 将当前配置保存到 config.ini：
 *              1. 检查 m_settings 是否有效
 *              2. 写入 FaceRecognition 组：
 *                 - landmark_model
 *                 - recognition_model
 *                 - test_image
 *                 - test_output
 *              3. 写入 Network 组：
 *                 - server_ip
 *                 - tcp_port
 *              4. 调用 sync() 立即写入磁盘
 *              5. 打印调试日志
 * 
 * @note 修改配置后必须调用 save() 才能持久化
 */
void ConfigManager::save()
{
    if (!m_settings) {
        return;
    }

    m_settings->beginGroup("FaceRecognition");
    m_settings->setValue("landmark_model", m_faceLandmarkModelPath);
    m_settings->setValue("recognition_model", m_faceRecognitionModelPath);
    m_settings->setValue("test_image", m_testImagePath);
    m_settings->setValue("test_output", m_testOutputPath);
    m_settings->endGroup();

    m_settings->beginGroup("Network");
    m_settings->setValue("server_ip", m_serverIp);
    m_settings->setValue("tcp_port", m_tcpPort);
    m_settings->endGroup();

    m_settings->sync();
    qDebug() << "Config saved to:" << getConfigFilePath();
}
