/******************************************************************
* @projectName   video_server
* @brief         hardwaremanager.h
* @author        Lu Yaolong
*******************************************************************/
#ifndef HARDWAREMANAGER_H
#define HARDWAREMANAGER_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include "servocontrol.h"
#include "at24c64.h"
#include "rc522.h"

class HardwareManager : public QObject
{
    Q_OBJECT

public:
    explicit HardwareManager(QObject *parent = nullptr);
    ~HardwareManager();
    
    bool initialize();
    void unlockDoor();
    void lockDoor();
    bool isDoorUnlocked() const { return m_isUnlocked; }
    void setDoorLockedState(bool locked);
    
    //RC522 相关
    void startCardReading();
    void stopCardReading();
    void prepareWriteCard(const QString& cardId, const QString& userName);
    
    //EEPROM 相关
    bool addCardToDatabase(const QString& cardId, const QString& userName);
    bool verifyCard(const QString& cardId, QString& userName);
    AT24C64* getEEPROM() const { return m_eeprom; }

signals:
    void cardDetected(const QString& cardId, const QString& userName);
    void unauthorizedCard(const QString& cardId);
    void doorUnlocked();
    void doorLocked();
    void writeCardSuccess(const QString& cardId, const QString& userName);
    void writeCardFailed();

private slots:
    void onReadCardTimer();
    void onWriteCardTimer();
    void onDoorLockTimer();

private:
    ServoControl* m_servoControl;
    AT24C64* m_eeprom;
    QTimer* m_readCardTimer;
    QTimer* m_writeCardTimer;
    QTimer* m_doorLockTimer;
    
    bool m_isUnlocked;
    bool m_isAllowControlReset;
    bool m_allowControl1;
    
    QString m_pendingWriteCardId;
    QString m_pendingWriteUserName;
    unsigned char m_writeData[16];
    
    void rc522ReadCard();
    void rc522WriteCard();
    void resetServoAngle();
};

#endif // HARDWAREMANAGER_H
