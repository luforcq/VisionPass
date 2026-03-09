/******************************************************************
* @projectName   video_client
* @brief         facerecognizerthread.cpp - 人脸识别线程实现
* @author        Lu Yaolong
* 
* @description   本文件实现了人脸识别线程，在独立线程中运行人脸识别算法
* 
* @responsibilities
*                - 加载人脸检测和识别模型（Dlib）
*                - 处理视频帧（人脸检测、关键点定位、特征提取）
*                - 人脸识别匹配（计算欧氏距离）
*                - 人脸库管理（注册、删除、清空）
* 
* @thread_design   生产者 - 消费者模式
*                 - 主线程：提交视频帧（生产者）
*                 - 识别线程：处理帧并识别（消费者）
*                 - 使用 QWaitCondition 同步
* 
* @performance     性能优化
*                 - 跳帧处理（每 2 帧处理 1 次）
*                 - 降分辨率（640x480）
*                 - 内存人脸库（启动时加载）
*******************************************************************/
#include "facerecognizerthread.h"
#include "faceutils.h"
#include <QDebug>
#include <QElapsedTimer> // 用于高精度计时
#include <QFileInfo>
#include <dlib/image_io.h>
#include <dlib/opencv.h>

/**
 * @brief FaceRecognizerThread 构造函数
 * 
 * @param parent 父对象指针
 * 
 * @description 初始化人脸识别线程：
 *              1. 初始化人脸检测器（HOG + SVM 前向检测器）
 *              2. 初始化人脸数据库管理器
 *              3. 从数据库加载已注册的人脸特征
 * 
 * @note 此时还未加载深度学习模型
 *       模型在 loadModels() 中异步加载
 */
FaceRecognizerThread::FaceRecognizerThread(QObject *parent) : QThread(parent)
{
    m_detector = dlib::get_frontal_face_detector();
    
    // 统一由 FaceDbManager 初始化数据库
    auto dbMgr = FaceDbManager::getInstance();
    if (dbMgr->initDb()) {
        loadDescriptorsFromDb();
    }
}

/**
 * @brief FaceRecognizerThread 析构函数
 * 
 * @description 清理线程资源：
 *              1. 加锁保护
 *              2. 设置停止标志（m_isRunning = false）
 *              3. 唤醒线程（wakeOne）避免死锁
 *              4. 解锁
 *              5. 等待线程结束（wait）
 *              6. 关闭数据库连接
 * 
 * @note 必须先设置标志再唤醒，避免线程卡死
 *       wait() 会阻塞直到 run() 退出
 */
FaceRecognizerThread::~FaceRecognizerThread()
{
    // 停止线程
    m_mutex.lock();
    m_isRunning = false;
    m_condition.wakeOne();
    m_mutex.unlock();
    
    wait();
    
    if (m_db.isOpen()) {
        m_db.close();
    }
}

/**
 * @brief 从数据库删除指定人脸
 * 
 * @param id 人脸 ID
 * @return bool 删除是否成功
 * 
 * @description 删除人脸的完整流程：
 *              1. 从数据库删除记录（FaceDbManager::deleteFace）
 *              2. 从内存人脸库删除（加锁保护）
 *                 - 遍历 m_registeredFaces
 *                 - 查找匹配的 ID
 *                 - 使用 erase() 删除
 *              3. 打印调试日志
 * 
 * @note 需要同时删除数据库和内存中的数据
 *       使用迭代器查找并删除
 */
bool FaceRecognizerThread::deleteFaceFromDb(const QString &id)
{
    // 1. 删除数据库中的记录
    auto dbMgr = FaceDbManager::getInstance();
    if (!dbMgr->deleteFace(id)) {
        return false;
    }
    // 2. 删除内存中的记录
    QMutexLocker locker(&m_faceLibMutex);
    for (auto it = m_registeredFaces.begin(); it != m_registeredFaces.end(); ++it) {
        if (it->id == id) {
            m_registeredFaces.erase(it);
            break;
        }
    }// ← 锁结束
    qDebug() << "删除人脸：" << id << "，当前库中总数：" << m_registeredFaces.size();
    return true;
}

