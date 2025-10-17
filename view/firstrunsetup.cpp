#include "firstrunsetup.h"
#include "mainwindow.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QStandardPaths>
#include "GlobalConfig.h"

FirstRunSetup::FirstRunSetup(QWidget *parent)
    : QWidget(parent)
{
    // 设置窗口标题和大小
    setWindowTitle(tr("首次启动设置"));
    setMinimumSize(600, 400);

    // 构建UI
    setupUI();
}

FirstRunSetup::~FirstRunSetup()
{
}

void FirstRunSetup::setupUI()
{
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(15);  // 优化间距，避免界面拥挤

    // 标题
    m_titleLabel = new QLabel(tr("设置数据存储目录"));
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    // 描述
    m_descriptionLabel = new QLabel(tr("请选择应用程序的数据存储目录。该目录将用于保存音频文件、封面图片、配置和数据库。"));
    m_descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(m_descriptionLabel);

    // 新增：存储空间提示区域
    QWidget *storageTipWidget = new QWidget();
    QVBoxLayout *storageTipLayout = new QVBoxLayout(storageTipWidget);
    storageTipLayout->setSpacing(8);

    // 提示标题（带图标模拟）
    QLabel *storageTipTitle = new QLabel(tr("💾 存储空间参考（基于平均音频质量）"));
    QFont tipTitleFont = storageTipTitle->font();
    tipTitleFont.setBold(true);
    tipTitleFont.setPointSize(10);
    storageTipTitle->setFont(tipTitleFont);
    storageTipLayout->addWidget(storageTipTitle);

    // 具体数据提示
    QLabel *storageData1 = new QLabel(tr("• 100首音乐：约 3.28GB 空间"));
    QLabel *storageData2 = new QLabel(tr("• 500首音乐：约 16.5GB 空间"));
    QLabel *storageData3 = new QLabel(tr("• 1000首音乐：约 32.85GB 空间"));
    storageTipLayout->addWidget(storageData1);
    storageTipLayout->addWidget(storageData2);
    storageTipLayout->addWidget(storageData3);

    // 将提示区域加入主布局
    mainLayout->addWidget(storageTipWidget);

    // 路径选择区域
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->setSpacing(10);

    m_pathLabel = new QLabel(tr("数据目录:"));
    pathLayout->addWidget(m_pathLabel);

    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText(tr("请选择或输入目录路径"));
    m_pathEdit->setText(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    pathLayout->addWidget(m_pathEdit, 1);

    m_browseButton = new QPushButton(tr("浏览..."));
    connect(m_browseButton, &QPushButton::clicked, this, &FirstRunSetup::onBrowseClicked);
    pathLayout->addWidget(m_browseButton);

    mainLayout->addLayout(pathLayout);

    // 继续按钮
    m_continueButton = new QPushButton(tr("继续"));
    // m_continueButton->setEnabled(false);  // 初始禁用，选择路径后启用
    connect(m_continueButton, &QPushButton::clicked, this, &FirstRunSetup::onContinueClicked);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_continueButton);
    mainLayout->addLayout(buttonLayout);

    // 现有数据提示区域（初始隐藏）
    m_existingDataWidget = new QWidget();
    QVBoxLayout *existingDataLayout = new QVBoxLayout(m_existingDataWidget);
    existingDataLayout->setSpacing(15);

    m_existingDataLabel = new QLabel(tr("所选目录中已存在应用数据。请选择以下操作:"));
    m_existingDataLabel->setWordWrap(true);
    existingDataLayout->addWidget(m_existingDataLabel);

    QHBoxLayout *existingButtonsLayout = new QHBoxLayout();
    existingButtonsLayout->setSpacing(10);

    m_useExistingButton = new QPushButton(tr("使用现有数据"));
    connect(m_useExistingButton, &QPushButton::clicked, this, &FirstRunSetup::onUseExistingClicked);
    existingButtonsLayout->addWidget(m_useExistingButton);

    m_clearExistingButton = new QPushButton(tr("清空并重新创建"));
    connect(m_clearExistingButton, &QPushButton::clicked, this, &FirstRunSetup::onClearExistingClicked);
    existingButtonsLayout->addWidget(m_clearExistingButton);

    existingDataLayout->addLayout(existingButtonsLayout);
    mainLayout->addWidget(m_existingDataWidget);
    m_existingDataWidget->setVisible(false);  // 初始隐藏

    // 添加拉伸项，将所有内容推到顶部
    mainLayout->addStretch();

    // 设置主布局
    setLayout(mainLayout);
}

