/******************************************************************
* @projectName   video_client
* @brief         uimanager.cpp - UI 管理器实现
* @author        Lu Yaolong
* 
* @description   本文件实现了客户端的 UI 管理器，负责所有界面元素的创建和管理
* 
* @responsibilities
*                - 创建和初始化所有 UI 控件
*                - 设置界面布局和样式（QSS）
*                - 管理用户交互（按钮点击、状态更新）
*                - 显示提示消息和实时时间
* 
* @design_pattern  Manager 模式
*                 - 集中管理所有 UI 元素
*                 - 提供统一的接口
*                 - 降低耦合度
*******************************************************************/
#include "uimanager.h"
#include <QMainWindow>
#include <QMessageBox>
#include <QDateTime>
#include <QDebug>

/**
 * @brief UIManager 构造函数
 * 
 * @param parent 父对象指针
 * 
 * @description 初始化所有成员为 nullptr
 * 
 * @note 所有指针初始化为 nullptr，避免野指针
 *       实际创建在 setupMainWindow() 中进行
 */
UIManager::UIManager(QObject *parent)
    : QObject(parent)
    , m_unlockBtn(nullptr)
    , m_startVideoBtn(nullptr)
    , m_changePwdBtn(nullptr)
    , m_sendNotifyBtn(nullptr)
    , m_btnOpenFaceDb(nullptr)
    , m_videoLabel(nullptr)
    , m_connStatusLabel(nullptr)
    , m_doorbellTipLabel(nullptr)
    , m_timeLabel(nullptr)
    , m_timeUpdateTimer(nullptr)
    , m_tipHideTimer(nullptr)
    , m_mainWindow(nullptr)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_contentLayout(nullptr)
    , m_rightLayout(nullptr)
{
}

/**
 * @brief UIManager 析构函数
 * 
 * @description 清理资源
 * 
 * @note Qt 的对象树机制会自动删除所有子对象
 *       按钮、标签等都有父对象，无需手动删除
 */
UIManager::~UIManager()
{
}

/**
 * @brief 设置主窗口
 * 
 * @param mainWindow 主窗口指针
 * 
 * @description 完成主窗口的初始化设置：
 *              1. 保存主窗口指针
 *              2. 设置窗口标题
 *              3. 设置窗口大小（800x480）
 *              4. 调用初始化流程：
 *                 - setupStyle(): 设置样式
 *                 - createButtons(): 创建按钮
 *                 - createLabels(): 创建标签
 *                 - setupLayout(): 设置布局
 *                 - setupConnections(): 连接信号槽
 * 
 * @note 采用链式调用，按顺序执行
 *       确保所有控件创建完成后再设置布局
 */
void UIManager::setupMainWindow(QMainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
    
    m_mainWindow->setWindowTitle("智能门禁系统客户端");
    m_mainWindow->setGeometry(0, 0, 800, 480);
    
    setupStyle();
    createButtons();
    createLabels();
    setupLayout();
    setupConnections();
}

/**
 * @brief 设置整体样式
 * 
 * @description 设置主窗口的背景样式：
 *              - 使用线性渐变背景
 *              - 从深蓝灰色到深灰色
 *              - 营造科技感
 * 
 * @note QSS（Qt Style Sheets）类似 CSS
 *       使用 qlineargradient 创建渐变效果
 */
void UIManager::setupStyle()
{
    m_mainWindow->setStyleSheet(
        "QMainWindow {"
        "    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "                stop:0 #2c3e50, stop:1 #1a252f);"
        "}"
    );
}

/**
 * @brief 创建所有按钮
 * 
 * @description 按顺序初始化 5 个功能按钮：
 *              1. initUnlockBtn(): 开锁按钮
 *              2. initStartVideoBtn(): 视频开关按钮
 *              3. initChangePwdBtn(): 修改密码按钮
 *              4. initSendNotifyBtn(): 发送通知按钮
 *              5. initOpenFaceDbBtn(): 人脸库按钮
 * 
 * @note 按钮创建顺序与界面布局一致
 */
void UIManager::createButtons()
{
    initUnlockBtn();
    initStartVideoBtn();
    initChangePwdBtn();
    initSendNotifyBtn();
    initOpenFaceDbBtn();
}

