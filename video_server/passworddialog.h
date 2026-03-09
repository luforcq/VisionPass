/******************************************************************
* @projectName   video_server
* @brief         passworddialog.h
* @author        Lu Yaolong
*******************************************************************/
#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QString>
#include <QShowEvent>

class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordDialog(QWidget *parent = nullptr);
    ~PasswordDialog();

    // 获取输入的密码
    QString getInputPassword() const { return inputPwdStr; }

protected:
    void showEvent(QShowEvent *event) override;  // 重写显示事件

private:
    void initUI();
    void initStyle();
    void resetInput();  // 重置密码输入

    QLabel *pwdInputDisplay;       // 密码输入显示框
    QPushButton *numButtons[10];   // 0-9 数字按键
    QPushButton *confirmButton;    // 确认按键
    QPushButton *deleteButton;     // 删除按键
    QGridLayout *keypadLayout;     // 键盘布局
    QString inputPwdStr;           // 存储输入的密码字符串

signals:
    void passwordConfirmed(const QString &password);  // 密码确认信号
    void passwordDeleted();                            // 密码删除信号

private slots:
    void numButtonClicked();
    void confirmButtonClicked();
    void deleteButtonClicked();
    void closeButtonClicked();  // 关闭按钮点击
};

#endif // PASSWORDDIALOG_H
