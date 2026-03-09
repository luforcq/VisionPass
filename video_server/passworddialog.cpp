/******************************************************************
* @projectName   video_server
* @brief         passworddialog.cpp
* @author        Lu Yaolong
*******************************************************************/
#include "passworddialog.h"
#include <QVBoxLayout>
#include <QMessageBox>

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    initUI();
    initStyle();
}

PasswordDialog::~PasswordDialog()
{
}

void PasswordDialog::initUI()
{
    // 设置对话框标题和模态属性
    setWindowTitle("密码输入");
    setModal(true);  // 模态窗口
    setFixedSize(320, 450);  // 增加高度以容纳关闭按钮
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);
    
    // 1. 密码输入显示框
    pwdInputDisplay = new QLabel(this);
    pwdInputDisplay->setFixedSize(290, 50);
    pwdInputDisplay->setAlignment(Qt::AlignCenter);
    pwdInputDisplay->setText("");
    
    // 2. 创建数字键盘布局
    keypadLayout = new QGridLayout();
    keypadLayout->setSpacing(6);  // 紧凑间距
    keypadLayout->setContentsMargins(0, 0, 0, 0);
    
    // 创建 0-9 数字按键
    QString numKeyStyle = "QPushButton {background-color: #f5f5f5; border-radius: 8px; color: #333; font-size: 20px; font-weight: bold;}"
                          "QPushButton:pressed {background-color: #e0e0e0;}";
    
    for (int i = 0; i < 10; ++i) {
        numButtons[i] = new QPushButton(QString::number(i), this);
        numButtons[i]->setFixedSize(80, 60);
        numButtons[i]->setStyleSheet(numKeyStyle);
    }
    
    // 确认键（绿色）
    confirmButton = new QPushButton("确认", this);
    confirmButton->setFixedSize(80, 60);
    confirmButton->setStyleSheet("QPushButton {background-color: #4CAF50; border-radius: 8px; color: white; font-size: 18px; font-weight: bold;}"
                                 "QPushButton:pressed {background-color: #45a049;}");
    
    // 删除键（红色）
    deleteButton = new QPushButton("删除", this);
    deleteButton->setFixedSize(80, 60);
    deleteButton->setStyleSheet("QPushButton {background-color: #F44336; border-radius: 8px; color: white; font-size: 18px; font-weight: bold;}"
                                "QPushButton:pressed {background-color: #da190b;}");
    
    // 关闭键（灰色）
    QPushButton *closeButton = new QPushButton("关闭", this);
    closeButton->setFixedSize(80, 60);
    closeButton->setStyleSheet("QPushButton {background-color: #9E9E9E; border-radius: 8px; color: white; font-size: 18px; font-weight: bold;}"
                               "QPushButton:pressed {background-color: #757575;}");
    
    // 3. 排列布局
    // 第 0 行：密码显示框
    keypadLayout->addWidget(pwdInputDisplay, 0, 0, 1, 3, Qt::AlignCenter);
    
    // 第 1-3 行：数字键 1-9
    int numIdx = 1;
    for (int row = 1; row <= 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            keypadLayout->addWidget(numButtons[numIdx], row, col);
            numIdx++;
        }
    }
    
    // 第 4 行：0、删除、确认
    keypadLayout->addWidget(numButtons[0], 4, 0);
    keypadLayout->addWidget(deleteButton, 4, 1);
    keypadLayout->addWidget(confirmButton, 4, 2);
    
    // 第 5 行：关闭按钮（居中）
    keypadLayout->addWidget(closeButton, 5, 0, 1, 3, Qt::AlignCenter);
    
    // 添加到主布局
    mainLayout->addStretch();
    mainLayout->addLayout(keypadLayout);
    mainLayout->addStretch();
    
    // 4. 连接信号槽
    for (int i = 0; i < 10; ++i) {
        connect(numButtons[i], &QPushButton::clicked, this, &PasswordDialog::numButtonClicked);
    }
    connect(confirmButton, &QPushButton::clicked, this, &PasswordDialog::confirmButtonClicked);
    connect(deleteButton, &QPushButton::clicked, this, &PasswordDialog::deleteButtonClicked);
    connect(closeButton, &QPushButton::clicked, this, &PasswordDialog::closeButtonClicked);
}

void PasswordDialog::initStyle()
{
    // 设置对话框背景样式
    setStyleSheet("QDialog {background-color: white; border-radius: 15px;}");
}

void PasswordDialog::numButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        // 限制密码长度最多 8 位
        if (inputPwdStr.length() < 8) {
            inputPwdStr += btn->text();
            pwdInputDisplay->setText(inputPwdStr);
            
            // 用圆点隐藏密码（可选，如果需要显示数字则注释掉这行）
            // pwdInputDisplay->setText(QString("*").repeated(inputPwdStr.length()));
        }
    }
}

void PasswordDialog::deleteButtonClicked()
{
    if (!inputPwdStr.isEmpty()) {
        inputPwdStr.chop(1);
        pwdInputDisplay->setText(inputPwdStr);
        // pwdInputDisplay->setText(QString("*").repeated(inputPwdStr.length()));
    }
}

void PasswordDialog::confirmButtonClicked()
{
    if (inputPwdStr.isEmpty()) {
        pwdInputDisplay->setStyleSheet("QLabel {background-color: #ffebee; color: #F44336; font-size: 20px; border-radius: 8px;}");
        pwdInputDisplay->setText("请输入密码");
        return;
    }
    
    // 发射密码确认信号，传递密码给主窗口
    emit passwordConfirmed(inputPwdStr);
    
    // 重置输入并关闭对话框
    resetInput();
    accept();
}

void PasswordDialog::closeButtonClicked()
{
    // 重置输入并关闭对话框
    resetInput();
    reject();
}

void PasswordDialog::resetInput()
{
    inputPwdStr.clear();
    pwdInputDisplay->setText("");
    pwdInputDisplay->setStyleSheet("QLabel {background-color: white; color: black; font-size: 24px; border-radius: 8px;}");
}

// 重写 showEvent，每次显示时重置输入
void PasswordDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    resetInput();
}
