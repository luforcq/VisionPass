/******************************************************************
* @projectName   video_server
* @brief         notificationservice.cpp
* @author        Lu Yaolong
*******************************************************************/
#include "notificationservice.h"
#include "QDebug"
NotificationService::NotificationService(QObject *parent)
    : QObject(parent)
    , m_msgLabel(nullptr)
    , m_closeBtn(nullptr)
    , m_timer(nullptr)
{
}

NotificationService::~NotificationService()
{
}

void NotificationService::initialize(QLabel* msgLabel, QPushButton* closeBtn, QTimer* timer)
{
    m_msgLabel = msgLabel;
    m_closeBtn = closeBtn;
    m_timer = timer;
    
    if (m_msgLabel && m_closeBtn && m_timer) {
        connect(m_closeBtn, &QPushButton::clicked, 
                this, &NotificationService::onCloseButtonClicked);
        connect(m_timer, &QTimer::timeout, 
                this, &NotificationService::onTimeout);
    }
}

void NotificationService::showMessage(const QString& message, int durationMs)
{
    if (!m_msgLabel || !m_closeBtn || !m_timer) {
        qWarning() << "NotificationService 未初始化";
        return;
    }
    
    m_currentMessage = message;
    m_msgLabel->setText(message);
    m_msgLabel->show();
    m_closeBtn->show();
    
    m_timer->stop();
    m_timer->start(durationMs);
    
    emit messageShown();
    qDebug() << "显示通知消息：" << message << "时长：" << durationMs << "ms";
}

void NotificationService::hideMessage()
{
    if (!m_msgLabel || !m_closeBtn || !m_timer) {
        return;
    }
    
    m_timer->stop();
    m_msgLabel->hide();
    m_closeBtn->hide();
    
    emit messageHidden();
    qDebug() << "隐藏通知消息";
}

void NotificationService::showSuccess(const QString& message)
{
    showMessage(message, 3000);
    if (m_msgLabel) {
        m_msgLabel->setStyleSheet(R"(
            QLabel{
                color: #00FF00;
                font-size: 24px;
                font-weight: bold;
                background-color: rgba(255,255,255,0.95);
                border-radius: 15px;
                border:2px solid green;
            }
        )");
    }
}

void NotificationService::showError(const QString& message)
{
    showMessage(message, 3000);
    if (m_msgLabel) {
        m_msgLabel->setStyleSheet(R"(
            QLabel{
                color: #FF0000;
                font-size: 24px;
                font-weight: bold;
                background-color: rgba(255,255,255,0.95);
                border-radius: 15px;
                border:2px solid red;
            }
        )");
    }
}

void NotificationService::showWarning(const QString& message)
{
    showMessage(message, 3000);
    if (m_msgLabel) {
        m_msgLabel->setStyleSheet(R"(
            QLabel{
                color: #FFA500;
                font-size: 24px;
                font-weight: bold;
                background-color: rgba(255,255,255,0.95);
                border-radius: 15px;
                border:2px solid orange;
            }
        )");
    }
}

void NotificationService::showInfo(const QString& message)
{
    showMessage(message, 3000);
    if (m_msgLabel) {
        m_msgLabel->setStyleSheet(R"(
            QLabel{
                color: #0066FF;
                font-size: 24px;
                font-weight: bold;
                background-color: rgba(255,255,255,0.9);
                border-radius: 15px;
                border:2px solid #0066FF;
            }
        )");
    }
}

void NotificationService::onCloseButtonClicked()
{
    hideMessage();
    emit closeButtonClicked();
    qDebug() << "用户手动关闭了通知消息";
}

void NotificationService::onTimeout()
{
    hideMessage();
    qDebug() << "通知消息显示时长到，自动关闭";
}

void NotificationService::applyStyle(const QString& color, const QString& borderColor)
{
    if (!m_msgLabel) {
        return;
    }
    
    QString style = QString(R"(
        QLabel{
            color: %1;
            font-size: 24px;
            font-weight: bold;
            background-color: rgba(255,255,255,0.95);
            border-radius: 15px;
            border:2px solid %2;
        }
    )").arg(color).arg(borderColor);
    
    m_msgLabel->setStyleSheet(style);
}
