/******************************************************************
* @projectName   video_client
* @brief         notifydialog.cpp - 通知消息对话框实现
* @description   本文件实现了通知消息发送对话框，用于向门禁端发送预设通知
* @features      1. 预设通知消息选择（下拉框）
*                2. 显示时长选择（3-20 秒）
*                3. 现代化 UI 样式（圆角按钮、渐变效果）
*                4. 模态对话框（阻塞父窗口）
* @use_case      用户外出时发送"外卖放门口"、"请不要敲门"等通知
* @author        Lu Yaolong
*******************************************************************/
#include "notifydialog.h"
#include <QMessageBox>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 * @description 初始化通知对话框 UI 界面：
 *              1. 设置窗口标题和大小（350x240）
 *              2. 创建消息选择下拉框（4 条预设消息）
 *              3. 创建时长选择下拉框（6 个时长选项）
 *              4. 创建确认/取消按钮
 *              5. 应用现代化样式表
 * @note 使用模态对话框（setModal(true)）
 */
NotifyDialog::NotifyDialog(QWidget *parent)
    : QDialog(parent)
{
    this->setWindowTitle("发送通知");
    this->setFixedSize(350, 240);
    this->setModal(true);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(25, 20, 25, 20);

    // 创建消息选择下拉框
    QLabel *msgLabel = new QLabel("选择通知消息：");
    msgLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
    mainLayout->addWidget(msgLabel);

    msgComboBox = new QComboBox(this);
    msgComboBox->setFixedSize(280, 45);
    msgComboBox->setStyleSheet(R"(
        QComboBox {
            background-color: white;
            color: black;
            font-size: 14px;
            border-radius: 8px;
            padding-left: 15px;
            border: 1px solid #ddd;
        }
        QComboBox::drop-down {
            border: none;
            padding-right: 10px;
        }
        QComboBox QAbstractItemView {
            background-color: white;
            color: black;
            selection-background-color: #4CAF50;
            border: 1px solid #ddd;
        }
        QComboBox:hover {
            border: 1px solid #4CAF50;
        }
    )");
    msgComboBox->addItems({
        "外卖放门口就好",
        "快递放门口就好",
        "请不要敲门",
        "家里没有人",
    });
    mainLayout->addWidget(msgComboBox);

    // 添加间距
    mainLayout->addSpacing(5);

    // 创建时长选择下拉框
    QLabel *timeLabel = new QLabel("选择显示时长：");
    timeLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
    mainLayout->addWidget(timeLabel);

    timeComboBox = new QComboBox(this);
    timeComboBox->setFixedSize(280, 45);
    timeComboBox->setStyleSheet(msgComboBox->styleSheet());
    timeComboBox->addItems({"3 秒", "5 秒", "8 秒", "10 秒", "15 秒", "20 秒"});
    timeComboBox->setCurrentIndex(1); // 默认选中 5 秒
    mainLayout->addWidget(timeComboBox);

    // 添加弹性空间，将按钮推到底部
    mainLayout->addStretch();

    // 创建按钮布局
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);

    confirmBtn = new QPushButton("确认");
    confirmBtn->setFixedSize(110, 42);
    confirmBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #4CAF50;
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #45a049;
        }
        QPushButton:pressed {
            background-color: #3d8b40;
        }
    )");
    connect(confirmBtn, &QPushButton::clicked, this, &NotifyDialog::onConfirmClicked);

    cancelBtn = new QPushButton("取消");
    cancelBtn->setFixedSize(110, 42);
    cancelBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #9E9E9E;
            color: white;
            font-size: 16px;
            font-weight: bold;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #757575;
        }
        QPushButton:pressed {
            background-color: #616161;
        }
    )");
    connect(cancelBtn, &QPushButton::clicked, this, &NotifyDialog::onCancelClicked);

    btnLayout->addStretch();
    btnLayout->addWidget(confirmBtn);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
}

/**
 * @brief 析构函数
 * @description 清理资源（Qt 对象树自动管理子对象内存）
 */
NotifyDialog::~NotifyDialog()
{
}

/**
 * @brief 获取选中的通知消息
 * @return QString 选中的消息文本
 * @description 从消息下拉框获取当前选中的通知内容
 * @note 返回的字符串将作为网络消息发送到门禁端
 */
QString NotifyDialog::getSelectedMessage() const
{
    return msgComboBox->currentText();
}

/**
 * @brief 获取选中的显示时长
 * @return int 时长（单位：秒）
 * @description 从时长下拉框获取当前选中的显示时长：
 *              1. 获取当前文本（如"5 秒"）
 *              2. 截取数字部分（去掉"秒"字）
 *              3. 转换为整数
 * @note 默认返回 5 秒（索引 1）
 */
int NotifyDialog::getSelectedTime() const
{
    QString timeStr = timeComboBox->currentText();
    return timeStr.left(timeStr.indexOf("秒")).toInt();
}

/**
 * @brief 确认按钮点击槽函数
 * @description 确认发送通知并关闭对话框
 * @note 调用 accept() 返回 QDialog::Accepted
 */
void NotifyDialog::onConfirmClicked()
{
    accept();
}

/**
 * @brief 取消按钮点击槽函数
 * @description 取消发送通知并关闭对话框
 * @note 调用 reject() 返回 QDialog::Rejected
 */
void NotifyDialog::onCancelClicked()
{
    reject();
}