/**
 * @brief 创建所有标签
 * 
 * @description 按顺序初始化 4 个标签：
 *              1. initVideoLabel(): 视频显示标签
 *              2. initConnStatusLabel(): 连接状态标签
 *              3. initDoorbellTipLabel(): 门铃提示标签
 *              4. initTimeLabel(): 时间显示标签
 * 
 * @note 标签用于显示信息和状态
 */
void UIManager::createLabels()
{
    initVideoLabel();
    initConnStatusLabel();
    initDoorbellTipLabel();
    initTimeLabel();
}

/**
 * @brief 设置界面布局
 * 
 * @description 创建并设置整体布局结构：
 *              1. 创建中心 widget 和主布局（垂直）
 *              2. 添加时间标签到顶部居中
 *              3. 创建内容布局（水平）：
 *                 - 左侧：视频标签（占据大部分空间）
 *                 - 右侧：按钮布局（垂直排列）
 *              4. 设置间距和边距
 *              5. 设置中心 widget
 *              6. 提升浮动标签（连接状态、门铃提示）
 * 
 * @note 布局层次：
 *       QVBoxLayout (主布局)
 *       └── QHBoxLayout (内容布局)
 *           ├── videoLabel (视频)
 *           └── QVBoxLayout (右侧按钮)
 */
void UIManager::setupLayout()
{
    m_centralWidget = new QWidget(m_mainWindow);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    
    m_mainLayout->addWidget(m_timeLabel, 0, Qt::AlignHCenter | Qt::AlignTop);
    m_mainLayout->addSpacing(5);
    
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->addWidget(m_videoLabel);
    
    m_rightLayout = new QVBoxLayout();
    m_rightLayout->addStretch();
    m_rightLayout->addWidget(m_unlockBtn, 0, Qt::AlignCenter);
    m_rightLayout->addSpacing(15);
    m_rightLayout->addWidget(m_startVideoBtn, 0, Qt::AlignCenter);
    m_rightLayout->addSpacing(15);
    m_rightLayout->addWidget(m_changePwdBtn, 0, Qt::AlignCenter);
    m_rightLayout->addSpacing(15);
    m_rightLayout->addWidget(m_sendNotifyBtn, 0, Qt::AlignCenter);
    m_rightLayout->addSpacing(15);
    m_rightLayout->addWidget(m_btnOpenFaceDb, 0, Qt::AlignCenter);
    m_rightLayout->addStretch();
    
    m_contentLayout->addLayout(m_rightLayout);
    m_contentLayout->setSpacing(20);
    m_contentLayout->setContentsMargins(20, 0, 20, 20);
    
    m_mainLayout->addLayout(m_contentLayout);
    m_mainLayout->setSpacing(0);
    m_mainLayout->setContentsMargins(20, 20, 20, 0);
    
    m_mainWindow->setCentralWidget(m_centralWidget);
    
    m_connStatusLabel->setParent(m_centralWidget);
    m_doorbellTipLabel->setParent(m_centralWidget);
    m_connStatusLabel->raise();
    m_doorbellTipLabel->raise();
}

/**
 * @brief 设置信号槽连接
 * 
 * @description 建立所有按钮的信号槽连接：
 *              1. 开锁按钮 -> unlockButtonClicked 信号
 *              2. 视频按钮 -> startVideoButtonClicked 信号（带 checked 状态）
 *              3. 修改密码按钮 -> changePwdButtonClicked 信号
 *              4. 发送通知按钮 -> sendNotifyButtonClicked 信号
 *              5. 人脸库按钮 -> openFaceDbButtonClicked 信号
 *              6. 时间定时器 -> updateTime 槽
 *              7. 提示隐藏定时器 -> hide 槽
 * 
 * @note 使用 Lambda 表达式传递 checked 状态
 *       定时器每秒更新一次时间
 */
void UIManager::setupConnections()
{
    connect(m_unlockBtn, &QPushButton::clicked, this, &UIManager::unlockButtonClicked);
    connect(m_startVideoBtn, &QPushButton::clicked, this, [this]() {
        emit startVideoButtonClicked(m_startVideoBtn->isChecked());
    });
    connect(m_changePwdBtn, &QPushButton::clicked, this, &UIManager::changePwdButtonClicked);
    connect(m_sendNotifyBtn, &QPushButton::clicked, this, &UIManager::sendNotifyButtonClicked);
    connect(m_btnOpenFaceDb, &QPushButton::clicked, this, &UIManager::openFaceDbButtonClicked);
    
    m_timeUpdateTimer = new QTimer(this);
    m_timeUpdateTimer->setInterval(1000);
    connect(m_timeUpdateTimer, &QTimer::timeout, this, &UIManager::updateTime);
    m_timeUpdateTimer->start();
    
    m_tipHideTimer = new QTimer(this);
    m_tipHideTimer->setSingleShot(true);
    
    updateTime();
}