void FirstRunSetup::onBrowseClicked()
{
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择数据存储目录"),
        defaultPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    if (!dir.isEmpty()) {
        m_pathEdit->setText(dir);
        m_selectedPath = dir;
        m_continueButton->setEnabled(true);
    }
}

void FirstRunSetup::onContinueClicked()
{
    QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), tr("请选择一个有效的目录"));
        return;
    }

    m_selectedPath = path;

    // 检查目录是否存在，不存在则创建
    QDir dir(path);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::critical(this, tr("创建目录失败"), tr("无法创建所选目录，请选择其他位置"));
            return;
        }
    }

    // 检查目录结构
    if (checkDirectoryStructure(path)) {
        // 显示现有数据提示
        m_existingDataWidget->setVisible(true);
        m_continueButton->setEnabled(false);
        m_pathEdit->setEnabled(false);
        m_browseButton->setEnabled(false);
    } else {
        // 创建所需的目录和文件
        if (createDirectoryStructure(path)) {
            // 保存设置并进入主窗口
            m_settings.setValue("App/DBpath", path);
            showMainWindow();
        } else {
            QMessageBox::critical(this, tr("设置失败"), tr("无法创建必要的目录结构"));
        }
    }
}

void FirstRunSetup::onUseExistingClicked()
{
    // 保存设置并进入主窗口
    m_settings.setValue("App/DBpath", m_selectedPath);
    showMainWindow();
}

void FirstRunSetup::onClearExistingClicked()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::warning(
        this,
        tr("确认清空"),
        tr("确定要清空除所选目录中的所有应用数据吗？此操作不可恢复。"),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        if (clearDirectoryContents(m_selectedPath)) {
            if (createDirectoryStructure(m_selectedPath)) {
                m_settings.setValue("App/DBpath", m_selectedPath);
                showMainWindow();
            }
        } else {
            QMessageBox::critical(this, tr("操作失败"), tr("无法清空目录内容"));
        }
    }
}

bool FirstRunSetup::checkDirectoryStructure(const QString &path)
{
    // 检查是否存在所需的目录和文件
    QDir dir(path);

    bool hasAudioCovers = dir.exists("AudioCovers");
    bool hasAudioFiles = dir.exists("AudioFiles");
    bool hasDatabase = QFile::exists(dir.filePath("database.db"));

    // 如果所有必要的元素都存在，则返回true
    return hasAudioCovers && hasAudioFiles && hasDatabase;
}

bool FirstRunSetup::createDirectoryStructure(const QString &path)
{
    QDir dir(path);

    // 创建目录
    if (!dir.mkdir("AudioCovers")) return false;
    if (!dir.mkdir("AudioFiles")) return false;

    return true;
}

bool FirstRunSetup::clearDirectoryContents(const QString &path)
{
    QDir dir(path);

    // 删除AudioCovers目录及其内容
    if (dir.exists("AudioCovers") && !dir.removeRecursively()) {
        return false;
    }

    // 删除AudioFiles目录及其内容
    if (dir.exists("AudioFiles") && !dir.removeRecursively()) {
        return false;
    }

    // 删除数据库文件
    QFile::remove(dir.filePath("database.db"));

    return true;
}

void FirstRunSetup::showMainWindow()
{
    initDatabaseTables(m_selectedPath + "/database.db");
    MainWindow *mainWindow = new MainWindow();
    mainWindow->show();
    this->close();
}

