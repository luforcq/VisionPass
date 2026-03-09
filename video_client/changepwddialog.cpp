/******************************************************************
* @projectName   video_client
* @brief         changepwddialog.cpp - 修改密码对话框实现
* @description   本文件实现了修改密码对话框，提供虚拟键盘输入界面
* @features      1. 数字虚拟键盘输入（防键盘记录）
*                2. 密码长度验证（6-16 位）
*                3. 密码星号显示（安全保护）
*                4. 删除/确认/取消功能
* @author        Lu Yaolong
* @date          2026-03-01
*******************************************************************/
#include "changepwddialog.h"
#include <QDebug>
#include <QMessageBox>

/**
 * @brief 构造函数
 * @param parent 父窗口指针
 * @description 初始化对话框 UI 界面
 */
ChangePwdDialog::ChangePwdDialog(QWidget *parent)
    : QDialog(parent)
{
    initUi();
}

/**
 * @brief 析构函数
 * @description 清理资源（Qt 对象树自动管理子对象内存）
 */
ChangePwdDialog::~ChangePwdDialog()
{
}

/**
 * @brief 初始化 UI 界面
 * @description 创建并布局所有 UI 元素：
 *              1. 密码显示标签（60px 高度，圆角边框）
 *              2. 提示标签（密码长度要求）
 *              3. 数字键盘（0-9，电话键盘布局）
 *              4. 功能按钮（删除/确认/取消）
 */
void ChangePwdDialog::initUi()
{
    setWindowTitle("修改密码");
    setFixedSize(400, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 密码显示标签 - 显示星号密码
    pwdDisplayLabel = new QLabel("请输入密码", this);
    pwdDisplayLabel->setFixedHeight(60);
    pwdDisplayLabel->setStyleSheet(R"(
        QLabel {
            background-color: #f0f0f0;
            border: 2px solid #ccc;
            border-radius: 8px;
            font-size: 24px;
            color: #333;
            padding: 10px;
            qproperty-alignment: AlignCenter;
        }
    )");
    mainLayout->addWidget(pwdDisplayLabel);

    // 提示标签 - 显示密码长度要求
    QLabel *tipLabel = new QLabel("密码长度要求：6-16 位", this);
    tipLabel->setStyleSheet("color: #666; font-size: 14px;");
    tipLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(tipLabel);

    // 数字按钮网格布局 - 电话键盘布局
    QGridLayout *numGrid = new QGridLayout();
    numGrid->setSpacing(10);

    // 创建 0-9 数字按钮
    for (int i = 0; i < 10; i++) {
        numButtons[i] = new QPushButton(QString::number(i), this);
        numButtons[i]->setFixedSize(80, 60);
        numButtons[i]->setStyleSheet(R"(
            QPushButton {
                background-color: #2196F3;
                color: white;
                font-size: 24px;
                font-weight: bold;
                border-radius: 8px;
            }
            QPushButton:hover {
                background-color: #1976D2;
            }
            QPushButton:pressed {
                background-color: #0D47A1;
            }
        )");
        connect(numButtons[i], &QPushButton::clicked, this, &ChangePwdDialog::onNumberClicked);
    }

    // 布局数字按钮 (1-9 按电话键盘布局)
    // 1 2 3
    // 4 5 6
    // 7 8 9
    //   0
    numGrid->addWidget(numButtons[1], 0, 0);
    numGrid->addWidget(numButtons[2], 0, 1);
    numGrid->addWidget(numButtons[3], 0, 2);
    numGrid->addWidget(numButtons[4], 1, 0);
    numGrid->addWidget(numButtons[5], 1, 1);
    numGrid->addWidget(numButtons[6], 1, 2);
    numGrid->addWidget(numButtons[7], 2, 0);
    numGrid->addWidget(numButtons[8], 2, 1);
    numGrid->addWidget(numButtons[9], 2, 2);
    numGrid->addWidget(numButtons[0], 3, 1);

    mainLayout->addLayout(numGrid);

    // 功能按钮布局
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(15);

    // 删除按钮 - 删除最后一位密码
    deleteBtn = new QPushButton("删除", this);
    deleteBtn->setFixedSize(100, 50);
    deleteBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #FF9800;
            color: white;
            font-size: 18px;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #F57C00;
        }
    )");
    connect(deleteBtn, &QPushButton::clicked, this, &ChangePwdDialog::onDeleteClicked);

    // 确认按钮 - 提交密码
    confirmBtn = new QPushButton("确认", this);
    confirmBtn->setFixedSize(100, 50);
    confirmBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #4CAF50;
            color: white;
            font-size: 18px;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #388E3C;
        }
    )");
    connect(confirmBtn, &QPushButton::clicked, this, &ChangePwdDialog::onConfirmClicked);

    // 取消按钮 - 关闭对话框
    cancelBtn = new QPushButton("取消", this);
    cancelBtn->setFixedSize(100, 50);
    cancelBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #f44336;
            color: white;
            font-size: 18px;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #d32f2f;
        }
    )");
    connect(cancelBtn, &QPushButton::clicked, this, &ChangePwdDialog::onCancelClicked);

    btnLayout->addWidget(deleteBtn);
    btnLayout->addWidget(confirmBtn);
    btnLayout->addWidget(cancelBtn);

    mainLayout->addLayout(btnLayout);
    mainLayout->addStretch();
}