/**
 * @brief 从数据库清空整个人脸库
 * 
 * @return bool 清空是否成功
 * 
 * @description 清空人脸库的完整流程：
 *              1. 清空数据库（FaceDbManager::clearAllFaces）
 *              2. 清空内存人脸库（clearFaceLib）
 * 
 * @note 先清空数据库，再清空内存
 *       确保数据一致性
 */
bool FaceRecognizerThread::clearFaceLibFromDb()
{
    // 1. 清空数据库
    auto dbMgr = FaceDbManager::getInstance();
    if (!dbMgr->clearAllFaces()) {
        return false;
    }
    // 2. 清空内存
    clearFaceLib();
    return true;
}

/**
 * @brief 从数据库加载所有人脸特征到内存
 * 
 * @return bool 加载是否成功
 * 
 * @description 程序启动时加载人脸库：
 *              1. 检查数据库是否打开
 *              2. 执行 SQL 查询所有记录
 *              3. 加锁保护内存人脸库（QMutexLocker）
 *              4. 遍历查询结果：
 *                 - 获取 ID、姓名、身份
 *                 - 读取特征向量（BLOB）
 *                 - 反序列化为 dlib 矩阵
 *                 - 插入 m_registeredFaces
 *                 - 异常处理（跳过坏数据）
 *              5. 打印加载日志
 * 
 * @note 使用 QMutexLocker 自动管理锁
 *       批量加载只加一次锁，提高效率
 *       启动时内存为空，无需检查重复
 */
bool FaceRecognizerThread::loadDescriptorsFromDb()
{
    if (!m_db.isOpen()) {
        qCritical() << "数据库未打开，加载失败";
        return false;
    }
    QSqlQuery query("SELECT descriptor,id, name, identity, preview, create_time FROM face_registry", m_db);

    QMutexLocker locker(&m_faceLibMutex); // 批量加载只加一次锁 这个锁保护的是 m_registeredFaces，在头文件里定义的 
    int loadedCount = 0;
    // 遍历查询结果，逐条加载
    while (query.next()) {
        try { // 捕获单条数据异常，不影响整体
            QString id = query.value("id").toString();
            QString name=query.value("name").toString();
            QString identity=query.value("identity").toString();
            QByteArray data = query.value("descriptor").toByteArray();
            dlib::matrix<float,0,1> descriptor = FaceDbManager::deserializeDescriptor(data);
            // 直接插入，无需检查 id（启动时内存为空）
            m_registeredFaces.emplace_back(descriptor, id,name,identity);
            loadedCount++;
        } catch (const std::exception& e) {
            qWarning() << "加载人脸 [" << query.value(0).toString() << "] 失败：" << e.what();
            continue; // 跳过坏数据，继续加载其余数据
        }
    }

    qDebug() << "启动加载人脸数：" << loadedCount;
    return true; // ← 锁在这里结束（locker 析构自动解锁）
}

/**
 * @brief 根据图片路径注册新人脸
 * 
 * @param imagePath 图片文件路径
 * @param fileData 人脸信息（name, id, identity）
 * @return bool 注册是否成功
 * 
 * @description 完成人脸注册的完整流程：
 *              1. 检查模型是否加载完成
 *              2. 加载图片（OpenCV imread）
 *              3. 转换为 dlib 格式并检测人脸：
 *                 - cv_image 包装
 *                 - HOG + SVM 检测
 *              4. 提取第一张人脸的特征：
 *                 - 人脸对齐（alignFace）
 *                 - ResNet 提取 128 维特征
 *              5. 截取人脸预览图
 *              6. 保存到数据库：
 *                 - ID、姓名、身份
 *                 - 特征向量（序列化）
 *                 - 预览图（JPG 压缩）
 *              7. 添加到内存人脸库
 * 
 * @note 多张人脸时只取第一张
 *       预览图用于界面显示
 *       使用 INSERT OR REPLACE 避免重复
 */
