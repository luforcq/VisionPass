/******************************************************************
* @projectName   video_server
* @brief         mpu6050_thread.cpp - MPU6050 振动检测线程实现
* @description   本文件实现了 MPU6050 六轴传感器数据采集与振动检测线程
*                通过 I2C 接口读取加速度计和陀螺仪数据，检测异常振动并告警
* @features      1. 独立线程运行，避免阻塞主线程
*                2. 加速度计三轴（X/Y/Z）数据读取
*                3. 陀螺仪三轴（X/Y/Z）角速度读取
*                4. 振动检测算法（合加速度 + 合角速度阈值判断）
*                5. 门锁状态联动（仅锁门时检测振动）
*                6. 实时数据信号发送（用于调试显示）
* @hardware      MPU6050 六轴传感器（I2C 接口，设备节点：/dev/mpu6050）
* @data_format   加速度：±2g (16 位，0.000122g/LSB)
*                角速度：±250°/s (16 位，0.00767°/s/LSB)
* @thread_design 继承 QThread，使用标志位控制启停
* @author        Lu Yaolong
*******************************************************************/
#include "mpu6050_thread.h"

/**
 * @brief 构造函数
 * @param parent 父对象指针
 * @description 初始化 MPU6050 线程对象
 * @note 默认设备路径：/dev/mpu6050
 */
Mpu6050Thread::Mpu6050Thread(QObject *parent) : QThread(parent) {}

/**
 * @brief 析构函数
 * @description 清理资源：
 *              1. 停止线程运行
 *              2. 关闭设备文件描述符
 */
Mpu6050Thread::~Mpu6050Thread()
{
    setThreadStart(false); // 停止线程
    if (fd >= 0) {
        close(fd); // 关闭设备文件
    }
}

/**
 * @brief 设置线程启停状态
 * @param start true-启动线程，false-停止线程
 * @description 控制线程的运行状态：
 *              1. 设置 isRunningFlag 标志位
 *              2. 如果启动且线程未运行，则调用 start() 启动
 * @note 线程启动后会执行 run() 方法
 */
void Mpu6050Thread::setThreadStart(bool start)
{
    isRunningFlag = start;
    if (start && !this->isRunning()) {
        this->start(); // 启动线程
    }
}

/**
 * @brief 设置门锁状态
 * @param isLocked true-门锁闭，false-门开启
 * @description 设置门锁状态，用于振动检测逻辑判断：
 *              1. 更新 isDoorLocked 成员变量
 *              2. 仅当门锁闭时才进行振动告警
 * @note 线程安全设计（注释掉的互斥锁可用于多线程环境）
 */
void Mpu6050Thread::setDoorLockedState(bool isLocked) {
    //QMutexLocker locker(&doorStateMutex); // 自动加锁/解锁
    isDoorLocked = isLocked;
    //qDebug() << "[MPU6050 线程] 门锁状态更新：" << (isLocked ? "锁闭" : "开门");
}

/**
 * @brief 设置振动检测阈值
 * @param accThresh 加速度阈值（单位：g，重力加速度）
 * @param gyroThresh 角速度阈值（单位：°/s）
 * @description 设置振动检测的灵敏度：
 *              1. 设置加速度阈值（默认 12.5g）
 *              2. 设置角速度阈值（默认 500°/s）
 *              3. 超过任一阈值即判定为振动
 * @note 阈值过低会导致误报，过高会漏报
 */
void Mpu6050Thread::setVibrationThreshold(float accThresh, float gyroThresh)
{
    accThreshold = accThresh;
    gyroThreshold = gyroThresh;
}

