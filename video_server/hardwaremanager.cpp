/******************************************************************
* @projectName   video_server
* @brief         hardwaremanager.cpp - 硬件设备管理器实现
* @description   本文件实现了嵌入式端硬件设备统一管理模块
*                负责 RFID 读卡、舵机控制、EEPROM 存储等硬件操作
* @features      1. RC522 RFID 读卡器管理（ISO14443A 协议）
*                2. 舵机角度控制（0-90°，门锁开关）
*                3. AT24C64 EEPROM 存储（用户信息、密码）
*                4. 定时器轮询读卡（100ms 间隔）
*                5. 写卡功能（ID 卡绑定）
*                6. 用户数据库验证（EEPROM 查询）
*                7. 自动关锁机制（5 秒延时）
* @hardware      RC522 RFID(SPI), 舵机 (PWM), AT24C64(I2C)
* @eeprom_layout 用户信息结构体：ID(4 字节)+姓名 (16 字节)
*                密码哈希：SHA256(32 字节)
* @author        Lu Yaolong
*******************************************************************/
#include "hardwaremanager.h"
#include <QMetaType>

/**
 * @brief 构造函数
 * @param parent 父对象指针
 * @description 初始化硬件管理器成员变量：
 *              1. 初始化指针成员为 nullptr
 *              2. 初始化标志位为 false
 *              3. 清零写卡数据缓冲区
 */
HardwareManager::HardwareManager(QObject *parent)
    : QObject(parent)
    , m_servoControl(nullptr)
    , m_eeprom(nullptr)
    , m_readCardTimer(nullptr)
    , m_writeCardTimer(nullptr)
    , m_doorLockTimer(nullptr)
    , m_isUnlocked(false)
    , m_isAllowControlReset(false)
    , m_allowControl1(false)
{
    memset(m_writeData, 0, sizeof(m_writeData));
}

/**
 * @brief 析构函数
 * @description 停止读卡，清理资源
 */
HardwareManager::~HardwareManager()
{
    stopCardReading();
}

/**
 * @brief 硬件初始化
 * @return bool 成功返回 true，失败返回 false
 * @description 初始化所有硬件设备：
 *              1. 创建舵机控制对象（ServoControl）
 *              2. 创建 EEPROM 对象（AT24C64）
 *              3. 初始化 EEPROM（检查设备）
 *              4. 复位 RC522 读卡器
 *              5. 配置 RC522 为 Type A 模式
 *              6. 创建定时器：
 *                 - 读卡定时器（100ms）
 *                 - 写卡定时器（300ms）
 *                 - 门锁定时器（单次触发，5 秒）
 *              7. 启动读卡定时器
 * @note 任何硬件初始化失败都会返回 false
 */
bool HardwareManager::initialize()
{
    m_servoControl = new ServoControl();
    m_eeprom = new AT24C64();
    
    if (!m_eeprom->init()) {
        qWarning() << "AT24C64 初始化失败";
        return false;
    }
    
    m_eeprom->initEEPROM();
    
    int ret = RC522::PcdReset();
    if (ret != 0) {
        qWarning() << "RC522 复位失败：" << ret;
        return false;
    }
    
    RC522::M500PcdConfigISOType('A');
    qDebug() << "RC522 初始化完成";
    
    m_readCardTimer = new QTimer(this);
    m_readCardTimer->setInterval(100);
    connect(m_readCardTimer, &QTimer::timeout, 
            this, &HardwareManager::onReadCardTimer);
    
    m_writeCardTimer = new QTimer(this);
    m_writeCardTimer->setInterval(300);
    connect(m_writeCardTimer, &QTimer::timeout, 
            this, &HardwareManager::onWriteCardTimer);
    
    m_doorLockTimer = new QTimer(this);
    m_doorLockTimer->setSingleShot(true);
    connect(m_doorLockTimer, &QTimer::timeout, 
            this, &HardwareManager::onDoorLockTimer);
    
    startCardReading();
    
    return true;
}

/**
 * @brief 执行开锁操作
 * @description 控制舵机转动 90°实现开锁：
 *              1. 检查是否已开锁（避免重复）
 *              2. 设置舵机角度为 90°
 *              3. 更新开锁状态标志
 *              4. 更新门锁状态为 false
 *              5. 启动 5 秒自动关锁定时器
 *              6. 发射 doorUnlocked 信号
 * @note 5 秒后自动调用 onDoorLockTimer() 关锁
 */