bool FaceRecognizerThread::registerFaceByImagePath(const QString& imagePath, QVariantHash fileData)
{
    QString name = fileData["name"].toString();
    QString id = fileData["id"].toString();
    QString identity = fileData["identity"].toString();
    // 1. 校验模型是否加载完成
    if (!m_isModelsLoaded) {
        qCritical() << "模型未加载完成，无法注册人脸！";
        return false;
    }

    // 2. 加载图片
    cv::Mat frame = cv::imread(imagePath.toStdString());
    if (frame.empty()) {
        qCritical() << "图片加载失败：" << imagePath;
        return false;
    }

    // 3. 转换为 dlib 格式，检测人脸
    dlib::cv_image<dlib::rgb_pixel> dlibImage(frame);
    std::vector<dlib::rectangle> faces = m_detector(dlibImage);
    if (faces.empty()) {
        qCritical() << "图片中未检测到人脸：" << imagePath;
        return false;
    }
    if (faces.size() > 1) {
        qWarning() << "图片中检测到多张人脸，仅取第一张进行注册：" << imagePath;
    }

    // 4. 提取第一张人脸的特征
    dlib::rectangle face = faces[0];
    dlib::matrix<dlib::rgb_pixel> faceChip = alignFace(dlibImage, face, m_shapePredictor);
    dlib::matrix<float,0,1> descriptor = m_recognitionNet(faceChip);

    // 声明 faceData 变量（原代码缺失）
    FaceLandmarkData faceData;
    faceData.faceRect = cv::Rect(face.left(), face.top(), face.width(), face.height()); // 从检测到的 face 赋值

    // 截取人脸预览图并使用 FaceUtils 工具函数转换
    cv::Mat facePreview = frame(faceData.faceRect).clone();
    QByteArray previewData = FaceUtils::matToData(facePreview, "JPG");

    // 修复：SQL 语句绑定原始二进制（推荐，减少解码步骤）
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO face_registry
        (id, name, identity, descriptor, preview, create_time)
        VALUES (:id, :name, :identity, :descriptor, :preview, :create_time)
    )");
    query.bindValue(":id", id);
        query.bindValue(":name", name);       // 新增：绑定名字
        query.bindValue(":identity", identity); // 新增：绑定身份
    query.bindValue(":descriptor", FaceDbManager::serializeDescriptor(descriptor));
    query.bindValue(":preview", previewData); // 绑定原始二进制
    query.bindValue(":create_time", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    if (!query.exec()) {
        qCritical() << "保存人脸（含预览）失败：" << query.lastError().text();
        return false;
    }

    // 原有注册逻辑
    m_registeredFaces.emplace_back(descriptor, id,name,identity);

    return true;
}

/**
 * @brief 清空人脸库
 * 
 * @description 清空内存中的人脸数据：
 *              1. 加锁保护（QMutexLocker）
 *              2. 调用 clear() 清空向量
 *              3. 打印日志
 * 
 * @note 只清空内存，不清空数据库
 *       需要配合 clearFaceLibFromDb() 使用
 */
void FaceRecognizerThread::clearFaceLib()
{
    QMutexLocker locker(&m_faceLibMutex);
    m_registeredFaces.clear();
    qDebug() << "人脸库已清空";
}

/**
 * @brief 计算人脸特征距离并识别身份
 * 
 * @param descriptor 待识别的人脸特征向量（128 维）
 * @return RecognizeResult 识别结果（ID、姓名、身份、距离）
 * 
 * @description 人脸识别的核心算法：
 *              1. 初始化结果结构体
 *              2. 加锁保护人脸库
 *              3. 检查人脸库是否为空
 *              4. 遍历所有已注册人脸：
 *                 - 计算欧氏距离：length(descriptor - registered)
 *                 - 记录最小距离及其对应的人脸
 *                 - 距离 < 阈值（0.6）才认为匹配成功
 *              5. 返回最佳匹配结果
 * 
 * @return 识别结果
 *         - 成功：distance >= 0，包含 ID、姓名等信息
 *         - 失败：distance = -1，表示未匹配到
 * 
 * @note 使用欧氏距离衡量相似度
 *       阈值越小越严格（误识率↓，误拒率↑）
 *       RECOGNITION_THRESHOLD = 0.6
 */
