/******************************************************************
* @projectName   video_client
* @brief         faceservice.h - 人脸识别服务头文件
* @author        Lu Yaolong
* 
* @description   人脸识别服务：封装人脸识别功能，提供简单的接口
* 
* @architecture  服务层设计模式：
*                - 对外提供简单的 processFrame() 接口
*                - 内部封装 FaceRecognizerThread 的复杂性
*                - 作为 MainWindow 和 FaceRecognizerThread 之间的桥梁
*******************************************************************/
#ifndef FACESERVICE_H
#define FACESERVICE_H

#include <QObject>
#include <QMutex>
#include <QVector>
#include <QImage>
#include <opencv2/opencv.hpp>
#include "facerecognizerthread.h"

/**
 * @brief 人脸识别服务类
 * 
 * @description 封装人脸识别的完整流程，包括：
 *              - 模型加载
 *              - 帧处理
 *              - 人脸检测和识别
 *              - 结果通知
 * 
 * @note 采用Facade 模式，简化外部调用
 */
class FaceService : public QObject
{
    Q_OBJECT

public:
    explicit FaceService(QObject *parent = nullptr);
    ~FaceService();

    /**
     * @brief 初始化人脸识别服务
     * @param landmarkModelPath 人脸关键点模型路径
     * @param recognitionModelPath 人脸识别模型路径
     */
    void initialize(const QString& landmarkModelPath, const QString& recognitionModelPath);
    
    /**
     * @brief 处理视频帧
     * @param frame 输入的 OpenCV 图像（BGR 格式）
     */
    void processFrame(const cv::Mat& frame);
    
    /**
     * @brief 在图像上绘制人脸数据
     * @param frame 输入输出图像（会绘制人脸框和关键点）
     * @param faceDataList 人脸数据列表（包含人脸框、关键点、特征向量）
     */
    void drawFaceData(cv::Mat& frame, const std::vector<FaceLandmarkData>& faceDataList);
    
    bool isModelLoaded() const { return m_isFaceModelLoaded; }  ///< 检查模型是否加载完成
    FaceRecognizerThread* getFaceThread() const { return m_faceThread; }  ///< 获取人脸识别线程指针

signals:
    void faceRecognizedSuccess(const RecognizeResult& result);  ///< 人脸识别成功信号
    void faceDataProcessed(const std::vector<FaceLandmarkData>& faceDataList);  ///< 人脸数据处理完成信号
    void modelsLoaded(bool success, const QString& errorMsg);  ///< 模型加载完成信号
    void recognitionResultReady(const RecognizeResult& result);  ///< 识别结果就绪信号

public slots:
    void onFaceRecognizedSuccess(const RecognizeResult& result);  ///< 人脸识别成功槽
    void onFaceDataProcessed(const std::vector<FaceLandmarkData>& faceDataList);  ///< 人脸数据处理完成槽
    void onModelsLoaded(bool success, const QString& errorMsg);  ///< 模型加载完成槽

private:
    FaceRecognizerThread* m_faceThread;  ///< 人脸识别线程指针
    QMutex m_faceDataMutex;  ///< 人脸数据互斥锁
    std::vector<FaceLandmarkData> m_cachedFaceData;  ///< 缓存的人脸数据
    bool m_isFaceModelLoaded;  ///< 模型加载标志
};

#endif // FACESERVICE_H