/**
 * @brief 初始化开锁按钮
 * 
 * @description 创建开锁按钮并设置样式：
 *              - 尺寸：130x55
 *              - 颜色：绿色渐变（#27ae60 -> #229954）
 *              - 圆角：12px
 *              - 字体：16px，加粗
 *              - 禁用状态：灰色
 * 
 * @note 绿色代表安全、通行
 */
void UIManager::initUnlockBtn()
{
    m_unlockBtn = new QPushButton("开锁", m_mainWindow);
    m_unlockBtn->setFixedSize(130, 55);
    m_unlockBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #27ae60, stop:1 #229954);
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 12px;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #229954, stop:1 #1e8449);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1e8449, stop:1 #27ae60);
        }
        QPushButton:disabled {
            background-color: #7f8c8d;
            color: #bdc3c7;
        }
    )");
}

/**
 * @brief 初始化视频开关按钮
 * 
 * @description 创建视频开关按钮并设置样式：
 *              - 尺寸：130x55
 *              - 可勾选（checkable）
 *              - 未选中：蓝色渐变（#3498db -> #2980b9）
 *              - 选中：红色渐变（#e74c3c -> #c0392b）
 *              - 圆角：12px
 *              - 字体：16px，加粗
 * 
 * @note 选中状态表示视频正在推流
 *       红色表示正在运行（醒目）
 */
void UIManager::initStartVideoBtn()
{
    m_startVideoBtn = new QPushButton("打开视频", m_mainWindow);
    m_startVideoBtn->setFixedSize(130, 55);
    m_startVideoBtn->setCheckable(true);
    m_startVideoBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3498db, stop:1 #2980b9);
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 12px;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2980b9, stop:1 #2471a3);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2471a3, stop:1 #3498db);
        }
        QPushButton:checked {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e74c3c, stop:1 #c0392b);
        }
        QPushButton:disabled {
            background-color: #7f8c8d;
            color: #bdc3c7;
        }
    )");
}

/**
 * @brief 初始化修改密码按钮
 * 
 * @description 创建修改密码按钮并设置样式：
 *              - 尺寸：130x55
 *              - 颜色：橙色渐变（#f39c12 -> #e67e22）
 *              - 圆角：12px
 *              - 字体：16px，加粗
 * 
 * @note 橙色代表警告、重要操作
 */
void UIManager::initChangePwdBtn()
{
    m_changePwdBtn = new QPushButton("修改密码", m_mainWindow);
    m_changePwdBtn->setFixedSize(130, 55);
    m_changePwdBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f39c12, stop:1 #e67e22);
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 12px;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e67e22, stop:1 #d35400);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #d35400, stop:1 #f39c12);
        }
        QPushButton:disabled {
            background-color: #7f8c8d;
            color: #bdc3c7;
        }
    )");
}

/**
 * @brief 初始化发送通知按钮
 * 
 * @description 创建发送通知按钮并设置样式：
 *              - 尺寸：130x55
 *              - 颜色：紫色渐变（#9b59b6 -> #8e44ad）
 *              - 圆角：12px
 *              - 字体：16px，加粗
 * 
 * @note 紫色代表通信、消息
 */
void UIManager::initSendNotifyBtn()
{
    m_sendNotifyBtn = new QPushButton("发送通知", m_mainWindow);
    m_sendNotifyBtn->setFixedSize(130, 55);
    m_sendNotifyBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #9b59b6, stop:1 #8e44ad);
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 12px;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #8e44ad, stop:1 #7d3c98);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7d3c98, stop:1 #9b59b6);
        }
        QPushButton:disabled {
            background-color: #7f8c8d;
            color: #bdc3c7;
        }
    )");
}

/**
 * @brief 初始化人脸库按钮
 * 
 * @description 创建人脸库按钮并设置样式：
 *              - 尺寸：130x55
 *              - 颜色：青绿色渐变（#1abc9c -> #16a085）
 *              - 圆角：12px
 *              - 字体：16px，加粗
 * 
 * @note 青绿色代表数据、管理
 */