/**
 * @brief 数字按钮点击槽函数
 * @description 处理数字键盘输入：
 *              1. 获取发送者按钮（sender()）
 *              2. 检查密码长度限制（最大 16 位）
 *              3. 追加数字到密码字符串
 *              4. 更新密码显示（星号）
 * @note 使用 sender() 动态获取点击的按钮
 */
void ChangePwdDialog::onNumberClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    // 密码最多 16 位
    if (currentPwd.length() >= 16) {
        return;
    }

    currentPwd.append(btn->text());
    updatePwdDisplay();
}

/**
 * @brief 删除按钮点击槽函数
 * @description 删除密码最后一位字符：
 *              1. 检查密码是否为空
 *              2. 使用 chop(1) 删除末尾字符
 *              3. 更新密码显示
 */
void ChangePwdDialog::onDeleteClicked()
{
    if (!currentPwd.isEmpty()) {
        currentPwd.chop(1);
        updatePwdDisplay();
    }
}

/**
 * @brief 确认按钮点击槽函数
 * @description 验证密码并确认修改：
 *              1. 验证密码长度（最少 6 位）
 *              2. 验证密码长度（最多 16 位）
 *              3. 验证通过则调用 accept() 关闭对话框
 * @note 验证失败时弹出警告对话框
 */
void ChangePwdDialog::onConfirmClicked()
{
    // 验证密码长度
    if (currentPwd.length() < 6) {
        QMessageBox::warning(this, "提示", "密码长度不能少于 6 位！");
        return;
    }
    if (currentPwd.length() > 16) {
        QMessageBox::warning(this, "提示", "密码长度不能超过 16 位！");
        return;
    }

    accept();
}

/**
 * @brief 取消按钮点击槽函数
 * @description 取消修改并关闭对话框
 * @note 调用 reject() 返回 QDialog::Rejected
 */
void ChangePwdDialog::onCancelClicked()
{
    reject();
}

/**
 * @brief 更新密码显示
 * @description 更新密码标签的显示内容和样式：
 *              1. 空密码时显示提示文字（灰色）
 *              2. 有密码时显示星号（绿色边框）
 * @note 实际密码存储在 currentPwd 成员变量中
 */
void ChangePwdDialog::updatePwdDisplay()
{
    if (currentPwd.isEmpty()) {
        pwdDisplayLabel->setText("请输入密码");
        pwdDisplayLabel->setStyleSheet(R"(
            QLabel {
                background-color: #f0f0f0;
                border: 2px solid #ccc;
                border-radius: 8px;
                font-size: 24px;
                color: #999;
                padding: 10px;
                qproperty-alignment: AlignCenter;
            }
        )");
    } else {
        // 显示星号代替实际密码
        QString stars;
        for (int i = 0; i < currentPwd.length(); i++) {
            stars += "*";
        }
        pwdDisplayLabel->setText(currentPwd);
        pwdDisplayLabel->setStyleSheet(R"(
            QLabel {
                background-color: #f0f0f0;
                border: 2px solid #4CAF50;
                border-radius: 8px;
                font-size: 24px;
                color: #333;
                padding: 10px;
                qproperty-alignment: AlignCenter;
            }
        )");
    }
}

/**
 * @brief 获取输入的密码
 * @return QString 密码字符串
 * @description 供外部获取用户输入的密码
 * @note 返回的是明文密码
 */
QString ChangePwdDialog::getPassword() const
{
    return currentPwd;
}