// 初始化数据库表结构（包含music_lrc表）
bool FirstRunSetup::initDatabaseTables(const QString& dbPath)
{
    qDebug() << "数据库文件：" << dbPath;
    // 获取SqliteManager单例并初始化数据库
    SqliteManager& dbManager = SqliteManager::getInstance();
    if (!dbManager.init(dbPath)) {
        qWarning() << "数据库初始化失败:" << dbManager.getLastError();
        return false;
    }

    // 开始事务，确保表结构创建的原子性
    if (!dbManager.beginTransaction()) {
        qWarning() << "开启事务失败:" << dbManager.getLastError();
        return false;
    }

    // 创建group_data表
    if (!dbManager.tableExists("group_data")) {
        QString createSql = "CREATE TABLE group_data ("
                            "id CHAR(64) PRIMARY KEY,"
                            "name CHAR(64) NOT NULL,"
                            "img CHAR(128)"
                            ");";
        if (!dbManager.executeSql(createSql)) {
            qWarning() << "创建group_data表失败:" << dbManager.getLastError();
            dbManager.rollbackTransaction();
            return false;
        }
        QString incertSql = "INSERT INTO group_data VALUES("
                            "'cd8551c8e0078d59d4c6eef21fcb2731',"
                            "'默认歌单',"
                            "':/cloudbread/src/favicon_round.png'"
                            ");";
        if (!dbManager.executeSql(incertSql)) {
            qWarning() << "插入默认歌单失败:" << dbManager.getLastError();
            dbManager.rollbackTransaction();
            return false;
        }
        qDebug() << "group_data表创建成功";
    } else {
        qDebug() << "group_data表已存在";
    }

    // 创建music_group表
    if (!dbManager.tableExists("music_group")) {
        QString createSql = "CREATE TABLE music_group ("
                            "music_id CHAR(64) NOT NULL,"
                            "group_id CHAR(64) NOT NULL,"
                            "PRIMARY KEY (music_id, group_id),"
                            "FOREIGN KEY (music_id) REFERENCES music_data(id),"
                            "FOREIGN KEY (group_id) REFERENCES group_data(id)"
                            ");";
        if (!dbManager.executeSql(createSql)) {
            qWarning() << "创建music_group表失败:" << dbManager.getLastError();
            dbManager.rollbackTransaction();
            return false;
        }
        qDebug() << "music_group表创建成功";
    } else {
        qDebug() << "music_group表已存在";
    }

    // 创建music_data表
    if (!dbManager.tableExists("music_data")) {
        QString createSql = "CREATE TABLE music_data ("
                            "id CHAR(64) PRIMARY KEY,"
                            "sort_id INT NOT NULL,"
                            "img_name CHAR(128),"
                            "name CHAR(128) NOT NULL,"
                            "auther CHAR(128),"
                            "duration INT,"
                            "timestamp INT,"
                            "pitch INT,"
                            "count INT,"
                            "file_path CHAR(256)"
                            ");";
        if (!dbManager.executeSql(createSql)) {
            qWarning() << "创建music_data表失败:" << dbManager.getLastError();
            dbManager.rollbackTransaction();
            return false;
        }
        qDebug() << "music_data表创建成功";
    } else {
        qDebug() << "music_data表已存在";
    }

    // 创建music_lrc表（新增）
    if (!dbManager.tableExists("music_lrc")) {
        QString createSql = "CREATE TABLE music_lrc ("
                            "id CHAR(64) PRIMARY KEY,"
                            "trans TEXT,"
                            "yrc TEXT,"
                            "roma TEXT,"
                            "FOREIGN KEY (id) REFERENCES music_data(id) ON DELETE CASCADE"
                            ");";
        if (!dbManager.executeSql(createSql)) {
            qWarning() << "创建music_lrc表失败:" << dbManager.getLastError();
            dbManager.rollbackTransaction();
            return false;
        }
        qDebug() << "music_lrc表创建成功";
    } else {
        qDebug() << "music_lrc表已存在";
    }

    // 创建playing_list表（新增）
    if (!dbManager.tableExists("playing_list")) {
        QString createSql = "CREATE TABLE playing_list ("
                            "id CHAR(64) PRIMARY KEY,"
                            "sort_id INT,"
                            "music_id CHAR(64),"
                            "playing BOOL,"
                            "played BOOL,"
                            "FOREIGN KEY (music_id) REFERENCES music_data(id)"
                            ");";
        if (!dbManager.executeSql(createSql)) {
            qWarning() << "创建playing_list表失败:" << dbManager.getLastError();
            dbManager.rollbackTransaction();
            return false;
        }
        qDebug() << "playing_list表创建成功";
    } else {
        qDebug() << "playing_list表已存在";
    }

    if (!dbManager.tableExists("online_music_lrc")) {
        QString createTableSql = R"(
                CREATE TABLE IF NOT EXISTS online_music_lrc (
                    mid TEXT PRIMARY KEY NOT NULL,  -- 歌曲唯一标识（与SongInfo.mid对应）
                    trans TEXT,                    -- 翻译歌词
                    yrc TEXT,                      -- 逐字歌词
                    roma TEXT                      -- 罗马音歌词
                );
            )";
        if (!dbManager.executeSql(createTableSql)) {
            qCritical() << "创建online_music_lrc表失败：" << dbManager.getLastError();
        } else {
            qDebug() << "online_music_lrc表初始化成功";
        }
    }

    // 提交事务
    if (!dbManager.commitTransaction()) {
        qWarning() << "提交事务失败:" << dbManager.getLastError();
        dbManager.rollbackTransaction();
        return false;
    }

    qDebug() << "数据库表结构初始化完成";
    return true;
}
