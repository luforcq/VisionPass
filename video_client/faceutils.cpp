/******************************************************************
* @projectName   video_client
* @brief         faceutils.cpp - 人脸工具类实现
* @author        Lu Yaolong
* 
* @description   提供人脸相关的工具函数
*******************************************************************/
#include "faceutils.h"
#include <QBuffer>

/**
 * @brief 将 OpenCV Mat 转换为 QImage
 * 
 * @param mat OpenCV 矩阵对象
 * @return QImage Qt 图像对象
 * 
 * @description 实现 OpenCV 到 Qt 的图像格式转换：
 *              1. 检查输入是否为空
 *              2. 根据通道数选择转换方式：
 *                 - 3 通道：BGR -> RGB 转换
 *                 - 单通道：直接创建灰度图像
 *              3. 返回 QImage 对象
 * 
 * @note 不创建副本，直接使用 Mat 的数据
 *       需确保 Mat 在 QImage 使用期间不被释放
 */
QImage FaceUtils::matToQImage(const cv::Mat& mat)
{
    if (mat.empty()) {
        return QImage();
    }
    
    cv::Mat rgbMat;
    if (mat.channels() == 3) {
        cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
        return QImage(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                     rgbMat.step, QImage::Format_RGB888);
    } else {
        return QImage(mat.data, mat.cols, mat.rows, 
                     mat.step, QImage::Format_Grayscale8);
    }
}

QByteArray FaceUtils::imageToData(const QImage& image, const char* format, int quality)
{
    if (image.isNull()) {
        return QByteArray();
    }
    
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format, quality);
    return data;
}

QByteArray FaceUtils::matToData(const cv::Mat& mat, const char* format, int quality)
{
    QImage image = matToQImage(mat);
    return imageToData(image, format, quality);
}
