/******************************************************************
* @projectName   video_client
* @brief         faceservice.cpp - 人脸识别服务实现
* @author        Lu Yaolong
* 
* @description   本文件实现了人脸识别服务层，封装了 FaceRecognizerThread 的复杂性
*                提供简单的接口供 MainWindow 调用
* 
* @design_pattern  Facade 模式（外观模式）
*                 - 对外提供简化的接口
*                 - 内部封装多线程、模型加载等复杂逻辑
*                 - 降低调用者的使用难度
*******************************************************************/
#include "faceservice.h"
#include <QDebug>
#include <QMessageBox>

/**
 * @brief FaceService 构造函数
 * 
 * @param parent 父对象指针
 * 
 * @description 初始化人脸识别服务：
 *              - 初始化人脸线程指针为 nullptr
 *              - 设置模型加载标志为 false
 * 
 * @note 此时还未创建实际的人脸识别线程
 */
FaceService::FaceService(QObject *parent)
    : QObject(parent)
    , m_faceThread(nullptr)
    , m_isFaceModelLoaded(false)
{
}

/**
 * @brief FaceService 析构函数
 * 
 * @description 清理人脸识别资源：
 *              1. 检查人脸线程是否存在
 *              2. 如果线程正在运行：
 *                 - 调用 quit() 请求退出
 *                 - 调用 wait() 等待线程结束
 *              3. 删除线程对象
 * 
 * @note 必须先停止线程再删除，避免资源泄漏
 *       Qt 的对象树机制不会自动删除 QThread 对象
 */
FaceService::~FaceService()
{
    if (m_faceThread) {
        if (m_faceThread->isRunning()) {
            m_faceThread->quit();
            m_faceThread->wait();
        }
        delete m_faceThread;
    }
}

/**
 * @brief 初始化人脸识别服务
 * 
 * @param landmarkModelPath 人脸关键点模型路径（shape_predictor_68_face_landmarks.dat）
 * @param recognitionModelPath 人脸识别模型路径（dlib_face_recognition_resnet_model_v1.dat）
 * 
 * @description 完成人脸识别服务的初始化：
 *              1. 创建 FaceRecognizerThread 对象
 *              2. 连接信号槽（3 个核心信号）：
 *                 - faceDataProcessed: 人脸数据处理完成
 *                 - modelsLoaded: 模型加载完成
 *                 - faceRecognizedSuccess: 人脸识别成功
 *              3. 启动人脸线程
 *              4. 异步加载模型文件
 * 
 * @note 模型加载是异步的，加载完成后通过 modelsLoaded 信号通知
 *       两个模型文件约 130MB，首次加载需要几秒钟
 */
void FaceService::initialize(const QString& landmarkModelPath, const QString& recognitionModelPath)
{
    m_faceThread = new FaceRecognizerThread(this);
    
    connect(m_faceThread, &FaceRecognizerThread::faceDataProcessed,
            this, &FaceService::onFaceDataProcessed);
    
    connect(m_faceThread, &FaceRecognizerThread::modelsLoaded,
            this, &FaceService::onModelsLoaded);
    
    connect(m_faceThread, &FaceRecognizerThread::faceRecognizedSuccess,
            this, &FaceService::onFaceRecognizedSuccess);
    
    m_faceThread->start();
    m_faceThread->loadModels(landmarkModelPath, recognitionModelPath);
    
    qDebug() << "人脸识别服务初始化完成";
}

/**
 * @brief 处理视频帧
 * 
 * @param frame 输入的 OpenCV 图像（BGR 格式）
 * 
 * @description 将视频帧提交给人脸识别线程处理：
 *              1. 检查输入图像是否有效
 *              2. 检查人脸线程是否存在
 *              3. 调用 submitFrame() 提交帧
 * 
 * @note 这是一个异步操作，立即返回
 *       实际处理在 FaceRecognizerThread 中进行
 *       
 * @note 帧处理流程：
 *       人脸检测 -> 关键点定位 -> 人脸对齐 -> 特征提取 -> 识别匹配
 */
void FaceService::processFrame(const cv::Mat& frame)
{
    if (frame.empty() || !m_faceThread) {
        return;
    }
    
    m_faceThread->submitFrame(frame);
}

/**
 * @brief 在图像上绘制人脸数据
 * 
 * @param frame 输入输出图像（会在上面绘制人脸框和关键点）
 * @param faceDataList 人脸数据列表（包含人脸框、68 个关键点、128 维特征向量）
 * 
 * @description 在视频帧上可视化人脸检测结果：
 *              1. 检查模型是否加载完成
 *              2. 使用互斥锁保护共享数据（线程安全）
 *              3. 缓存人脸数据
 *              4. 遍历所有人脸：
 *                 - 绘制人脸矩形框（蓝色，线宽 2px）
 *                 - 绘制 68 个人脸关键点（红色实心圆点）
 * 
 * @note 绘制操作在主线程进行，避免阻塞识别线程
 *       使用 OpenCV 的绘图函数
 */
void FaceService::drawFaceData(cv::Mat& frame, const std::vector<FaceLandmarkData>& faceDataList)
{
    if (!m_isFaceModelLoaded) {
        return;
    }
    
    QMutexLocker locker(&m_faceDataMutex);
    m_cachedFaceData = faceDataList;
    
    for (const auto& faceData : m_cachedFaceData) {
        // 绘制人脸矩形框
        cv::rectangle(frame, faceData.faceRect, cv::Scalar(255, 0, 0), 2);
        // 绘制 68 个人脸关键点
        for (const auto& pt : faceData.landmarks) {
            cv::circle(frame, pt, 2, cv::Scalar(0, 0, 255), -1);
        }
    }
}

/**
 * @brief 人脸识别成功槽函数
 * 
 * @param result 识别结果（包含姓名、ID、身份、匹配距离）
 * 
 * @description 处理人脸识别成功事件：
 *              1. 转发 faceRecognizedSuccess 信号给 MainWindow
 *              2. 发射 recognitionResultReady 信号
 * 
 * @note 这个槽函数在 FaceRecognizerThread 线程中被调用
 *       信号会跨线程传递到主线程
 */
void FaceService::onFaceRecognizedSuccess(const RecognizeResult& result)
{
    emit faceRecognizedSuccess(result);
    emit recognitionResultReady(result);
}

/**
 * @brief 人脸数据处理完成槽函数
 * 
 * @param faceDataList 人脸数据列表
 * 
 * @description 转发人脸数据处理完成信号：
 *              直接转发给 MainWindow 用于绘制
 * 
 * @note 数据包含：
 *       - 人脸矩形框
 *       - 68 个关键点坐标
 *       - 128 维特征向量
 */
void FaceService::onFaceDataProcessed(const std::vector<FaceLandmarkData>& faceDataList)
{
    emit faceDataProcessed(faceDataList);
}

/**
 * @brief 模型加载完成槽函数
 * 
 * @param success 是否加载成功
 * @param errorMsg 错误信息（如果失败）
 * 
 * @description 处理模型加载完成事件：
 *              1. 更新模型加载标志
 *              2. 转发 modelsLoaded 信号给 MainWindow
 *              3. 打印调试日志
 * 
 * @note 模型加载成功后才能进行人脸识别
 *       如果失败会显示错误对话框提示用户
 */
void FaceService::onModelsLoaded(bool success, const QString& errorMsg)
{
    m_isFaceModelLoaded = success;
    emit modelsLoaded(success, errorMsg);
    
    if (success) {
        qDebug() << "人脸模型加载成功（FaceService）";
    } else {
        qCritical() << "人脸模型加载失败：" << errorMsg;
    }
}