RecognizeResult FaceRecognizerThread::CalculateDistance(const dlib::matrix<float,0,1>& descriptor)
{
    RecognizeResult result;
    QMutexLocker locker(&m_faceLibMutex);
    if (m_registeredFaces.empty()) {
        return result;
    }

    double minDist = 1e9; // 最小距离（初始极大值）

    for (const auto& regFace : m_registeredFaces) {
        double dist =dlib::length(descriptor - regFace.descriptor);
        if (dist < minDist && dist < RECOGNITION_THRESHOLD) {
            minDist = dist;
            result.id = regFace.id;
            result.name=regFace.name;
            result.identity=regFace.identity;
            result.distance=dist;
        }
    }

     return result;
}

/**
 * @brief 提交新视频帧进行处理
 * 
 * @param frame 视频帧（OpenCV Mat）
 * 
 * @description 主线程调用此函数提交帧：
 *              1. 加锁（QMutexLocker）
 *              2. 帧计数器递增
 *              3. 跳帧检测（每 sampleInterval 帧处理一次）
 *              4. 克隆帧数据（避免原数据被覆盖）
 *              5. 设置新帧标志（m_hasNewFrame = true）
 *              6. 唤醒线程（wakeOne）
 * 
 * @note 使用跳帧策略降低 CPU 占用
 *       默认每 2 帧处理一次（15fps）
 *       clone() 确保数据安全
 */
void FaceRecognizerThread::submitFrame(const cv::Mat &frame)
{
    QMutexLocker locker(&m_mutex);
    frameCount++;
    if (frameCount % sampleInterval != 0) {
        return; // 跳帧，不处理
    }
    m_pendingFrame = frame.clone(); // 克隆帧，避免原数据被覆盖
    m_hasNewFrame = true;
    m_condition.wakeOne(); // 唤醒线程处理新帧
}

/**
 * @brief 加载人脸识别模型
 * 
 * @param shapePredictorPath 关键点模型路径
 * @param recognitionModelPath 特征提取模型路径
 * 
 * @description 异步加载深度学习模型：
 *              1. 加锁
 *              2. 转换为绝对路径
 *              3. 打印路径（调试用）
 *              4. 检查文件是否存在
 *              5. 设置加载标志（m_needLoadModels = true）
 *              6. 保存路径
 *              7. 唤醒线程
 * 
 * @note 实际加载在 run() 中进行
 *       这里是触发加载命令
 *       文件不存在时立即发射错误信号
 */
void FaceRecognizerThread::loadModels(const QString &shapePredictorPath, const QString &recognitionModelPath)
{
    QMutexLocker locker(&m_mutex);
    
    // 打印绝对路径，方便调试
    QString absShapePath = QFileInfo(shapePredictorPath).absoluteFilePath();
    QString absRecPath = QFileInfo(recognitionModelPath).absoluteFilePath();
    qDebug() << "模型文件绝对路径:";
    qDebug() << "  关键点模型:" << absShapePath;
    qDebug() << "  特征模型:" << absRecPath;
    
    // 检查文件是否存在
    QFileInfo spFile(shapePredictorPath);
    QFileInfo recFile(recognitionModelPath);
    if (!spFile.exists() || !spFile.isFile()) {
        emit modelsLoaded(false, "关键点模型文件不存在：" + absShapePath);
        return;
    }
    if (!recFile.exists() || !recFile.isFile()) {
        emit modelsLoaded(false, "特征提取模型文件不存在：" + absRecPath);
        return;
    }
    m_shapePredictorPath = shapePredictorPath;
    m_recognitionModelPath = recognitionModelPath;
    m_needLoadModels = true;
    m_condition.wakeOne();
}

/**
 * @brief 人脸对齐（提取标准化人脸）
 * 
 * @param dlibImage 输入图像（dlib 格式）
 * @param face 人脸矩形框
 * @param sp 关键点预测模型
 * @return dlib::matrix<dlib::rgb_pixel> 150x150 标准化人脸
 * 
 * @description 人脸对齐标准化：
 *              1. 提取 68 个人脸关键点
 *              2. 计算仿射变换矩阵
 *              3. 裁剪并旋转到标准位置
 *              4. 输出 150x150 图像
 * 
 * @note 对齐消除姿态影响
 *       提升识别准确率
 *       get_face_chip_details 计算变换矩阵
 */
