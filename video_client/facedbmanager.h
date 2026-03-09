// facedbmanager.h
#ifndef FACEDBMANAGER_H
#define FACEDBMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QByteArray>
#include <dlib/matrix.h>

// 人脸数据结构体（用于界面展示）
 struct FaceRecord {
     QString id;
     QString name;
     QString identity;
     QByteArray previewData;
     QString createTime;
 };
class FaceDbManager : public QObject
{
    Q_OBJECT
public:
    QSqlDatabase getDbConnection() const {
        return m_db;
    }
    static FaceDbManager* getInstance(); // 单例模式，避免多连接

    // 序列化/反序列化特征（改为静态工具函数）
    static QByteArray serializeDescriptor(const dlib::matrix<float,0,1>& descriptor);
    static dlib::matrix<float,0,1> deserializeDescriptor(const QByteArray &data);

    bool isFaceExists(const QString& id);

    // 初始化数据库
    bool initDb(const QString& dbPath = "./face_db.db");
    // 保存人脸（含特征 + 预览图）
    bool saveFace(const QString& id, const dlib::matrix<float,0,1>& descriptor, const QByteArray& previewBase64);
    // 删除指定 ID 人脸
    bool deleteFace(const QString& id);
    // 查询所有人脸记录
    QList<FaceRecord> getAllFaces();
    // 清空数据库
    bool clearAllFaces();
    // 加载所有人脸特征到内存
    bool loadAllFaces(std::vector<dlib::matrix<float,0,1>>& descriptors, 
                      std::vector<QString>& ids,
                      std::vector<QString>& names,
                      std::vector<QString>& identities);

    QSqlDatabase m_db;
    static FaceDbManager* m_instance;
private:
    explicit FaceDbManager(QObject *parent = nullptr);
    ~FaceDbManager();
    FaceDbManager(const FaceDbManager&) = delete;
    FaceDbManager& operator=(const FaceDbManager&) = delete;

};

#endif // FACEDBMANAGER_H
