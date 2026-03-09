/******************************************************************
* @projectName   video_server
* @brief         notificationservice.h
* @author        Lu Yaolong
*******************************************************************/
#ifndef NOTIFICATIONSERVICE_H
#define NOTIFICATIONSERVICE_H

#include <QObject>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QString>

class NotificationService : public QObject
{
    Q_OBJECT

public:
    explicit NotificationService(QObject *parent = nullptr);
    ~NotificationService();
    
    void initialize(QLabel* msgLabel, QPushButton* closeBtn, QTimer* timer);
    void showMessage(const QString& message, int durationMs = 3000);
    void hideMessage();
    void showSuccess(const QString& message);
    void showError(const QString& message);
    void showWarning(const QString& message);
    void showInfo(const QString& message);

signals:
    void messageShown();
    void messageHidden();
    void closeButtonClicked();

public slots:
    void onCloseButtonClicked();
    void onTimeout();

private:
    QLabel* m_msgLabel;
    QPushButton* m_closeBtn;
    QTimer* m_timer;
    QString m_currentMessage;
    
    void applyStyle(const QString& color, const QString& borderColor);
};

#endif // NOTIFICATIONSERVICE_H
