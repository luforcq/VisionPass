// facedbdialog.h
#ifndef FACEDBDIALOG_H
#define FACEDBDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include "facedbmanager.h"
#include "faceservice.h"
#include <QInputDialog> // 解决 QInputDialog 未声明
#include <QLineEdit>    // 解决 QLineEdit::Normal 未定义
#include <QDebug>
#include <QFileInfo> // 新增：用于提取文件名
#include <QVariant>
class FaceDbDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FaceDbDialog(FaceRecognizerThread* thread, QWidget *parent = nullptr);
    ~FaceDbDialog() override;

private slots:
    void onWriteCardClicked();

    // 刷新人脸列表
    void refreshFaceList();
    // 添加新人脸
    void addNewFace();
    // 删除选中人脸
    void deleteSelectedFace();
    // 清空所有人脸
    void clearAllFaces();
    // 选中列表项时显示预览图
    void onItemSelected(QListWidgetItem* item);

private:
    FaceRecognizerThread* m_recognizerThread;
    QListWidget* m_faceListWidget;
    QLabel* m_previewLabel;
    QLabel* m_idLabel;
    QLabel* m_timeLabel;
    QLabel *m_nameLabel;
    QLabel *m_identityLabel;
    
    void initUi();
    void loadFaceItems();

signals:
         void writeCardRequested(const QString& id, const QString& name);
          void syncUsersRequested(const QStringList& userIds);  // 请求同步用户列表
};

#endif // FACEDBDIALOG_H
