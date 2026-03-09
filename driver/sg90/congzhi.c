#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

// ==================== 配置参数（根据硬件修改） ====================
#define ADC_RAW_PATH    "/sys/bus/iio/devices/iio:device0/in_voltage1_raw"  // ADC读取路径
#define SG90_DEV_PATH   "/dev/sg90_servo"                                   // SG90设备节点
#define ADC_MAX_RAW     4095                                                // ADC最大原始值（12位ADC=4095，10位=1023）
#define SG90_ANGLE_MIN  0                                                   // SG90最小角度
#define SG90_ANGLE_MAX  180                                                 // SG90最大角度
#define LOOP_INTERVAL   50000                                             // 循环控制间隔（微秒，1秒=1000000）
// =================================================================

/**
 * 读取ADC原始值
 * @return 成功返回ADC原始值，失败返回-1
 */
int read_adc_raw(void)
{
    int fd, ret;
    char buf[16] = {0};

    // 打开ADC sysfs文件
    fd = open(ADC_RAW_PATH, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open ADC path: %s, err: %s\n", ADC_RAW_PATH, strerror(errno));
        return -1;
    }

    // 读取ADC值
    ret = read(fd, buf, sizeof(buf)-1);
    if (ret < 0) {
        fprintf(stderr, "Failed to read ADC value: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    // 字符串转整数
    return atoi(buf);
}

/**
 * ADC值映射为SG90角度（线性映射）
 * @param adc_raw: ADC原始值
 * @return 映射后的角度（0~180）
 */
int adc_to_angle(int adc_raw)
{
    // 边界保护（防止ADC值超出范围）
    if (adc_raw < 0) adc_raw = 0;
    if (adc_raw > ADC_MAX_RAW) adc_raw = ADC_MAX_RAW;

    // 线性映射公式：角度 = 最小角度 + (ADC值 / ADC最大值) * (最大角度-最小角度)
    return SG90_ANGLE_MIN + (float)adc_raw / ADC_MAX_RAW * (SG90_ANGLE_MAX - SG90_ANGLE_MIN);
}

/**
 * 写入角度到SG90驱动，控制舵机转动
 * @param angle: 目标角度（0~180）
 * @return 成功返回0，失败返回-1
 */
int sg90_set_angle(int angle)
{
    int fd;
    char angle_buf[16] = {0};

    // 打开SG90设备节点
    fd = open(SG90_DEV_PATH, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open SG90 dev: %s, err: %s\n", SG90_DEV_PATH, strerror(errno));
        return -1;
    }

    // 将角度转为字符串（驱动需要字符串格式的数值）
    snprintf(angle_buf, sizeof(angle_buf), "%d", angle);

    // 写入角度到设备节点
    if (write(fd, angle_buf, strlen(angle_buf)) < 0) {
        fprintf(stderr, "Failed to write angle to SG90: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    printf("ADC raw: %d -> SG90 angle: %d°\n", read_adc_raw(), angle); // 打印映射关系（可选）
    return 0;
}

int main(int argc, char *argv[])
{
    int adc_raw, angle;

    printf("SG90 ADC Control Program Start...\n");
    printf("ADC max raw: %d, SG90 angle range: %d~%d°\n", ADC_MAX_RAW, SG90_ANGLE_MIN, SG90_ANGLE_MAX);

    // 循环读取ADC并控制舵机（按Ctrl+C退出）
    while (1) {
        // 1. 读取ADC原始值
        adc_raw = read_adc_raw();
        if (adc_raw < 0) {
            fprintf(stderr, "Read ADC failed, retry after %dms\n", LOOP_INTERVAL/1000);
            usleep(LOOP_INTERVAL);
            continue;
        }

        // 2. ADC值映射为SG90角度
        angle = adc_to_angle(adc_raw);

        // 3. 控制SG90转动到目标角度
        if (sg90_set_angle(angle) < 0) {
            fprintf(stderr, "Set SG90 angle failed, retry after %dms\n", LOOP_INTERVAL/1000);
            usleep(LOOP_INTERVAL);
            continue;
        }

        // 间隔一段时间再读取（避免频繁控制）
        usleep(LOOP_INTERVAL);
    }

    return 0;
}
