/******************************************************************
* @projectName   video_client
* @brief         changepwddialog.h
* @author        Lu Yaolong
* @date          2026-03-01
*******************************************************************/
#ifndef CHANGEPWDDIALOG_H
#define CHANGEPWDDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

class ChangePwdDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePwdDialog(QWidget *parent = nullptr);
    ~ChangePwdDialog();

    QString getPassword() const;

private slots:
    void onNumberClicked();
    void onDeleteClicked();
    void onConfirmClicked();
    void onCancelClicked();

private:
    void initUi();
    void updatePwdDisplay();

    QLabel *pwdDisplayLabel;
    QPushButton *numButtons[10];
    QPushButton *deleteBtn;
    QPushButton *confirmBtn;
    QPushButton *cancelBtn;

    QString currentPwd;
};

#endif // CHANGEPWDDIALOG_H
