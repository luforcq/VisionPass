/******************************************************************
* @projectName   video_client
* @brief         notifydialog.h
* @author        Lu Yaolong
*******************************************************************/
#ifndef NOTIFYDIALOG_H
#define NOTIFYDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class NotifyDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NotifyDialog(QWidget *parent = nullptr);
    ~NotifyDialog();

    QString getSelectedMessage() const;
    int getSelectedTime() const;

private:
    QComboBox *msgComboBox;
    QComboBox *timeComboBox;
    QPushButton *confirmBtn;
    QPushButton *cancelBtn;

private slots:
    void onConfirmClicked();
    void onCancelClicked();
};

#endif // NOTIFYDIALOG_H