/**
 * @brief 线程主函数（核心数据采集与振动检测）
 * @description 在独立线程中循环读取 MPU6050 数据并检测振动：
 * 
 * 执行步骤：
 * 1. 打开设备文件（/dev/mpu6050，O_RDWR 模式）
 * 2. 检查文件描述符有效性
 * 3. 进入主循环（while isRunningFlag）
 *    - 读取 6 轴原始数据（3 轴加速度 +3 轴角速度）
 *    - 数据格式转换：
 *      * 加速度：原始值 × 0.000122 → g 单位
 *      * 角速度：原始值 × 0.0305175 → °/s 单位
 *    - 发送实时数据信号（mpu6050DataReady）
 *    - 读取门锁状态（线程安全）
 *    - 振动判定逻辑：
 *      * 仅当门锁闭时检测
 *      * 计算合加速度（减去重力 1g 影响）
 *      * 计算合角速度
 *      * 超过阈值则发射告警信号
 * 4. 延时 200ms（避免采样过快）
 * 5. 线程停止时关闭设备
 * 
 * @note 采样频率：5Hz（200ms 间隔）
 * @note 振动算法：合加速度 = |ax|+|ay|+|az|-1.0g
 */
void Mpu6050Thread::run()
{

    // 1. 打开设备文件
    fd = open(devPath, O_RDWR);
    if (fd < 0) {
        qDebug() << "MPU6050 设备打开失败：" << devPath;
        return;
    }
    short databuf[6] = {0};
    float ax, ay, az, gx, gy, gz;
    int ret = 0;

    // 2. 循环读取数据（线程运行时）
    while (isRunningFlag) {

        ret = read(fd, databuf, sizeof(databuf));
        //qDebug()<<ret;
        if (ret == 0) { // 数据读取成功
            // 转换原始数据（复用参考代码的转换公式）
            ax = databuf[0] * 1.2207e-4f;
            ay = databuf[1] * 1.2207e-4f;
            az = databuf[2] * 1.2207e-4f;
            gx = databuf[3] * 3.05175e-2f;
            gy = databuf[4] * 3.05175e-2f;
            gz = databuf[5] * 3.05175e-2f;

            // 发送实时数据（可选，用于调试）
            emit mpu6050DataReady(ax, ay, az, gx, gy, gz);

//            // 3. 计算合加速度（判断振动核心）
//            float accTotal = sqrt(ax*ax + ay*ay + az*az);
//            // 取陀螺仪三轴角速度绝对值最大值
//            float gyroMax = qMax(qMax(fabs(gx), fabs(gy)), fabs(gz));

//            // 4. 振动判断：合加速度超阈值 OR 陀螺仪角速度超阈值
//            bool isVibration = (accTotal > accThreshold) || (gyroMax > gyroThreshold);
//            if (isVibration) {
//                vibrationCount++;
//                // 连续 N 次超过阈值，触发告警
//                if (vibrationCount >= vibrateTriggerCount) {
//                    QString alarmMsg = QString("检测到剧烈振动！合加速度：%1g，最大角速度：%2°/s")
//                                           .arg(accTotal, 0, 'f', 2)
//                                           .arg(gyroMax, 0, 'f', 2);
//                    emit vibrationAlarm(alarmMsg); // 发送告警信号
//                    vibrationCount = 0; // 重置计数，避免重复告警
//                }
//            } else {
//                vibrationCount = 0; // 无振动，重置计数
//            }
//        }
            // ========== 核心修改：振动判定逻辑 ==========
                // 1. 加锁读取门锁状态（线程安全）
                //doorStateMutex.lock();
                bool currentDoorLocked = isDoorLocked;
               // doorStateMutex.unlock();

                // 2. 仅当门锁闭时，才判定振动告警
                if (currentDoorLocked) {
                    // 计算合加速度（排除重力影响，取绝对值）
                    float accTotal = abs(ax) + abs(ay) + abs(az) - 1.0f; // 减去重力加速度 1g
                    // 计算合角速度
                    float gyroTotal = abs(gx) + abs(gy) + abs(gz);

                    // 判定振动：合加速度或合角速度超过阈值
                    if (accTotal > accThreshold || gyroTotal > gyroThreshold) {
                        QString alarmMsg = QString("门锁闭时检测到剧烈振动！");
                        emit vibrationAlarm(alarmMsg);
                        qDebug() << "[振动告警]" << alarmMsg;
                    }
                }
        }
        usleep(200000); // 200ms 读取一次（和参考代码一致）
    }

    // 3. 线程停止，关闭设备
    close(fd);
    fd = -1;
}
