/******************************************************************
* @projectName   video_client
* @brief         facedbdialog.cpp - 人脸数据库管理对话框实现
* @description   本文件实现了人脸数据库管理对话框，提供人脸增删改查功能
* @features      1. 人脸列表显示（QListWidget）
*                2. 人脸预览与信息展示
*                3. 添加人脸（从图片文件注册）
*                4. 删除选中人脸
*                5. 清空所有人脸
*                6. 写 ID 卡功能（RFID 卡片写入）
*                7. 用户数据同步到嵌入式端
* @author        Lu Yaolong
*******************************************************************/
#include "facedbdialog.h"
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileInfo>

/**
 * @brief 构造函数
 * @param thread 人脸识别线程指针（用于注册人脸）
 * @param parent 父窗口指针
 * @description 初始化人脸数据库管理对话框：
 *              1. 确保数据库已初始化
 *              2. 初始化 UI 界面
 *              3. 刷新人脸列表
 */
FaceDbDialog::FaceDbDialog(FaceRecognizerThread *thread, QWidget *parent)
    : QDialog(parent), m_recognizerThread(thread)
{
    // 确保数据库已初始化
    auto dbMgr = FaceDbManager::getInstance();
    if (!dbMgr->getDbConnection().isOpen()) {
        dbMgr->initDb();
    }
    
    initUi();
    refreshFaceList();
}

/**
 * @brief 析构函数
 * @description 清理资源（Qt 对象树自动管理子对象内存）
 */
FaceDbDialog::~FaceDbDialog()
{
}

/**
 * @brief 初始化 UI 界面
 * @description 创建并布局所有 UI 元素：
 *              1. 左侧：人脸列表（QListWidget）
 *              2. 右侧：人脸预览 + 详细信息（姓名、ID、身份、注册时间）
 *              3. 底部：功能按钮（添加/删除/写卡/清空/刷新）
 * @note 使用水平布局分割左右区域
 */
void FaceDbDialog::initUi()
{
    setWindowTitle("人脸数据库管理");
    setFixedSize(600, 400);

    // 布局：左侧列表，右侧预览 + 信息，底部按钮
    QHBoxLayout* mainLayout = new QHBoxLayout();
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QVBoxLayout* rightLayout = new QVBoxLayout();
    QHBoxLayout* btnLayout = new QHBoxLayout();

    m_faceListWidget = new QListWidget();
    m_faceListWidget->setMinimumWidth(200);
    connect(m_faceListWidget, &QListWidget::itemClicked, this, &FaceDbDialog::onItemSelected);
    leftLayout->addWidget(new QLabel("已注册人脸列表"));
    leftLayout->addWidget(m_faceListWidget);

    m_previewLabel = new QLabel();
    m_previewLabel->setFixedSize(150, 150);
    m_previewLabel->setStyleSheet("border:1px solid #ccc;");
    m_previewLabel->setAlignment(Qt::AlignCenter);

    m_nameLabel = new QLabel("姓名：");
    m_idLabel = new QLabel("ID：");
    m_identityLabel = new QLabel("身份：");
    m_timeLabel = new QLabel("注册时间：");

    m_nameLabel->setStyleSheet("color:#333; font-weight:500;");
    m_identityLabel->setStyleSheet("color:#666;");

    rightLayout->addWidget(new QLabel("人脸预览"));
    rightLayout->addWidget(m_previewLabel);
    rightLayout->addSpacing(20);
    rightLayout->addWidget(m_nameLabel);
    rightLayout->addWidget(m_idLabel);
    rightLayout->addWidget(m_identityLabel);
    rightLayout->addWidget(m_timeLabel);
    rightLayout->addStretch();

    QPushButton* addBtn = new QPushButton("添加人脸");
    QPushButton* deleteBtn = new QPushButton("删除选中");
    QPushButton* clearBtn = new QPushButton("清空所有");
    QPushButton* refreshBtn = new QPushButton("刷新列表");
    QPushButton* writeCardBtn = new QPushButton("写 ID 卡");

    connect(deleteBtn, &QPushButton::clicked, this, &FaceDbDialog::deleteSelectedFace);
    connect(clearBtn, &QPushButton::clicked, this, &FaceDbDialog::clearAllFaces);
    connect(refreshBtn, &QPushButton::clicked, this, &FaceDbDialog::refreshFaceList);
    connect(addBtn, &QPushButton::clicked, this, &FaceDbDialog::addNewFace);
    connect(writeCardBtn, &QPushButton::clicked, this, &FaceDbDialog::onWriteCardClicked);

    // 连接人脸识别线程的操作完成信号
    connect(m_recognizerThread, &FaceRecognizerThread::operateFinished, this, [=](bool success, const QString& msg){
        if (success) {
            QMessageBox::information(this, "成功", msg);
            refreshFaceList();
        } else {
            QMessageBox::critical(this, "失败", msg);
        }
    });
    
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(deleteBtn);
    btnLayout->addWidget(writeCardBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addWidget(refreshBtn);
    btnLayout->addStretch();
    btnLayout->insertStretch(0);

    mainLayout->addLayout(leftLayout);
    mainLayout->addLayout(rightLayout);
    mainLayout->addStretch();

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(mainLayout);
    rootLayout->addLayout(btnLayout);
}

/**
 * @brief 写 ID 卡按钮点击槽函数
 * @description 将选中的人脸信息写入 RFID 卡片：
 *              1. 检查是否选中人脸
 *              2. 获取人脸 ID 和姓名
 *              3. 发射 writeCardRequested 信号（由硬件模块处理）
 * @note ID 卡写入需要硬件支持（RC522 RFID 模块）
 */
void FaceDbDialog::onWriteCardClicked()
{
    QListWidgetItem* selectedItem = m_faceListWidget->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, "Notice", "Please select a person to write the card.");
        return;
    }

    QString id = selectedItem->data(Qt::UserRole+3).toString();
    QString name = selectedItem->data(Qt::UserRole+2).toString();
    if (id.isEmpty()) {
        QMessageBox::warning(this, "Notice", "Invalid selection. Please choose again.");
        return;
    }

    emit writeCardRequested(id, name);
}

