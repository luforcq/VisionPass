#ifndef FACEUTILS_H
#define FACEUTILS_H

#include <QImage>
#include <QByteArray>
#include <opencv2/opencv.hpp>

/**
 * @brief 人脸处理工具类
 * 提供常用的图像转换、数据处理等工具函数
 */
class FaceUtils
{
public:
    /**
     * @brief OpenCV Mat 转 QImage
     * @param mat OpenCV 图像（支持 BGR 三通道或灰度单通道）
     * @return 转换后的 QImage
     */
    static QImage matToQImage(const cv::Mat& mat);
    
    /**
     * @brief QImage 转 QByteArray
     * @param image 输入图像
     * @param format 保存格式（"JPG", "PNG" 等）
     * @param quality 质量（0-100，默认 90）
     * @return 压缩后的图像数据
     */
    static QByteArray imageToData(const QImage& image, const char* format = "JPG", int quality = 90);
    
    /**
     * @brief cv::Mat 转 QByteArray
     * @param mat OpenCV 图像
     * @param format 保存格式（"JPG", "PNG" 等）
     * @param quality 质量（0-100，默认 90）
     * @return 压缩后的图像数据
     */
    static QByteArray matToData(const cv::Mat& mat, const char* format = "JPG", int quality = 90);
};

#endif // FACEUTILS_H