dlib::matrix<dlib::rgb_pixel> FaceRecognizerThread::alignFace(const dlib::cv_image<dlib::rgb_pixel>& dlibImage, const dlib::rectangle& face, const dlib::shape_predictor& sp)
{
    // 提取人脸关键点并对齐为 150x150 的标准化图像
    auto shape = sp(dlibImage, face);
    dlib::matrix<dlib::rgb_pixel> faceChip;
    dlib::extract_image_chip(dlibImage, dlib::get_face_chip_details(shape, 150, 0.25), faceChip);
    return faceChip;
}

/**
 * @brief 图像抖动增强
 * 
 * @param img 输入图像
 * @return std::vector<dlib::matrix<dlib::rgb_pixel>> 100 张增强图像
 * 
 * @description 数据增强技术：
 *              生成 100 张略有差异的图像
 *              提升特征提取的鲁棒性
 * 
 * @note 使用 thread_local 避免重复创建随机数生成器
 *       抖动包括：平移、缩放、旋转
 *       用于训练阶段，推理阶段未使用
 */
std::vector<dlib::matrix<dlib::rgb_pixel>> FaceRecognizerThread::jitterImage(const dlib::matrix<dlib::rgb_pixel>& img)
{
    // 图像抖动增强，生成 100 张略有差异的图像（提升特征精度）
    thread_local dlib::rand rnd;
    std::vector<dlib::matrix<dlib::rgb_pixel>> crops;
    for (int i = 0; i < 100; ++i) {
        crops.push_back(dlib::jitter_image(img, rnd));
    }
    return crops;
}

/**
 * @brief 识别人脸特征
 * 
 * @param faceDataList 人脸数据列表（包含特征向量）
 * 
 * @description 对多张人脸进行识别：
 *              1. 检查列表是否为空
 *              2. 初始化结果容器
 *              3. 遍历每个人脸：
 *                 - 调用 CalculateDistance() 识别
 *                 - 记录识别结果（ID、姓名、身份）
 *                 - 防抖处理（3 秒内不重复通知）
 *                 - 打印调试日志
 *              4. 发射识别成功信号
 *              5. 发射识别结果信号
 * 
 * @note 防抖避免重复开锁
 *       使用 3 秒时间窗口
 *       oldtime 记录上次通知时间
 */
void FaceRecognizerThread::recognize(std::vector<FaceLandmarkData>& faceDataList)
{
    if (faceDataList.empty()) return;

    // 初始化结果容器
    std::vector<QString> identities(faceDataList.size());
    std::vector<QString> names(faceDataList.size());
    std::vector<QString> identityTypes(faceDataList.size());
    // 遍历每个人脸→识别身份
    for (size_t i = 0; i < faceDataList.size(); ++i) {
        auto& faceData = faceDataList[i];
        // 跳过无效特征
        if (faceData.descriptor.size() == 0) {
            identities[i] = "无效特征";
            continue;
        }
        // 调用识别函数（需先实现该函数 + 人脸库）
        RecognizeResult res  = CalculateDistance(faceData.descriptor);
        identities[i] = res.id;
        names[i] = res.name;
        identityTypes[i] = res.identity;
        // 打印识别日志
        if (res.distance >= 0) {
            qint64 currenttime = QDateTime::currentMSecsSinceEpoch();//毫秒
            if (currenttime-oldtime>3*1000){
                oldtime =currenttime;
            emit faceRecognizedSuccess(res);
            }
            qDebug() << "人脸" << i << "识别结果："<< res.name << res.id <<res.identity<< "，匹配距离：" << res.distance;
        } else {
            qDebug() << "人脸" << i << "未匹配到已知身份";
        }
    }

    emit recognizeResult(identities);
}

