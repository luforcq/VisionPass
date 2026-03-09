#ifndef MPU6050_THREAD_H
#define MPU6050_THREAD_H

#include <QThread>
#include <QObject>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>
#include <QDebug>

class Mpu6050Thread : public QThread
{
    Q_OBJECT
public:
    explicit Mpu6050Thread(QObject *parent = nullptr);
    ~Mpu6050Thread() override;

    // 设置线程启停
    void setThreadStart(bool start);
    // 设置振动阈值（可外部调整）
    void setVibrationThreshold(float accThresh, float gyroThresh);
    // 设置门锁状态（0=锁闭，1=开门）
    void setDoorLockedState(bool isLocked);
protected:
    void run() override; // 线程核心执行函数

signals:
    // 振动告警信号（传递振动强度描述）
    void vibrationAlarm(QString alarmMsg);
    // 实时数据信号（可选，用于调试）
    void mpu6050DataReady(float ax, float ay, float az, float gx, float gy, float gz);

private:
    // 门锁状态（true=锁闭，false=开门）+ 互斥锁保护
    bool isDoorLocked = true; // 默认锁闭
    //QMutex doorStateMutex; // 线程安全锁


    bool isRunningFlag = false; // 线程运行标志
    int fd = -1;                // 设备文件描述符
    const char *devPath = "/dev/my_mpu6050"; // MPU6050设备节点（根据实际驱动调整）

    // 振动阈值（可自定义）
    float accThreshold = 1.5f;  // 合加速度阈值（正常重力≈1g，超过1.5g判定振动）
    float gyroThreshold = 50.0f;// 陀螺仪角速度阈值（°/s，超过50判定剧烈振动）

    // 防抖计数（避免单次波动误触发）
    int vibrationCount = 0;
    const int vibrateTriggerCount = 3; // 连续3次超过阈值才告警
};

#endif // MPU6050_THREAD_H
