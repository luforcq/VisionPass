/******************************************************************
* @projectName   video_client
* @brief         facedbmanager.cpp - 人脸数据库管理器实现
* @description   本文件实现了人脸数据库管理器，使用 SQLite 存储人脸特征
* @features      1. 单例模式（全局唯一实例）
*                2. SQLite 数据库操作（增删改查）
*                3. 人脸特征序列化/反序列化（Dlib 矩阵 ↔ QByteArray）
*                4. 预览图存储（BLOB 字段）
*                5. 批量加载人脸特征
* @database      SQLite (face_db.db)
* @table_schema  face_registry(id, name, identity, descriptor, preview, create_time)
* @author        Lu Yaolong
*******************************************************************/
#include "facedbmanager.h"
#include <QDebug>
#include <QDataStream>
#include <QSqlError>
#include <QDateTime>

// 静态成员变量初始化（单例模式）
FaceDbManager* FaceDbManager::m_instance = nullptr;

/**
 * @brief 构造函数（私有）
 * @param parent 父对象指针
 * @description 初始化数据库管理器（不立即打开数据库）
 * @note 构造函数设为私有是单例模式的关键
 */
FaceDbManager::FaceDbManager(QObject *parent) : QObject(parent)
{
}

/**
 * @brief 析构函数
 * @description 关闭数据库连接（如果已打开）
 */
FaceDbManager::~FaceDbManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

/**
 * @brief 检查人脸 ID 是否存在
 * @param id 人脸 ID（字符串）
 * @return bool 存在返回 true，否则返回 false
 * @description 查询数据库中是否已存在指定 ID 的人脸记录
 * @note 使用参数化查询防止 SQL 注入
 */
bool FaceDbManager::isFaceExists(const QString& id)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM face_registry WHERE id = :id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return true; // ID 已存在
    }
    return false;
}

/**
 * @brief 获取单例实例（线程不安全版本）
 * @return FaceDbManager* 单例实例指针
 * @description 提供全局访问点，确保只有一个实例
 * @note 懒汉式单例（首次调用时创建）
 * @warning 多线程环境下需要加锁
 */
FaceDbManager *FaceDbManager::getInstance()
{
    if (!m_instance) {
        m_instance = new FaceDbManager();
    }
    return m_instance;
}

/**
 * @brief 初始化数据库
 * @param dbPath 数据库文件路径（默认为"./face_db.db"）
 * @return bool 成功返回 true，失败返回 false
 * @description 初始化 SQLite 数据库并创建表结构：
 *              1. 检查数据库是否已打开
 *              2. 添加 SQLite 驱动
 *              3. 设置数据库文件名
 *              4. 打开数据库
 *              5. 创建 face_registry 表
 * @note 表结构包含 id(主键)、name、identity、descriptor(BLOB)、preview(BLOB)、create_time
 */
bool FaceDbManager::initDb(const QString &dbPath)
{
    if (m_db.isOpen()) {
        return true;  // 已经打开
    }
    
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath.isEmpty() ? "./face_db.db" : dbPath);

    if (!m_db.open()) {
        qCritical() << "数据库打开失败：" << m_db.lastError().text();
        return false;
    }

    // 创建表（含 preview 字段）
    QSqlQuery query(m_db);
    QString createSql = R"(
        CREATE TABLE IF NOT EXISTS face_registry (
            id TEXT PRIMARY KEY NOT NULL,
            name TEXT,
            identity TEXT,
            descriptor BLOB NOT NULL,
            preview BLOB,
            create_time TIMESTAMP DEFAULT (datetime('now', '+8 hours'))
        );
    )";
    
    if (!query.exec(createSql)) {
        qCritical() << "创建表失败：" << query.lastError().text();
        return false;
    }
    
    qDebug() << "数据库初始化成功";
    return true;
}

/**
 * @brief 序列化人脸特征描述子
 * @param descriptor Dlib 矩阵类型的人脸特征向量
 * @return QByteArray 序列化后的二进制数据
 * @description 将 Dlib 的 128 维特征向量转换为 QByteArray 存储到数据库：
 *              1. 创建 QByteArray 和 QDataStream
 *              2. 设置数据流版本（Qt_5_6）
 *              3. 写入向量大小
 *              4. 逐个写入浮点数值
 * @note 序列化格式：[size(int)][float0][float1]...[float127]
 */
QByteArray FaceDbManager::serializeDescriptor(const dlib::matrix<float,0,1> &descriptor)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_6);
    int size = descriptor.size();
    stream << size;
    for (int i = 0; i < size; ++i) {
        stream << descriptor(i);
    }
    return data;
}

/**
 * @brief 反序列化人脸特征描述子
 * @param data 数据库中的二进制数据
 * @return dlib::matrix<float,0,1> 反序列化后的特征向量
 * @description 从 QByteArray 恢复 Dlib 特征向量：
 *              1. 创建 QDataStream（读取模式）
 *              2. 读取向量大小
 *              3. 创建对应大小的 Dlib 矩阵
 *              4. 逐个读取浮点数值
 * @note 必须与 serializeDescriptor 使用相同的 Qt 版本
 */
dlib::matrix<float,0,1> FaceDbManager::deserializeDescriptor(const QByteArray &data)
{
    QByteArray tempData = data; // Create a non-const copy
    QDataStream stream(&tempData, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_5_6);
    int size;
    stream >> size;
    dlib::matrix<float,0,1> descriptor(size);
    for (int i = 0; i < size; ++i) {
        stream >> descriptor(i);
    }
    return descriptor;
}

