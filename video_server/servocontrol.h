#ifndef SERVOCONTROL_H
#define SERVOCONTROL_H

#include <string> // 用于设备路径配置

// 独立的舵机控制类（无Qt继承，避免命名冲突）
class ServoControl
{
public:
    // 构造函数：默认设备路径为 /dev/sg90_servo，支持自定义路径
    explicit ServoControl(const std::string &devPath = "/dev/sg90_servo");
    ~ServoControl(); // 析构函数

    // 核心功能：设置舵机角度（0-180°）
    bool setAngle(int angle);

private:
    std::string m_devPath; // 舵机设备节点路径
};

#endif // SERVOCONTROL_H
