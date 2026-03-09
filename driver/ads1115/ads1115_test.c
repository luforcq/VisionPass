#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>

int main(void) {
    int fd;
    __int16_t adc_val[2];
    float voltage1;
   float voltage2;
    fd = open("/dev/my_ads1115", O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return -1;
    }

    while (1) {
        read(fd, &adc_val, sizeof(adc_val));
        voltage1 = adc_val[0] * 4.096 / 32768.0;  // 增益 ±4.096V 时的换算公式
        voltage2 = adc_val[1] * 4.096 / 32768.0;  // 增益 ±4.096V 时的换算公式
        printf("ADC1 Value: %d, Voltage: %.4f V\n", adc_val[0], voltage1);
        printf("ADC2 Value: %d, Voltage: %.4f V\n", adc_val[1], voltage2);
        sleep(1);
    }
    close(fd);
    return 0;
}
