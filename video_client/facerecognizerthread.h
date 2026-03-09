// facerecognizerthread.h
#ifndef FACERECOGNIZERTHREAD_H
#define FACERECOGNIZERTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QFileInfo> // 新增：文件检查
#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/shape_predictor.h>
#include <dlib/dnn.h>
#include <dlib/clustering.h>
#include <vector>
#include <QString>
// 头文件
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QByteArray>
#include <QDataStream>
#include "facedbmanager.h"
#include <QImage>    // 解决QImage未定义
#include <QBuffer>   // 解决QBuffer未定义
#include <QIODevice> // 解决QIODevice::WriteOnly未定义
#include <QDateTime>
#include <QVariant>
// 定义人脸关键点数据结构
struct FaceLandmarkData {
    cv::Rect faceRect;                  // 人脸框
    std::vector<cv::Point> landmarks;   // 人脸关键点
    dlib::matrix<float,0,1> descriptor; // 128维特征向量
};
// 人脸库结构体：特征向量 + 身份标签
struct RegisteredFace {
    dlib::matrix<float,0,1> descriptor; // 人脸特征
    QString id; // 身份标识（如姓名、ID）
    QString name;
    QString identity;
    RegisteredFace(const dlib::matrix<float,0,1>& desc, const QString& id_, const QString& name_, const QString& identity_)
        : descriptor(desc), id(id_), name(name_), identity(identity_) {}
};
//识别结果结构体（承载ID+姓名+身份+匹配距离）
struct RecognizeResult {
    QString id = "未知";        // 匹配的ID
    QString name = "未知";      // 匹配的姓名
    QString identity = "未知";  // 匹配的身份
    double distance = -1.0;     // 匹配距离（-1表示无匹配）
    RecognizeResult() = default;
};

Q_DECLARE_METATYPE(RecognizeResult)

Q_DECLARE_METATYPE(FaceLandmarkData)
Q_DECLARE_METATYPE(std::vector<FaceLandmarkData>)
// 复制示例中的ResNet网络定义
template <template <int,template<typename>class,int,typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual = dlib::add_prev1<block<N,BN,1,dlib::tag1<SUBNET>>>;

template <template <int,template<typename>class,int,typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual_down = dlib::add_prev2<dlib::avg_pool<2,2,2,2,dlib::skip1<dlib::tag2<block<N,BN,2,dlib::tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET>
using block  = BN<dlib::con<N,3,3,1,1,dlib::relu<BN<dlib::con<N,3,3,stride,stride,SUBNET>>>>>;

template <int N, typename SUBNET> using ares      = dlib::relu<residual<block,N,dlib::affine,SUBNET>>;
template <int N, typename SUBNET> using ares_down = dlib::relu<residual_down<block,N,dlib::affine,SUBNET>>;

template <typename SUBNET> using alevel0 = ares_down<256,SUBNET>;
template <typename SUBNET> using alevel1 = ares<256,ares<256,ares_down<256,SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128,ares<128,ares_down<128,SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64,ares<64,ares<64,ares_down<64,SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32,ares<32,ares<32,SUBNET>>>;

using anet_type = dlib::loss_metric<dlib::fc_no_bias<128,dlib::avg_pool_everything<
                    alevel0<
                    alevel1<
                    alevel2<
                    alevel3<
                    alevel4<
                    dlib::max_pool<3,3,2,2,dlib::relu<dlib::affine<dlib::con<32,7,7,2,2,
                    dlib::input_rgb_image_sized<150>
                    >>>>>>>>>>>>;
class FaceRecognizerThread : public QThread
{
    Q_OBJECT
public:
  explicit FaceRecognizerThread(QObject *parent = nullptr);
    ~FaceRecognizerThread() override;

      void clearFaceLib();
      RecognizeResult CalculateDistance(const dlib::matrix<float,0,1>& descriptor);

    // 提交待处理的帧（线程安全）
    void submitFrame(const cv::Mat &frame);
    // 加载人脸模型（线程安全）
    void loadModels(const QString &shapePredictorPath, const QString &recognitionModelPath);
    
    // 公开访问方法（用于 FaceService）
    dlib::frontal_face_detector& detector() { return m_detector; }
    dlib::shape_predictor& faceShapePredictor() { return m_shapePredictor; }
    bool isModelsLoaded() const { return m_isModelsLoaded; }

public slots:
      // 从数据库删除指定ID人脸
      bool deleteFaceFromDb(const QString& id);
      // 清空数据库+内存人脸库
      bool clearFaceLibFromDb();
      // 通过图片路径注册人脸（主程序直接调用这个）
      bool registerFaceByImagePath(const QString& imagePath, QVariantHash id);

signals:

    void faceRecognizedSuccess(RecognizeResult res);
    void operateFinished(bool success, const QString& msg);
    void faceDataProcessed(const std::vector<FaceLandmarkData> &faceData);
    void modelsLoaded(bool success, const QString &errorMsg);
    void recognizeResult(const std::vector<QString>& identities);

protected:
    void run() override;

private:
    qint64 oldtime=0;
    // 加载数据库中所有人脸到内存
   // bool loadFacesFromDb();
        QSqlDatabase m_db;          // SQLite数据库对象
        bool initDatabase();        // 初始化数据库
        bool loadDescriptorsFromDb(); // 从库加载所有特征到内存
        QByteArray serializeDescriptor(const dlib::matrix<float,0,1>& descriptor); // 特征序列化
        dlib::matrix<float,0,1> deserializeDescriptor(const QByteArray& data); // 特征反序列化

    // 人脸库（线程安全）
    QMutex m_faceLibMutex;
    std::vector<RegisteredFace> m_registeredFaces;
    // 识别阈值（可调整，建议0.6，越小越严格）
    const double RECOGNITION_THRESHOLD = 0.6;
    // 人脸图像对齐（生成150x150的标准化人脸）
       dlib::matrix<dlib::rgb_pixel> alignFace(const dlib::cv_image<dlib::rgb_pixel>& dlibImage, const dlib::rectangle& face, const dlib::shape_predictor& sp);
       // 图像抖动增强（提升特征提取精度）
       std::vector<dlib::matrix<dlib::rgb_pixel>> jitterImage(const dlib::matrix<dlib::rgb_pixel>& img);
       // 人脸聚类
       void recognize(std::vector<FaceLandmarkData>& faceDataList);
       QString m_shapePredictorPath;
        QString m_recognitionModelPath;

    anet_type m_recognitionNet;             // 特征提取网络
    QMutex m_mutex;
    QWaitCondition m_condition;
    cv::Mat m_pendingFrame;  // 待处理的帧
    QString m_modelPath;     // 模型路径
    dlib::frontal_face_detector m_detector;
    dlib::shape_predictor m_shapePredictor;
    bool m_isModelsLoaded = false;
    bool m_isRunning = true;  // 线程运行标志
    bool m_hasNewFrame = false; // 是否有新帧待处理
    bool m_needLoadModels = false; // 是否需要加载模型
    int frameCount = 0; // 帧计数（用于跳帧）
    const int sampleInterval = 2; // 每2帧处理一次
};

#endif // FACERECOGNIZERTHREAD_H
