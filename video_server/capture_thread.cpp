/******************************************************************
* @projectName   video_server
* @brief         capture_thread.cpp - 视频采集与推流线程实现
* @description   本文件实现了基于 V4L2 的视频采集与 UDP 推流线程
*                负责从摄像头采集视频数据，进行本地显示和网络广播推流
* @features      1. V4L2 视频采集（Video4Linux2 API）
*                2. 内存映射（mmap）高效数据读取
*                3. 多缓冲区循环使用（VIDEO_BUFFER_COUNT）
*                4. 本地显示（QImage 信号发送）
*                5. UDP 广播推流（JPEG 编码+Base64）
*                6. 可配置的本地显示和推流开关
* @video_format V4L2_PIX_FMT_RGB565, 640x480@RGB16
* @stream_proto UDP 广播，端口 8888，JPEG+Base64 编码
* @performance   本地显示与网络推流互斥（避免相互影响导致卡顿）
* @hardware     V4L2 兼容设备，/dev/video0
* @author        Lu Yaolong
*******************************************************************/
#include "capture_thread.h"

/**
 * @brief 线程主函数（V4L2 视频采集与推流）
 * @description 实现基于 V4L2 的视频采集和 UDP 推流：
 * 
 * 执行步骤：
 * 1. 平台检查（仅 ARM Linux 平台运行）
 * 2. 打开视频设备（/dev/video0，O_RDWR 模式）
 * 3. 设置视频格式：
 *    - 分辨率：640x480
 *    - 像素格式：V4L2_PIX_FMT_RGB565
 *    - 色彩空间：V4L2_COLORSPACE_SRGB
 * 4. 申请缓冲区（VIDIOC_REQBUFS，VIDEO_BUFFER_COUNT 个）
 * 5. 查询并映射缓冲区（VIDIOC_QUERYBUF + mmap）
 * 6. 入队缓冲区（VIDIOC_QBUF，所有缓冲区）
 * 7. 启动采集流（VIDIOC_STREAMON）
 * 8. 主循环（while startFlag）：
 *    - 出队缓冲区（VIDIOC_DQBUF）
 *    - 创建 QImage（RGB16 格式）
 *    - 条件本地显示（emit imageReady）
 *    - 条件 UDP 推流：
 *      * QImage 转 JPEG（QByteArray）
 *      * Base64 编码
 *      * UDP 广播（端口 8888）
 *    - 缓冲区重新入队（VIDIOC_QBUF）
 * 9. 清理资源（munmap + close）
 * 
 * @note 本地显示与网络推流互斥（避免相互影响导致卡顿）
 * @note 延时 800ms 确保缓冲区稳定（至少 650ms）
 * @note 使用静态缓冲区结构体避免栈溢出
 */
void CaptureThread::run()
{
    /* 下面的代码请参考正点原子 C 应用编程 V4L2 章节，摄像头编程，这里不作解释 */
#ifdef linux
#ifndef __arm__
    return;
#endif
    int video_fd = -1;
    struct v4l2_format fmt;
    struct v4l2_requestbuffers req_bufs;
    static struct v4l2_buffer buf;
    int n_buf;
    struct buffer_info bufs_info[VIDEO_BUFFER_COUNT];
    enum v4l2_buf_type type;

    video_fd = open(VIDEO_DEV, O_RDWR);
    if (0 > video_fd) {
        printf("ERROR: failed to open video device %s\n", VIDEO_DEV);
        return ;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;

    if (0 > ioctl(video_fd, VIDIOC_S_FMT, &fmt)) {
        printf("ERROR: failed to VIDIOC_S_FMT\n");
        close(video_fd);
        return ;
    }

    req_bufs.count = VIDEO_BUFFER_COUNT;
    req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_bufs.memory = V4L2_MEMORY_MMAP;

    if (0 > ioctl(video_fd, VIDIOC_REQBUFS, &req_bufs)) {
        printf("ERROR: failed to VIDIOC_REQBUFS\n");
        return ;
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    for (n_buf = 0; n_buf < VIDEO_BUFFER_COUNT; n_buf++) {

        buf.index = n_buf;
        if (0 > ioctl(video_fd, VIDIOC_QUERYBUF, &buf)) {
            printf("ERROR: failed to VIDIOC_QUERYBUF\n");
            return ;
        }

        bufs_info[n_buf].length = buf.length;
        bufs_info[n_buf].start = mmap(NULL, buf.length,
                                      PROT_READ | PROT_WRITE, MAP_SHARED,
                                      video_fd, buf.m.offset);
        if (MAP_FAILED == bufs_info[n_buf].start) {
            printf("ERROR: failed to mmap video buffer, size 0x%x\n", buf.length);
            return ;
        }
    }

    for (n_buf = 0; n_buf < VIDEO_BUFFER_COUNT; n_buf++) {

        buf.index = n_buf;
        if (0 > ioctl(video_fd, VIDIOC_QBUF, &buf)) {
            printf("ERROR: failed to VIDIOC_QBUF\n");
            return ;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 > ioctl(video_fd, VIDIOC_STREAMON, &type)) {
        printf("ERROR: failed to VIDIOC_STREAMON\n");
        return ;
    }

    while (startFlag) {

        for (n_buf = 0; n_buf < VIDEO_BUFFER_COUNT; n_buf++) {

            buf.index = n_buf;

            if (0 > ioctl(video_fd, VIDIOC_DQBUF, &buf)) {
                printf("ERROR: failed to VIDIOC_DQBUF\n");
                return;
            }

            QImage qImage((unsigned char*)bufs_info[n_buf].start, fmt.fmt.pix.width, fmt.fmt.pix.height, QImage::Format_RGB16);

            /* 是否开启本地显示，开启本地显示可能会导致开启广播卡顿，它们互相制约 */
            if (startLocalDisplay)
                emit imageReady(qImage);

            /* 是否开启广播，开启广播会导致本地显示卡顿，它们互相制约 */
            if (startBroadcast) {
                /* udp套接字 */
                QUdpSocket udpSocket;

                /* QByteArray类型 */
                QByteArray byte;

                /* 建立一个用于IO读写的缓冲区 */
                QBuffer buff(&byte);

                /* image转为byte的类型，再存入buff */
                qImage.save(&buff, "JPEG", -1);

                /* 转换为base64Byte类型 */
                QByteArray base64Byte = byte.toBase64();

                /* 由udpSocket以广播的形式传输数据，端口号为8888 */
                udpSocket.writeDatagram(base64Byte.data(), base64Byte.size(), QHostAddress::Broadcast, 8888);
            }

            if (0 > ioctl(video_fd, VIDIOC_QBUF, &buf)) {
                printf("ERROR: failed to VIDIOC_QBUF\n");
                return;
            }
        }
    }

    msleep(800);//at lease 650

    for (int i = 0; i < VIDEO_BUFFER_COUNT; i++) {
        munmap(bufs_info[i].start, buf.length);
    }

    close(video_fd);
#endif
}
