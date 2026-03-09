/******************************************************************
* @projectName   video_server
* @brief         mainwindow.h
* @author        Lu Yaolong
*******************************************************************/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QImage>
#include <QPushButton>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QTimer>
#include "servocontrol.h"
#include "adccapture_thread.h"
#include "capture_thread.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>
#include <QList>
#include <QAbstractSocket>
#include <QString>
#include <QPainter>
#include <QPen>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <opencv2/opencv.hpp>
#include "at24c64.h"
#include "mpu6050_thread.h"
#include <QCryptographicHash>
#include "passworddialog.h"

// 模块化管理类
#include "tcpservermanager.h"
#include "hardwaremanager.h"
#include "notificationservice.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    // 核心管理器
    TcpServerManager* m_tcpManager;
    HardwareManager* m_hardwareManager;
    NotificationService* m_notificationService;
    
    // 视频采集
    CaptureThread* m_captureThread;
    adccapture_thread* m_adccaptureThread;
    
    // UI 控件
    QLabel* m_videoLabel;
    QLabel* m_statusIndicator;
    QLabel* m_connStatusLabel;
    QLabel* m_notifyMsgLabel;
    QPushButton* m_notifyCloseBtn;
    QPushButton* m_doorbellButton;
    QPushButton* m_passwordUnlockBtn;
    QTimer* m_notifyMsgTimer;
    
    // 密码对话框
    PasswordDialog* m_pwdDialog;
    
    // MPU6050
    Mpu6050Thread* m_mpu6050Thread;
    
    // ADC 相关状态
    bool m_isUnlockedState;
    bool m_allowControl;
    bool m_allowControl1;
    bool m_isStaticState;
    bool m_isAllowControlReset;
    bool m_channel1Detected;
    
    // EEPROM
    AT24C64* m_eep24c64;
    EEP_Key m_key;
    
    // 工具函数
    void hashPwd(const QString &plainPwd, EEP_Pwd *hashedPwd);
    void initLayout();
    void initWidgets();
    void initConnections();
    void setStatusIndicator(const QString &color);
    void updateConnStatus();
    
    // 命令处理
    void handleClientCommand(QTcpSocket* socket, const QString& command);
    void handleUnlockCommand(QTcpSocket* socket);
    void handleWriteCardCommand(QTcpSocket* socket, const QString& command);
    void handleSyncUsersCommand(QTcpSocket* socket, const QString& command);
    void handleVideoCommand(QTcpSocket* socket, const QString& command);
    void handleNotifyCommand(const QString& command);
    void handleSetPwdCommand(QTcpSocket* socket, const QByteArray& data);

private slots:
    void handleChannel1(int value);
    void handleChannel2(int value);
    void handleChannel3(int value);
    void processAdcValue(int value);
    void showImage(QImage image);
    
    void onDoorbellClicked();
    void onPasswordUnlockClicked();
    void onPasswordConfirmed(const QString &password);
    void onTcpMessageReceived(QTcpSocket* socket, const QString& message);
    void onClientConnected(QTcpSocket* socket);
    void onClientDisconnected(QTcpSocket* socket);
    void onCardDetected(const QString& cardId, const QString& userName);
    void onUnauthorizedCard(const QString& cardId);
    void onWriteCardSuccess(const QString& cardId, const QString& userName);
    void onWriteCardFailed();
    void onDoorUnlocked();
    void onDoorLocked();
    
    void handleVibrationAlarm(QString alarmMsg);
    void showMpu6050Data(float ax, float ay, float az, float gx, float gy, float gz);
};

#endif // MAINWINDOW_H