/**
 * @brief 刷新人脸列表
 * @description 重新加载数据库中的所有人脸数据：
 *              1. 清空列表控件
 *              2. 从数据库加载人脸记录
 *              3. 清空预览区域
 *              4. 构建用户信息列表
 *              5. 发射 syncUsersRequested 信号（同步到嵌入式端）
 * @note 每次刷新后都会触发用户数据同步
 */
void FaceDbDialog::refreshFaceList()
{
    m_faceListWidget->clear();
    loadFaceItems();
    
    m_previewLabel->clear();
    m_nameLabel->setText("姓名：");
    m_idLabel->setText("ID：");
    m_identityLabel->setText("身份：");
    m_timeLabel->setText("注册时间：");

    QStringList userInfoList;
    auto dbMgr = FaceDbManager::getInstance();
    QList<FaceRecord> records = dbMgr->getAllFaces();
    for (const auto& record : records) {
        QString userInfo = QString("%1|%2").arg(record.id).arg(record.name);
        userInfoList.append(userInfo);
    }
    emit syncUsersRequested(userInfoList);
}

/**
 * @brief 加载人脸记录到列表
 * @description 从数据库读取所有人脸记录并添加到列表控件：
 *              1. 获取 FaceDbManager 单例
 *              2. 查询所有人脸记录
 *              3. 为每条记录创建 QListWidgetItem
 *              4. 使用 UserRole 存储元数据（时间、预览图、姓名、ID、身份）
 * @note 使用 Qt::UserRole 存储额外数据是 Qt 列表控件的最佳实践
 */
void FaceDbDialog::loadFaceItems()
{
    auto dbMgr = FaceDbManager::getInstance();
    QList<FaceRecord> records = dbMgr->getAllFaces();
    for (const auto& record : records) {
        QListWidgetItem* item = new QListWidgetItem(record.id);
        item->setData(Qt::UserRole, record.createTime);
        item->setData(Qt::UserRole+1, record.previewData);
        item->setData(Qt::UserRole+2, record.name);
        item->setData(Qt::UserRole+3, record.id);
        item->setData(Qt::UserRole+4, record.identity);
        m_faceListWidget->addItem(item);
    }
}

/**
 * @brief 添加新人脸
 * @description 从图片文件注册新人脸：
 *              1. 打开文件选择对话框（支持 jpg/png/bmp 等格式）
 *              2. 解析文件名（格式：名字_id_身份）
 *              3. 检查 ID 是否已存在
 *              4. 调用 FaceRecognizerThread 注册人脸
 *              5. 刷新列表
 * @note 文件名解析规则：张三_001_业主
 */
