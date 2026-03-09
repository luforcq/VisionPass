#include "adccapture_thread.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/types.h>
//adccapture_thread::adccapture_thread()
//{

//}

void adccapture_thread::run()
{



while(startFlag){
    __int16_t adc_val[3];
      float voltage1;
     float voltage2;
     float voltage3;
    //int adcValue = -1;
    //char str[50] = {0}; // 用于存储读取的字符串
    //FILE *fp = nullptr;
     int fp;
    // 打开ADC原始值文件
    fp = open("/dev/my_ads1115", O_RDONLY);
    read(fp, &adc_val, sizeof(adc_val));
          voltage1 = adc_val[0] * 4.096 / 32768.0;  // 增益 ±4.096V 时的换算公式
          voltage2 = adc_val[1] * 4.096 / 32768.0;  // 增益 ±4.096V 时的换算公式
          voltage3 = adc_val[2] * 4.096 / 32768.0;  // 增益 ±4.096V 时的换算公式
//          qDebug("ADC1 Value: %d, Voltage: %.4f V\n", adc_val[0], voltage1);
//          qDebug("ADC2 Value: %d, Voltage: %.4f V\n", adc_val[1], voltage2);
//          qDebug("ADC3 Value: %d, Voltage: %.4f V\n", adc_val[2], voltage3);

//        if (fscanf(fp, "%s", str) <= 0) {
//            qDebug() << "Failed to read ADC value";
//        } else {
//            // 转换为整数（ADC原始值范围0-4096）
//            adcValue = atoi(str);
//        }

close(fp);
emit adcReady(adc_val[2]/10);
emit adcChannel1Changed(adc_val[0]);
emit adcChannel2Changed(adc_val[1]);

        usleep(100000);
    }

}