void UIManager::initOpenFaceDbBtn()
{
    m_btnOpenFaceDb = new QPushButton("人脸库", m_mainWindow);
    m_btnOpenFaceDb->setFixedSize(130, 55);
    m_btnOpenFaceDb->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1abc9c, stop:1 #16a085);
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 12px;
            border: none;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #16a085, stop:1 #138d75);
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #138d75, stop:1 #1abc9c);
        }
        QPushButton:disabled {
            background-color: #7f8c8d;
            color: #bdc3c7;
        }
    )");
}

/**
 * @brief 初始化视频标签
 * 
 * @description 创建视频显示标签并设置样式：
 *              - 最小尺寸：480x360
 *              - 尺寸策略：可扩展
 *              - 默认文本："未获取到图像数据"
 *              - 背景：半透明黑色
 *              - 边框：深蓝色，2px
 *              - 字体：16px，浅色
 *              - 对齐：居中
 * 
 * @note 视频标签占据界面左侧大部分区域
 */
void UIManager::initVideoLabel()
{
    m_videoLabel = new QLabel(m_mainWindow);
    m_videoLabel->setMinimumSize(480, 360);
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoLabel->setText("未获取到图像数据");
    m_videoLabel->setStyleSheet(R"(
        QLabel {
            color: #ecf0f1;
            background-color: rgba(0, 0, 0, 0.3);
            border: 2px solid #34495e;
            border-radius: 8px;
            font-size: 16px;
        }
    )");
    m_videoLabel->setAlignment(Qt::AlignCenter);
}

/**
 * @brief 初始化连接状态标签
 * 
 * @description 创建连接状态标签并设置样式：
 *              - 文本："连接状态：未连接"
 *              - 尺寸：200x32
 *              - 位置：左上角 (15, 15)
 *              - 颜色：红色（未连接）
 *              - 背景：半透明深蓝
 *              - 字体：13px，加粗
 * 
 * @note 状态会在 updateConnectionStatus() 中动态更新
 */
void UIManager::initConnStatusLabel()
{
    m_connStatusLabel = new QLabel("连接状态：未连接", m_mainWindow);
    m_connStatusLabel->setStyleSheet(R"(
        QLabel {
            color: #e74c3c;
            font-size: 13px;
            font-weight: bold;
            background-color: rgba(52, 73, 94, 0.8);
            padding: 5px 12px;
            border-radius: 6px;
            border: 1px solid rgba(231, 76, 60, 0.5);
        }
    )");
    m_connStatusLabel->setFixedSize(200, 32);
    m_connStatusLabel->move(15, 15);
}

/**
 * @brief 初始化门铃提示标签
 * 
 * @description 创建门铃提示标签并设置样式：
 *              - 尺寸：340x60
 *              - 背景：半透明红色
 *              - 字体：18px，加粗
 *              - 对齐：居中
 *              - 默认隐藏
 * 
 * @note 用于显示门铃、人脸识别等提示信息
 */
void UIManager::initDoorbellTipLabel()
{
    m_doorbellTipLabel = new QLabel(m_mainWindow);
    m_doorbellTipLabel->setFixedSize(340, 60);
    m_doorbellTipLabel->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 18px;
            font-weight: bold;
            background-color: rgba(255, 0, 0, 0.7);
            padding: 15px;
            border-radius: 10px;
            text-align: center;
        }
    )");
    m_doorbellTipLabel->setAlignment(Qt::AlignCenter);
    m_doorbellTipLabel->hide();
}

/**
 * @brief 初始化时间标签
 * 
 * @description 创建时间显示标签并设置样式：
 *              - 尺寸：200x32
 *              - 对齐：居中
 *              - 背景：半透明深蓝
 *              - 字体：14px，加粗
 *              - 边框：浅色
 * 
 * @note 时间每秒更新一次
 */