void FaceDbDialog::addNewFace()
{
    QString imagePath = QFileDialog::getOpenFileName(
        this, "选择人脸图片", "", "图片文件 (*.jpg *.jpeg *.png *.bmp *.tif);;所有文件 (*.*)");
    
    if (imagePath.isEmpty()) {
        return;
    }

    QVariantHash result;
    QFileInfo fileInfo(imagePath);
    QString fileName = fileInfo.baseName();
    
    QStringList parts = fileName.split("_");
    if (parts.size() < 3) {
        qCritical() << "文件名格式错误！需满足：名字_id_身份，当前：" << fileName;
        QMessageBox::warning(this, "错误", "文件名格式错误！请使用格式：名字_id_身份");
        return;
    }

    result["name"] = parts[0].trimmed();
    result["id"] = parts[1].trimmed();
    result["identity"] = parts[2].trimmed();

    qDebug() << "解析文件名结果：" << result;

    QString id = result["id"].toString();
    auto dbMgr = FaceDbManager::getInstance();
    if (dbMgr->isFaceExists(id)) {
        int ret = QMessageBox::question(this, "ID 已存在", "ID【" + id + "】已注册，是否覆盖原有数据？");
        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    bool success = m_recognizerThread->registerFaceByImagePath(imagePath, result);
    if (success) {
        QMessageBox::information(this, "成功", "人脸【" + id + "】添加成功！");
        refreshFaceList();
    } else {
        QMessageBox::critical(this, "失败", "人脸【" + id + "】添加失败，请检查模型是否加载！");
    }
}

/**
 * @brief 删除选中的人脸
 * @description 从数据库中删除选中的人脸记录：
 *              1. 检查是否选中人脸
 *              2. 获取人脸 ID
 *              3. 确认删除（二次确认）
 *              4. 调用 FaceDbManager 删除
 *              5. 刷新列表
 * @note 删除操作不可恢复，需要二次确认
 */
void FaceDbDialog::deleteSelectedFace()
{
    QListWidgetItem* selectedItem = m_faceListWidget->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, "提示", "请先选中要删除的人脸！");
        return;
    }
    QString id = selectedItem->text();

    int ret = QMessageBox::question(this, "确认删除", "是否删除 ID 为【" + id + "】的人脸？此操作不可恢复！");
    if (ret != QMessageBox::Yes) {
        return;
    }

    auto dbMgr = FaceDbManager::getInstance();
    if (dbMgr->deleteFace(id)) {
        QMessageBox::information(this, "成功", "人脸【" + id + "】删除成功！");
        refreshFaceList();
    } else {
        QMessageBox::critical(this, "失败", "人脸【" + id + "】删除失败，请检查数据库连接！");
    }
}

/**
 * @brief 清空所有人脸
 * @description 清空数据库中所有人脸记录：
 *              1. 显示当前人脸数量
 *              2. 二次确认（QMessageBox::question）
 *              3. 调用 FaceDbManager 清空
 *              4. 刷新列表
 * @note 危险操作，默认选中"否"防止误操作
 */
void FaceDbDialog::clearAllFaces()
{
    int ret = QMessageBox::question(
        this, "确认清空",
        "是否清空所有人脸数据？此操作不可恢复！\n当前已注册人脸数量：" + QString::number(m_faceListWidget->count()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    if (ret != QMessageBox::Yes) {
        return;
    }

    auto dbMgr = FaceDbManager::getInstance();
    if (dbMgr->clearAllFaces()) {
        QMessageBox::information(this, "成功", "人脸库已清空！");
        refreshFaceList();
    } else {
        QMessageBox::critical(this, "失败", "清空人脸库失败，请检查数据库连接！");
    }
}

/**
 * @brief 列表项选中槽函数
 * @description 显示选中人脸的详细信息：
 *              1. 获取列表项数据（姓名、ID、身份、时间、预览图）
 *              2. 更新右侧信息标签
 *              3. 加载并显示预览图
 * @note 预览图从 UserRole+1 中读取（QByteArray 格式）
 */
void FaceDbDialog::onItemSelected(QListWidgetItem *item)
{
    if (!item) return;

    QString name = item->data(Qt::UserRole+2).toString();
    QString id = item->data(Qt::UserRole+3).toString();
    QString identity = item->data(Qt::UserRole+4).toString();
    QString createTime = item->data(Qt::UserRole).toString();
    QByteArray previewData = item->data(Qt::UserRole+1).toByteArray();

    m_nameLabel->setText("姓名：" + (name.isEmpty() ? "未知" : name));
    m_idLabel->setText("ID：" + id);
    m_identityLabel->setText("身份：" + (identity.isEmpty() ? "未知" : identity));
    m_timeLabel->setText("注册时间：" + (createTime.isEmpty() ? "未知" : createTime));
    
    qDebug() << "选中人脸：" << name << "ID：" << id << "注册时间：" << createTime;

    if (previewData.isEmpty()) {
        m_previewLabel->setText("预览图缺失");
        return;
    }

    QPixmap pixmap;
    bool loadOk = pixmap.loadFromData(previewData);
    if (loadOk) {
        m_previewLabel->setPixmap(pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_previewLabel->setText("预览图加载失败");
    }
}