/**
 * @brief 线程主函数（生产者 - 消费者模式）
 * 
 * @description 人脸识别线程的主循环：
 *              1. 检查运行标志
 *              2. 加锁
 *              3. 等待条件（新帧或加载模型）
 *              4. 处理任务：
 *                 - 加载模型：
 *                   a. 加载关键点模型（shape_predictor_68_face_landmarks.dat）
 *                   b. 加载识别模型（dlib_face_recognition_resnet_model_v1.dat）
 *                   c. 发射 modelsLoaded 信号
 *                 - 处理帧：
 *                   a. 降分辨率（640x480）
 *                   b. 人脸检测（HOG + SVM）
 *                   c. 提取关键点（68 个）
 *                   d. 人脸对齐（150x150）
 *                   e. 特征提取（128 维）
 *                   f. 识别匹配（CalculateDistance）
 *                   g. 发射处理完成信号
 *              5. 解锁
 * 
 * @note 使用 QWaitCondition 避免忙等待
 *       超时时间 100ms
 */
void FaceRecognizerThread::run()
{
    while (m_isRunning) {
        m_mutex.lock();

        //如果没有新帧且不需要加载模型，就等待 100ms 再检查看看需不需要处理，否则就继续处理下面的内容，这样可以避免线程卡死
        while (m_isRunning && !m_hasNewFrame && !m_needLoadModels) {
            m_condition.wait(&m_mutex, 100);
        }

        // 1. 加载模型
        if (m_needLoadModels) {
            m_needLoadModels = false;//m_needLoadModels 的作用是标记是否需要加载模型，加载完成后就将其设为 false，不管它有没有加载成功，因为这个相当于是一个命令，去做了就算执行了命令
            m_isModelsLoaded = false;//m_isModelsLoaded 的作用是标记模型是否加载成功，这个才关心它有没有加载成功
            QString errorMsg;
            try {
                // 加载关键点模型
                dlib::deserialize(m_shapePredictorPath.toStdString()) >> m_shapePredictor;
                // 加载特征提取模型
                dlib::deserialize(m_recognitionModelPath.toStdString()) >> m_recognitionNet;
                m_isModelsLoaded = true;
                errorMsg = "";
                // 初始化数据库并加载人脸
               FaceDbManager::getInstance()->initDb();
            } catch (const std::exception &e) {
                errorMsg = QString::fromUtf8(e.what());
            }

            emit modelsLoaded(m_isModelsLoaded, errorMsg);
            m_mutex.unlock();
            continue;
        }

        // 2. 处理视频帧（模型加载完成 + 有新帧）
        if (m_hasNewFrame && m_isModelsLoaded) {
            m_hasNewFrame = false;
            cv::Mat frame = m_pendingFrame;
            m_mutex.unlock();

            std::vector<FaceLandmarkData> faceDataList;

            // 降分辨率减少计算量
            cv::Mat smallFrame;
            cv::resize(frame, smallFrame, cv::Size(640, 480), 0, 0, cv::INTER_LINEAR);

            // OpenCV Mat 转 dlib 图像
            dlib::cv_image<dlib::rgb_pixel> dlibImage(smallFrame);
            std::vector<dlib::rectangle> faces = m_detector(dlibImage);
          //  qDebug() << "检测到人脸数量：" << faces.size();

            // 处理每个人脸：关键点 + 特征提取
            for (const auto& face : faces) {
                FaceLandmarkData faceData;
                // 人脸框转换（dlib→OpenCV）
                faceData.faceRect = cv::Rect(face.left(), face.top(), face.width(), face.height());
                // 提取关键点
                dlib::full_object_detection shape = m_shapePredictor(dlibImage, face);
                for (unsigned long i = 0; i < shape.num_parts(); ++i) {
                    faceData.landmarks.emplace_back(shape.part(i).x(), shape.part(i).y());
                }
                // 人脸对齐 + 特征提取
                dlib::matrix<dlib::rgb_pixel> faceChip = alignFace(dlibImage, face, m_shapePredictor);
                faceData.descriptor = m_recognitionNet(faceChip);
                faceDataList.push_back(faceData);
            }
            // 人脸检测
            recognize(faceDataList);//输入的是人脸特征列表
            // 发射处理完成信号
            emit faceDataProcessed(faceDataList);
        } else {
            m_mutex.unlock();
        }
    }
}
