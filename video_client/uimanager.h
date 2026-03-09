/******************************************************************
* @projectName   video_client
* @brief         uimanager.h - UI 管理器头文件
* @author        Lu Yaolong
* 
* @description   UI 管理器：负责客户端所有界面元素的创建、布局和管理
* 
* @responsibilities
*                - 创建和初始化所有 UI 控件（按钮、标签等）
*                - 设置界面布局和样式
*                - 管理用户交互（按钮点击信号）
*                - 显示提示消息和状态更新
*                - 实时时间显示
*******************************************************************/
#ifndef UIMANAGER_H
#define UIMANAGER_H

#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QColor>

/**
 * @brief UI 管理器类
 * 
 * @description 封装所有 UI 相关操作，实现界面与业务逻辑分离
 * 
 * @note 采用单例模式，整个应用只有一个 UIManager 实例
 */
class UIManager : public QObject
{
    Q_OBJECT

public:
    explicit UIManager(QObject *parent = nullptr);
    ~UIManager();

    // 初始化和设置方法
    void setupMainWindow(QMainWindow* mainWindow);  ///< 设置主窗口
    void setupStyle();                               ///< 设置整体样式
    void setupLayout();                              ///< 设置布局
    void createButtons();                            ///< 创建所有按钮
    void createLabels();                             ///< 创建所有标签
    void createTimeDisplay();                        ///< 创建时间显示
    void setupConnections();                         ///< 设置信号槽连接

    // Getter 方法
    QPushButton* getUnlockBtn() const { return m_unlockBtn; }           ///< 获取开锁按钮
    QPushButton* getStartVideoBtn() const { return m_startVideoBtn; }   ///< 获取视频开关按钮
    QPushButton* getChangePwdBtn() const { return m_changePwdBtn; }     ///< 获取修改密码按钮
    QPushButton* getSendNotifyBtn() const { return m_sendNotifyBtn; }   ///< 获取发送通知按钮
    QPushButton* getBtnOpenFaceDb() const { return m_btnOpenFaceDb; }   ///< 获取人脸库按钮
    QLabel* getVideoLabel() const { return m_videoLabel; }              ///< 获取视频标签
    QLabel* getConnStatusLabel() const { return m_connStatusLabel; }    ///< 获取连接状态标签
    QLabel* getDoorbellTipLabel() const { return m_doorbellTipLabel; }  ///< 获取门铃提示标签
    QLabel* getTimeLabel() const { return m_timeLabel; }                ///< 获取时间标签

    // 功能方法
    /**
     * @brief 显示提示消息
     * @param tipText 提示文本内容
     * @param durationMs 显示时长（毫秒，默认 3000ms）
     * @param bgColor 背景颜色（默认半透明红色）
     * @param textColor 文字颜色（默认白色）
     */
    void showTipMessage(const QString& tipText,
                       int durationMs = 3000,
                       const QColor& bgColor = QColor(255, 0, 0, 178),
                       const QColor& textColor = Qt::white);
    
    void updateConnectionStatus(bool connected);  ///< 更新连接状态
    void updateTime();                             ///< 更新时间显示

signals:
    void unlockButtonClicked();           ///< 开锁按钮点击信号
    void startVideoButtonClicked(bool checked);  ///< 视频开关按钮点击信号
    void changePwdButtonClicked();        ///< 修改密码按钮点击信号
    void sendNotifyButtonClicked();       ///< 发送通知按钮点击信号
    void openFaceDbButtonClicked();       ///< 人脸库按钮点击信号
    void timeUpdateRequested();           ///< 时间更新请求信号

private:
    // 初始化方法
    void initUnlockBtn();          ///< 初始化开锁按钮
    void initStartVideoBtn();      ///< 初始化视频开关按钮
    void initChangePwdBtn();       ///< 初始化修改密码按钮
    void initSendNotifyBtn();      ///< 初始化发送通知按钮
    void initOpenFaceDbBtn();      ///< 初始化人脸库按钮
    void initVideoLabel();         ///< 初始化视频标签
    void initConnStatusLabel();    ///< 初始化连接状态标签
    void initDoorbellTipLabel();   ///< 初始化门铃提示标签
    void initTimeLabel();          ///< 初始化时间标签
    void setupButtonStyles();      ///< 设置按钮样式
    void setupLabelStyles();       ///< 设置标签样式

    // 按钮成员
    QPushButton *m_unlockBtn;         ///< 开锁按钮
    QPushButton *m_startVideoBtn;     ///< 视频开关按钮
    QPushButton *m_changePwdBtn;      ///< 修改密码按钮
    QPushButton *m_sendNotifyBtn;     ///< 发送通知按钮
    QPushButton *m_btnOpenFaceDb;     ///< 人脸库管理按钮
    
    // 标签成员
    QLabel *m_videoLabel;             ///< 视频显示标签
    QLabel *m_connStatusLabel;        ///< 网络连接状态标签
    QLabel *m_doorbellTipLabel;       ///< 门铃提示标签
    QLabel *m_timeLabel;              ///< 实时时间标签
    
    // 定时器成员
    QTimer *m_timeUpdateTimer;        ///< 时间更新定时器（1 秒）
    QTimer *m_tipHideTimer;           ///< 提示隐藏定时器
    
    // 窗口和布局成员
    QMainWindow *m_mainWindow;        ///< 主窗口指针
    QWidget *m_centralWidget;         ///< 中心控件
    QVBoxLayout *m_mainLayout;        ///< 主布局（垂直）
    QHBoxLayout *m_contentLayout;     ///< 内容布局（水平）
    QVBoxLayout *m_rightLayout;       ///< 右侧按钮布局（垂直）
};

#endif // UIMANAGER_H