void HardwareManager::unlockDoor()
{
    if (m_isUnlocked) {
        return;
    }
    
    m_servoControl->setAngle(90);
    m_isUnlocked = true;
    m_isAllowControlReset = 1;
    
    setDoorLockedState(false);
    
    m_doorLockTimer->stop();
    m_doorLockTimer->start(5000);
    
    emit doorUnlocked();
    qDebug() << "执行开锁操作";
}

/**
 * @brief 执行关锁操作
 * @description 调用 resetServoAngle() 将舵机复位到 0°
 * @note 需要门已关闭（m_allowControl1=true）才能成功
 */
void HardwareManager::lockDoor()
{
    resetServoAngle();
}

/**
 * @brief 设置门锁状态标志
 * @param locked true-门锁闭，false-门开启
 * @description 更新内部门锁状态标志（用于振动检测联动）
 */
void HardwareManager::setDoorLockedState(bool locked)
{
    m_isUnlocked = !locked;
}

/**
 * @brief 启动读卡
 * @description 启动读卡定时器（每 100ms 读取一次 RC522）
 */
void HardwareManager::startCardReading()
{
    if (m_readCardTimer) {
        m_readCardTimer->start();
    }
}

/**
 * @brief 停止读卡
 * @description 停止读卡和写卡定时器
 */
void HardwareManager::stopCardReading()
{
    if (m_readCardTimer) {
        m_readCardTimer->stop();
    }
    if (m_writeCardTimer) {
        m_writeCardTimer->stop();
    }
}

/**
 * @brief 准备写卡
 * @param cardId 要写入的卡片 ID
 * @param userName 要写入的用户名
 * @description 准备写卡数据并启动写卡模式：
 *              1. 保存待写入的卡 ID 和用户名
 *              2. 清零写卡数据缓冲区
 *              3. 将卡 ID 转为字节存入缓冲区
 *              4. 停止读卡定时器
 *              5. 启动写卡定时器（300ms 间隔）
 * @note 写卡时停止读卡，避免冲突
 */
void HardwareManager::prepareWriteCard(const QString& cardId, const QString& userName)
{
    m_pendingWriteCardId = cardId;
    m_pendingWriteUserName = userName;
    
    memset(m_writeData, 0, sizeof(m_writeData));
    QByteArray cardIdBytes = cardId.toUtf8();
    memcpy(m_writeData, cardIdBytes.constData(), qMin(cardIdBytes.size(), 16));
    
    m_readCardTimer->stop();
    m_writeCardTimer->start();
    
    qDebug() << "准备写卡：" << cardId << userName;
}

/**
 * @brief 添加卡片到数据库
 * @param cardId 卡片 ID
 * @param userName 用户名
 * @return bool 成功返回 true，失败返回 false
 * @description 将用户信息写入 EEPROM：
 *              1. 创建 EEP_User 结构体
 *              2. 清零并填充数据（ID+ 姓名）
 *              3. 调用 EEPROM 的 addUser 方法
 * @note 姓名限制 15 字节，超出会被截断
 */
bool HardwareManager::addCardToDatabase(const QString& cardId, const QString& userName)
{
    EEP_User user;
    memset(&user, 0, sizeof(EEP_User));
    QByteArray nameBytes = userName.toUtf8();
    memcpy(user.name, nameBytes.constData(), qMin(nameBytes.size(), 15));
    user.id = cardId.toUInt();
    
    return m_eeprom->addUser(&user);
}

/**
 * @brief 验证卡片合法性
 * @param cardId 卡片 ID
 * @param userName 输出参数，验证成功时返回用户名
 * @return bool 合法返回 true，非法返回 false
 * @description 在 EEPROM 中查询卡片 ID：
 *              1. 获取用户总数
 *              2. 遍历所有用户
 *              3. 比对 ID，找到匹配项
 *              4. 返回用户名和 true
 * @note 遍历查找时间复杂度 O(n)
 */
