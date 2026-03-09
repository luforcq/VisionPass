/******************************************************************
* @projectName   video_server
* @brief         servocontrol.cpp - 舵机控制模块实现
* @description   本文件实现了舵机角度控制功能，用于门禁开关控制
* @features      1. 角度控制（0-180°范围）
*                2. 设备文件写入控制（/dev/pwm0）
*                3. 参数合法性校验
*                4. 错误处理与日志输出
* @hardware      舵机（PWM 控制，设备节点：/dev/pwm0）
* @control_logic 0°- 锁门（舵机水平）
*                90°- 开门（舵机垂直）
* @pwm_signal    PWM 脉冲宽度调制，周期 20ms，占空比控制角度
* @author        Lu Yaolong
*******************************************************************/
#include "servocontrol.h"
// 硬件操作所需头文件
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/**
 * @brief 构造函数
 * @param devPath 舵机设备路径（默认"/dev/pwm0"）
 * @description 初始化舵机控制对象，保存设备路径
 */
ServoControl::ServoControl(const std::string &devPath)
    : m_devPath(devPath)
{
}

/**
 * @brief 析构函数
 * @description 清理资源（无动态分配内存）
 */
ServoControl::~ServoControl()
{
}

/**
 * @brief 设置舵机角度
 * @param angle 目标角度（0-180°）
 * @return bool 成功返回 true，失败返回 false
 * @description 控制舵机转动到指定角度：
 *              1. 参数校验（0-180°范围）
 *              2. 打开设备文件（O_WRONLY 模式）
 *              3. 角度值转字符串
 *              4. 写入设备节点
 *              5. 关闭文件描述符
 *              6. 打印成功日志
 * @note 舵机角度与门锁状态：
 *       0° - 锁门（舵机水平）
 *       90° - 开门（舵机垂直）
 * @warning 角度超出范围会返回 false
 */
bool ServoControl::setAngle(int angle)
{
    // 1. 参数合法性校验（0-180°）
    if (angle < 0 || angle > 180) {
        printf("ServoControl error: angle must be in 0-180 (current: %d)\n", angle);
        return false;
    }

    // 2. 打开舵机设备节点（只写模式）
    int fd = open(m_devPath.c_str(), O_WRONLY);
    if (fd < 0) {
        printf("ServoControl error: failed to open device %s, reason: %s\n",
               m_devPath.c_str(), strerror(errno));
        return false;
    }

    // 3. 将角度转为字符串并写入设备
    char angleBuf[16] = {0};
    snprintf(angleBuf, sizeof(angleBuf), "%d", angle);
    ssize_t writeLen = write(fd, angleBuf, strlen(angleBuf));
    if (writeLen < 0) {
        printf("ServoControl error: failed to write angle %d, reason: %s\n",
               angle, strerror(errno));
        close(fd); // 普通类中，直接调用全局 close()，无命名冲突
        return false;
    }

    // 4. 关闭设备文件描述符
    close(fd);
    printf("ServoControl success: set angle to %d°\n", angle);
    return true;
}
    return true;
}