void UIManager::initTimeLabel()
{
    m_timeLabel = new QLabel(m_mainWindow);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet(R"(
        QLabel {
            color: #ecf0f1;
            font-size: 14px;
            font-weight: bold;
            background-color: rgba(52, 73, 94, 0.8);
            padding: 5px 15px;
            border-radius: 6px;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
    )");
    m_timeLabel->setFixedSize(200, 32);
}

/**
 * @brief 显示提示消息
 * 
 * @param tipText 提示文本内容
 * @param durationMs 显示时长（毫秒，默认 3000ms）
 * @param bgColor 背景颜色（默认半透明红色）
 * @param textColor 文字颜色（默认白色）
 * 
 * @description 在视频标签中央显示提示消息：
 *              1. 检查门铃提示标签是否有效
 *              2. 动态生成样式表（支持自定义颜色）
 *              3. 计算居中位置（相对于视频标签）
 *              4. 设置文本和样式
 *              5. 显示标签
 *              6. 设置定时器自动隐藏
 * 
 * @note 使用动态样式表实现颜色自定义
 *       定时器超时后自动隐藏标签
 */
void UIManager::showTipMessage(const QString& tipText,
                               int durationMs,
                               const QColor& bgColor,
                               const QColor& textColor)
{
    if (!m_doorbellTipLabel) {
        qDebug() << "错误：门铃提示标签未初始化！";
        return;
    }

    QString styleSheet = QString(R"(
        QLabel {
            color: %1;
            font-size: 20px;
            font-weight: bold;
            background-color: rgba(%2, %3, %4, %5);
            padding: 10px;
            border-radius: 10px;
            text-align: center;
        }
    )").arg(textColor.name())
      .arg(bgColor.red())
      .arg(bgColor.green())
      .arg(bgColor.blue())
      .arg(bgColor.alpha());

    if (m_videoLabel) {
        int x = m_videoLabel->x() + (m_videoLabel->width() - m_doorbellTipLabel->width()) / 2;
        int y = m_videoLabel->y() + (m_videoLabel->height() - m_doorbellTipLabel->height()) / 2;
        m_doorbellTipLabel->move(x, y);
    }

    m_doorbellTipLabel->setText(tipText);
    m_doorbellTipLabel->setStyleSheet(styleSheet);
    m_doorbellTipLabel->show();

    if (m_tipHideTimer) {
        m_tipHideTimer->stop();
        m_tipHideTimer->setInterval(durationMs);
        m_tipHideTimer->start();
        connect(m_tipHideTimer, &QTimer::timeout, m_doorbellTipLabel, &QLabel::hide, Qt::UniqueConnection);
    }
}

/**
 * @brief 更新连接状态
 * 
 * @param connected 是否已连接
 * 
 * @description 更新连接状态标签的显示：
 *              1. 检查标签是否有效
 *              2. 根据连接状态设置：
 *                 - 已连接：
 *                   - 文本："连接状态：已连接"
 *                   - 颜色：绿色（#2ecc71）
 *                   - 边框：绿色
 *                 - 未连接：
 *                   - 文本："连接状态：未连接"
 *                   - 颜色：红色（#e74c3c）
 *                   - 边框：红色
 * 
 * @note 绿色表示正常，红色表示异常
 */
void UIManager::updateConnectionStatus(bool connected)
{
    if (!m_connStatusLabel) return;

    if (connected) {
        m_connStatusLabel->setText("连接状态：已连接");
        m_connStatusLabel->setStyleSheet(R"(
            QLabel {
                color: #2ecc71;
                font-size: 13px;
                font-weight: bold;
                background-color: rgba(52, 73, 94, 0.8);
                padding: 5px 12px;
                border-radius: 6px;
                border: 1px solid rgba(46, 204, 113, 0.5);
            }
        )");
    } else {
        m_connStatusLabel->setText("连接状态：未连接");
        m_connStatusLabel->setStyleSheet(R"(
            QLabel {
                color: #e74c3c;
                font-size: 13px;
                font-weight: bold;
                background-color: rgba(52, 73, 94, 0.8);
                padding: 5px 12px;
                border-radius: 6px;
                border: 1px solid rgba(231, 76, 60, 0.5);
            }
        )");
    }
}

/**
 * @brief 更新时间显示
 * 
 * @description 获取当前系统时间并更新标签：
 *              1. 检查标签是否有效
 *              2. 获取当前时间（QDateTime::currentDateTime）
 *              3. 格式化为字符串："yyyy-MM-dd hh:mm:ss"
 *              4. 设置到标签
 * 
 * @note 每秒调用一次
 *       格式：2024-01-15 14:30:45
 */
void UIManager::updateTime()
{
    if (!m_timeLabel) return;
    
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timeStr = currentTime.toString("yyyy-MM-dd hh:mm:ss");
    m_timeLabel->setText(timeStr);
}