bool HardwareManager::verifyCard(const QString& cardId, QString& userName)
{
    uint8_t userCount = m_eeprom->getUserCount();
    for (int i = 0; i < userCount; i++) {
        EEP_User user;
        if (m_eeprom->readUser(i, &user)) {
            if (user.id == cardId.toUInt()) {
                userName = QString::fromUtf8((char*)user.name).trimmed();
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief 读卡定时器槽函数
 * @description 每 100ms 调用一次 rc522ReadCard() 读取卡片
 */
void HardwareManager::onReadCardTimer()
{
    rc522ReadCard();
}

/**
 * @brief 写卡定时器槽函数
 * @description 每 300ms 调用一次 rc522WriteCard() 写入卡片
 */
void HardwareManager::onWriteCardTimer()
{
    rc522WriteCard();
}

/**
 * @brief 门锁定时器槽函数
 * @description 5 秒延时后自动调用 resetServoAngle() 关锁
 */
void HardwareManager::onDoorLockTimer()
{
    resetServoAngle();
}

/**
 * @brief RC522 读卡函数
 * @description 读取 RC522 卡片数据并验证：
 *              1. 调用 IC_Read_Or_Write(1, NULL, read_buf) 读卡
 *              2. 检查返回状态（0 表示成功）
 *              3. 解析卡片数据（UTF-8 字符串）
 *              4. 验证卡片（verifyCard）
 *              5. 成功：
 *                 - 发射 cardDetected 信号
 *                 - 调用 unlockDoor()
 *                 - 暂停读卡 2 秒
 *              6. 失败：
 *                 - 发射 unauthorizedCard 信号
 * @note 读卡成功后暂停 2 秒避免重复触发
 */
void HardwareManager::rc522ReadCard()
{
    uint8_t read_buf[16] = {0};
    int8_t state = RC522::IC_Read_Or_Write(1, NULL, read_buf);
    
    if (state == 0) {
        QString cardData = QString::fromUtf8((char*)read_buf).trimmed();
        qDebug() << "读取到卡片数据：" << cardData;
        
        if (!cardData.isEmpty()) {
            QString userName;
            if (verifyCard(cardData, userName)) {
                qDebug() << "欢迎回家，" << userName << "！";
                emit cardDetected(cardData, userName);
                unlockDoor();
                
                m_readCardTimer->stop();
                QTimer::singleShot(2000, this, [this]() {
                    m_readCardTimer->start();
                });
            } else {
                qDebug() << "未授权卡片：" << cardData;
                emit unauthorizedCard(cardData);
            }
        }
    }
}

/**
 * @brief RC522 写卡函数
 * @description 将数据写入 RFID 卡片：
 *              1. 调用 IC_Read_Or_Write(0, m_writeData, read_buf) 写卡
 *              2. 检查返回状态（12 表示成功）
 *              3. 验证待写入数据有效性
 *              4. 添加到数据库（addCardToDatabase）
 *              5. 成功：
 *                 - 发射 writeCardSuccess 信号
 *                 - 清空待写入数据
 *              6. 失败：
 *                 - 发射 writeCardFailed 信号
 *              7. 停止写卡，恢复读卡
 * @note 写卡成功状态码：12
 */
void HardwareManager::rc522WriteCard()
{
    uint8_t read_buf[16] = {0};
    int8_t state = RC522::IC_Read_Or_Write(0, m_writeData, read_buf);
    
    if (state == 12) {
        if (!m_pendingWriteCardId.isEmpty() && !m_pendingWriteUserName.isEmpty()) {
            if (addCardToDatabase(m_pendingWriteCardId, m_pendingWriteUserName)) {
                qDebug() << "写卡成功，用户信息已存入数据库：" 
                         << m_pendingWriteUserName << "ID:" << m_pendingWriteCardId;
                emit writeCardSuccess(m_pendingWriteCardId, m_pendingWriteUserName);
            } else {
                qWarning() << "写卡成功，但用户信息存入数据库失败！";
                emit writeCardFailed();
            }
            
            m_pendingWriteCardId.clear();
            m_pendingWriteUserName.clear();
        }
        
        m_writeCardTimer->stop();
        m_readCardTimer->start();
    }
}

/**
 * @brief 舵机复位函数（关锁）
 * @description 将舵机角度复位到 0°实现关锁：
 *              1. 检查门是否已关闭（m_allowControl1）
 *              2. 门未关闭：
 *                 - 禁止关锁
 *                 - 保持开锁状态
 *                 - 直接返回
 *              3. 门已关闭：
 *                 - 重置允许控制标志
 *                 - 设置舵机角度为 0°
 *                 - 更新开锁状态为 false
 *                 - 更新门锁状态为 true
 *                 - 发射 doorLocked 信号
 * @note 门磁传感器检测门是否关闭（ADC 通道 2）
 */
void HardwareManager::resetServoAngle()
{
    if (!m_allowControl1) {
        qDebug() << "门未关闭，禁止关锁";
        m_isUnlocked = true;
        return;
    }
    
    m_isAllowControlReset = 0;
    m_servoControl->setAngle(0);
    printf("Sbdsss°\n");
    m_isUnlocked = false;
    
    setDoorLockedState(true);
    
    emit doorLocked();
    qDebug() << "舵机复位，门锁闭";
}
