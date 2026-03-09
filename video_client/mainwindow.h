/******************************************************************
* @projectName   video_client
* @brief         mainwindow.h - 智能门禁系统客户端主窗口
* @author        Lu Yaolong
*******************************************************************/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMutex>
#include <QImage>
#include <QUdpSocket>
#include "uimanager.h"
#include "networkmanager.h"
#include "faceservice.h"
#include "configmanager.h"
#include "changepwddialog.h"
#include "notifydialog.h"
#include "facedbdialog.h"

using namespace cv;
using namespace dlib;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onUnlockBtnClicked();
    void onStartVideoBtnClicked(bool checked);
    void onChangePwdBtnClicked();
    void onSendNotifyBtnClicked();
    void onOpenFaceDbBtnClicked();
    void onNetworkConnected();
    void onNetworkDisconnected();
    void onNetworkError(QAbstractSocket::SocketError error);
    void onTcpDataReceived(const QByteArray& data);
    void onVideoFrameReceived(const QByteArray& datagram);
    void onFaceRecognized(const RecognizeResult& result);
    void onFaceDataProcessed(const std::vector<FaceLandmarkData>& faceDataList);
    void onModelsLoaded(bool success, const QString& errorMsg);
    void onWriteCardRequested(const QString& id, const QString& name);
    void onSyncUsersRequested(const QStringList& userIds);

private:
    void initializeModules();
    void setupConnections();
    void sendUnlockCommand();
    cv::Mat qImageToCvMat(const QImage &qimg);
    QImage cvMatToQImage(const cv::Mat &cvmat);
    void resizeEvent(QResizeEvent *event) override;
    void showTipMessage(const QString& tipText,
                       int durationMs = 3000,
                       const QColor& bgColor = QColor(255, 0, 0, 178),
                       const QColor& textColor = Qt::white);

    UIManager *m_uiManager;
    NetworkManager *m_networkManager;
    FaceService *m_faceService;
    
    QMutex m_faceDataMutex;
    std::vector<FaceLandmarkData> m_cachedFaceData;
    bool m_isVideoRunning;
};

#endif // MAINWINDOW_H