/**
 * @brief 保存人脸记录到数据库
 * @param id 人脸 ID
 * @param descriptor 人脸特征描述子（Dlib 矩阵）
 * @param previewBase64 预览图数据（JPEG 格式的 QByteArray）
 * @return bool 成功返回 true，失败返回 false
 * @description 将人脸信息保存到数据库：
 *              1. 检查数据库连接
 *              2. 准备 INSERT OR REPLACE 语句
 *              3. 绑定参数（id、descriptor、preview）
 *              4. 执行查询
 * @note 使用 INSERT OR REPLACE 实现覆盖保存
 */
bool FaceDbManager::saveFace(const QString &id, const dlib::matrix<float,0,1> &descriptor, const QByteArray &previewBase64)
{
    if (!m_db.isOpen()) {
        qCritical() << "数据库未打开";
        return false;
    }

    QSqlQuery query(m_db);
//    query.prepare(R"(
//        INSERT OR REPLACE INTO face_registry (id, descriptor, preview)
//        VALUES (:id, :descriptor, :preview)
//    )");
        query.prepare("INSERT OR REPLACE INTO face_registry (id, descriptor, preview) VALUES (:id, :descriptor, :preview)");
    query.bindValue(":id", id);
    query.bindValue(":descriptor", serializeDescriptor(descriptor));
    query.bindValue(":preview", previewBase64);

    if (!query.exec()) {
        qCritical() << "保存人脸失败：" << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief 从数据库删除人脸记录
 * @param id 要删除的人脸 ID
 * @return bool 成功返回 true，失败返回 false
 * @description 从数据库中删除指定 ID 的人脸记录：
 *              1. 检查数据库连接
 *              2. 准备 DELETE 语句
 *              3. 绑定 ID 参数
 *              4. 执行查询
 * @note 删除操作不可恢复
 */
bool FaceDbManager::deleteFace(const QString &id)
{
    if (!m_db.isOpen()) {
        qCritical() << "数据库未打开";
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM face_registry WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qCritical() << "删除人脸失败：" << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief 获取所有人脸记录
 * @return QList<FaceRecord> 人脸记录列表
 * @description 查询数据库中所有的人脸记录：
 *              1. 检查数据库连接
 *              2. 执行 SELECT 查询（id, name, identity, preview, create_time）
 *              3. 遍历结果集构建 FaceRecord 对象
 *              4. 返回记录列表
 * @note FaceRecord 结构体包含：id, name, identity, previewData, createTime
 */
QList<FaceRecord> FaceDbManager::getAllFaces()
{
    QList<FaceRecord> records;
    if (!m_db.isOpen()) {
        qCritical() << "数据库未打开";
        return records;
    }

    QSqlQuery query("SELECT id, name, identity, preview, create_time FROM face_registry", m_db);
        while (query.next()) {
        FaceRecord record;

        record.id = query.value("id").toString();          // 改用字段名取值，更健壮
           record.name = query.value("name").toString();      // 新增：读取姓名
           record.identity = query.value("identity").toString(); // 新增：读取身份
           record.previewData = query.value("preview").toByteArray(); // 读取预览图
           record.createTime = query.value("create_time").toDateTime().toString("yyyy-MM-dd HH:mm:ss");

        records.append(record);
    }

    return records;
}

/**
 * @brief 清空所有人脸记录
 * @return bool 成功返回 true，失败返回 false
 * @description 删除数据库中所有的人脸数据：
 *              1. 检查数据库连接
 *              2. 执行 DELETE FROM face_registry（不带 WHERE 条件）
 *              3. 检查执行结果
 * @note 危险操作，调用前需要二次确认
 */
bool FaceDbManager::clearAllFaces()
{
    if (!m_db.isOpen()) {
        qCritical() << "数据库未打开";
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM face_registry")) {
        qCritical() << "清空数据库失败：" << query.lastError().text();
        return false;
    }
    return true;
}

/**
 * @brief 批量加载所有人脸特征
 * @param descriptors 输出参数，存储所有人脸特征向量
 * @param ids 输出参数，存储所有人脸 ID
 * @param names 输出参数，存储所有人脸姓名
 * @param identities 输出参数，存储所有人脸身份
 * @return bool 成功返回 true，失败返回 false
 * @description 从数据库加载所有人脸特征到内存（用于识别匹配）：
 *              1. 检查数据库连接
 *              2. 查询 descriptor, id, name, identity 字段
 *              3. 遍历结果集
 *              4. 反序列化 descriptor
 *              5. 添加到各个 vector 中
 *              6. 统计加载数量
 * @note 此方法在系统启动时调用，将人脸特征加载到内存供识别使用
 */
bool FaceDbManager::loadAllFaces(std::vector<dlib::matrix<float,0,1>>& descriptors, 
                                  std::vector<QString>& ids,
                                  std::vector<QString>& names,
                                  std::vector<QString>& identities)
{
    if (!m_db.isOpen()) {
        qCritical() << "数据库未打开";
        return false;
    }
    
    QSqlQuery query("SELECT descriptor, id, name, identity FROM face_registry", m_db);
    
    int loadedCount = 0;
    while (query.next()) {
        try {
            QString id = query.value("id").toString();
            QString name = query.value("name").toString();
            QString identity = query.value("identity").toString();
            QByteArray data = query.value("descriptor").toByteArray();
            
            dlib::matrix<float,0,1> descriptor = deserializeDescriptor(data);
            
            descriptors.push_back(descriptor);
            ids.push_back(id);
            names.push_back(name);
            identities.push_back(identity);
            loadedCount++;
        } catch (const std::exception& e) {
            qWarning() << "加载人脸 [" << query.value("id").toString() << "] 失败：" << e.what();
            continue;
        }
    }
    
    qDebug() << "从数据库加载人脸数：" << loadedCount;
    return true;
}
