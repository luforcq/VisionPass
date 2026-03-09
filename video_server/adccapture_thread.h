#ifndef ADCCAPTURE_THREAD_H
#define ADCCAPTURE_THREAD_H
#include <QThread>
#include <QDebug>
class adccapture_thread : public QThread
{
    Q_OBJECT

signals:
    /* 准备图片 */
    void adcReady(int);
    /* 三个 ADC 通道值 */
    void adcChannel1Changed(int value);
    void adcChannel2Changed(int value);
private:
    /* 线程开启flag */
    bool startFlag = false;
public:
    adccapture_thread(QObject *parent = nullptr) {
        Q_UNUSED(parent);
    }
        void run() override;

public slots:
    /* 设置线程 */
    void setThreadStart(bool start) {
        startFlag = start;
        if (start) {
            if (!this->isRunning())
                this->start();
        } else {
            this->quit();
        }
    }

};

#endif // ADCCAPTURE_THREAD_H
