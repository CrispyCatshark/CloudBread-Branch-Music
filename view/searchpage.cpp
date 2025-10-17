#include "searchpage.h"
#include "ElaPushButton.h"
#include "ElaText.h"
#include <qboxlayout.h>
#include <ElaLineEdit.h>
#include <qheaderview.h>
#include <QMouseEvent>
#include <QDebug>
#include <QAction>
#include <QDateTime>
// 需确保包含 ElaMessageBar 头文件
#include "ElaMessageBar.h"
#include "lyricssearchpreviewpage.h"

// 播放列表单例快捷引用
#define PLAYLIST_INSTANCE Tm_musicPlayListModel::getInstance()

// -----------------------------------------------------------------------------
// searchPage 类实现
// -----------------------------------------------------------------------------
searchPage::searchPage(int type, QWidget *parent)
    : ElaWidget(parent),
    m_type(type),
    m_contextMenu(nullptr),
    m_musicModel(nullptr),
    m_playListModel(PLAYLIST_INSTANCE)
{
    intPage();
}

searchPage::~searchPage()
{
    // 释放动态资源
    delete m_contextMenu;
    delete m_musicModel;
    // delete m_lyricModel;
}

void searchPage::setMusicID(QString musicId)
{
    m_music_id = musicId;

    SqliteManager& m_db = SqliteManager::getInstance();
    // SQL查询：补充 m.file_path 字段
    QString sql = "SELECT id, sort_id, img_name, name, auther, "
                  "duration, timestamp, pitch, count, file_path "
                  "FROM music_data WHERE id = ?";
    QVector<QVariant> params = {m_music_id};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    QString Keyword = "";
    if (!result.isEmpty() && m_db.getLastError().isEmpty()) {
        const auto &row = result.first();
        Keyword = row["name"].toString() + ((row["auther"].toString() == "未知创作者" || row["auther"].toString().isEmpty())?"":"-" + row["auther"].toString());
    } else {
        qDebug() << m_db.getLastError();
    }
    qDebug() << Keyword << " | "<<m_music_id;
    ElaMessageBar::information(ElaMessageBarType::Top, "歌词搜索", "正在搜索", 2000, this);
    m_searchInput->setText(Keyword);
    m_lyricModel->searchSongsByKeyword(Keyword);
}

void searchPage::intPage()
{
    setVisible(false);
    // 1. 基础窗口配置（差异化标题）
    resize(800, 500);
    moveToCenter();
    QString titleText = (m_type == 1) ? "搜索歌词" : "搜索点歌";
    setWindowTitle(titleText);
    setWindowIcon(QIcon(":/cloudbread/src/favicon_round.png"));

    // 2. 主布局（统一结构）
    QVBoxLayout* contentLayout = new QVBoxLayout(this);
    contentLayout->setContentsMargins(15, 15, 15, 15);  // 增加内边距，优化视觉
    setLayout(contentLayout);

    // 3. 搜索区域（差异化占位符）
    m_searchArea = new ElaScrollPageArea(this);
    m_searchArea->setFixedHeight(95);
    QVBoxLayout* searchAreaLayout = new QVBoxLayout(m_searchArea);
    m_searchArea->setContentsMargins(10, 10, 10, 10);
    searchAreaLayout->setContentsMargins(0, 0, 0, 0);
    searchAreaLayout->setSpacing(10);

    // 3.1 搜索标题
    ElaText* searchTitle = new ElaText(titleText);
    searchTitle->setTextPointSize(16);
    searchAreaLayout->addWidget(searchTitle);

    // 3.2 搜索输入栏
    QHBoxLayout* searchInputAreaLayout = new QHBoxLayout();
    searchInputAreaLayout->setSpacing(10);  // 输入框与按钮间距

    m_searchInput = new ElaLineEdit();
    m_searchInput->setFixedHeight(30);
    // 差异化占位符
    m_searchInput->setPlaceholderText(m_type == 1
                                        ? "请输入歌词内容、歌曲名或歌手..."
                                        : "请输入歌名或作家进行搜索...");

    ElaPushButton* searchButton = new ElaPushButton();
    searchButton->setText("点击搜索");
    searchButton->setFixedSize(100, 34);  // 固定按钮大小，优化布局

    searchInputAreaLayout->addWidget(m_searchInput);
    searchInputAreaLayout->addWidget(searchButton);
    searchAreaLayout->addLayout(searchInputAreaLayout);
    contentLayout->addWidget(m_searchArea);

    // 4. 表格视图（统一基础配置）
    m_tableView = new ElaTableView(this);
    QFont tableHeaderFont = m_tableView->horizontalHeader()->font();
    tableHeaderFont.setPixelSize(16);
    m_tableView->horizontalHeader()->setFont(tableHeaderFont);
    m_tableView->setAlternatingRowColors(true);  // 交替行颜色，优化可读性
    m_tableView->verticalHeader()->setHidden(true);  // 隐藏行号
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);  // 整行选择
    m_tableView->horizontalHeader()->setMinimumSectionSize(60);  // 列最小宽度
    m_tableView->verticalHeader()->setMinimumSectionSize(46);    // 行最小高度
    m_tableView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_tableView->horizontalHeader()->setStretchLastSection(true);  // 最后一列自适应
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    contentLayout->addWidget(m_tableView);

    // 5. 初始化差异化模式（歌曲/歌词）
    if (m_type == 0) {
        initMusicSearchMode();
    } else {
        initLyricSearchMode();
    }

    // 6. 创建上下文菜单（根据类型区分菜单项）
    createContextMenu();

    // 7. 搜索逻辑（差异化调用模型接口）
    auto doSearch = [=]() {
        QString keyword = m_searchInput->text().trimmed();
        if (keyword.isEmpty()) {
            // 替换：搜索关键词为空警告
            ElaMessageBar::warning(
                ElaMessageBarType::Top,          // 消息栏位置（顶部）
                "搜索警告",                  // 消息标题
                "关键词不能为空，请输入搜索内容！",  // 消息内容
                3000,                        // 显示时长（3秒）
                this                         // 目标显示组件（当前页面）
                );
            return;
        }

        if (m_type == 0) {
            m_musicModel->search(keyword);  // 歌曲搜索：调用歌曲模型
        } else if (m_type == 1) {
            ElaMessageBar::information(ElaMessageBarType::Top, "歌词搜索", "正在搜索", 2000, this);
            m_lyricModel->searchSongsByKeyword(keyword);  // 歌词搜索：调用歌词模型
        }
    };
    // 绑定搜索触发（按钮点击+回车键）
    connect(searchButton, &ElaPushButton::clicked, this, doSearch);
    connect(m_searchInput, &ElaLineEdit::returnPressed, this, doSearch);

    // 8. 菜单触发信号（右键+单击）
    connect(m_tableView, &ElaTableView::customContextMenuRequested, this, &searchPage::showContextMenu);
    connect(m_tableView, &ElaTableView::clicked, this, &searchPage::showContextMenuForIndex);

    // 9. 透传播放列表的播放信号（供外部页面响应）
    connect(m_playListModel, &Tm_musicPlayListModel::playMusic,
            this, &searchPage::playMusic);
}

// 初始化歌曲搜索模式（m_type=0）
void searchPage::initMusicSearchMode()
{
    // 初始化歌曲模型
    m_musicModel = new MusicSearchTableModel(this);
    m_tableView->setModel(m_musicModel);

    // 歌曲表格列宽配置（固定列宽，优化布局）
    connect(m_tableView, &ElaTableView::tableViewShow, this, [=]() {
        m_tableView->setColumnWidth(0, 50);     // 序号列
        m_tableView->setColumnWidth(1, 250);    // 歌曲名列
        m_tableView->setColumnWidth(2, 200);    // 歌手列
        m_tableView->setColumnWidth(3, 80);     // 歌单列
        m_tableView->setColumnWidth(4, 80);     // 时长列
        m_tableView->setColumnWidth(5, 100);    // 播放次数列
    });

    // 加载播放列表（确保操作前列表已初始化）
    if (!m_playListModel->loadPlayList()) {
        // 替换：播放列表加载失败错误
        ElaMessageBar::error(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放列表错误",              // 消息标题
            QString("加载播放列表失败：%1").arg(m_playListModel->getLastError()),  // 错误详情
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
    }
}

// 初始化歌词搜索模式（m_type=1）
void searchPage::initLyricSearchMode()
{
    // 初始化歌词模型（预留实现，业务层需补充数据库查询逻辑）
    m_tableView->setModel(m_lyricModel);

    // 歌词表格列宽配置（根据列定义调整）
    connect(m_tableView, &ElaTableView::tableViewShow, this, [=]() {
        m_tableView->setColumnWidth(0, 250);     // 序号列
        m_tableView->setColumnWidth(1, 250);    // 歌词名/歌曲名列
        m_tableView->setColumnWidth(2, 150);    // 歌手列
        m_tableView->setColumnWidth(3, 100);    // 专辑列
    });

    // 歌词操作信号绑定（预留外部响应逻辑）
    connect(this, &searchPage::previewLyric, this, [=](const QString& lyricId) {
        // 替换：歌词预览信息提示
        ElaMessageBar::information(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "歌词操作",                  // 消息标题
            QString("触发歌词预览，歌词ID：%1").arg(lyricId),  // 操作详情
            2000,                        // 显示时长（2秒）
            this                         // 目标显示组件（当前页面）
            );
        // 业务层可在此处打开歌词预览窗口
    });
    connect(this, &searchPage::setLyricConfig, this, [=](const QString& lyricId) {
        // 替换：歌词设置信息提示
        ElaMessageBar::information(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "歌词操作",                  // 消息标题
            QString("触发歌词设置，歌词ID：%1").arg(lyricId),  // 操作详情
            2000,                        // 显示时长（2秒）
            this                         // 目标显示组件（当前页面）
            );
        // 业务层可在此处打开歌词设置窗口（如字体、颜色等）
    });

    lyricWindow = new lyricsSearchPreviewPage();

    connect(this, &searchPage::fetchLyricByIndex, m_lyricModel, &LyricSearchTableModel::fetchLyricByIndex);
    connect(m_lyricModel, &LyricSearchTableModel::lyricFetchFailed, this, [this](const QString& errorMsg) {
        ElaMessageBar::error(ElaMessageBarType::Top, "歌词搜索", "搜索失败 " + errorMsg + " 请稍后重试", 2000, this);
    });
    connect(m_lyricModel, &LyricSearchTableModel::lyricFetched, this, [this](const LyricData& lyricData) {
        ElaMessageBar::success(ElaMessageBarType::Top, "歌词搜索", "搜索成功", 2000, this);
        lyricWindow->setLayrics(lyricData.yrc);
        lyricWindow->setVisible(true);
        lyricWindow->activateWindow();
    });

    connect(m_lyricModel, &LyricSearchTableModel::lyricSetSuccess, this, [this]() {
        ElaMessageBar::success(ElaMessageBarType::Top, "提示", "修改歌词成功", 3000, this);
    });
    connect(m_lyricModel, &LyricSearchTableModel::lyricSetError, this, [this](const QString& msg) {
        ElaMessageBar::error(ElaMessageBarType::Top, "提示", "修改歌词失败，" + msg + "请稍后再试", 3000, this);
    });
}

// 创建上下文菜单（差异化菜单项）
void searchPage::createContextMenu()
{
    // 先释放旧菜单（防止内存泄漏）
    if (m_contextMenu) {
        m_contextMenu->clear();
        delete m_contextMenu;
    }
    m_contextMenu = new ElaMenu(this);

    // 歌曲搜索菜单（m_type=0）
    if (m_type == 0) {
        QAction* playNowAction = new QAction("立即播放", this);
        QAction* playNextAction = new QAction("下一首播放", this);
        QAction* addToPlaylistAction = new QAction("添加到播放列表", this);

        m_contextMenu->addAction(playNowAction);
        m_contextMenu->addAction(playNextAction);
        m_contextMenu->addAction(addToPlaylistAction);

        // 绑定歌曲菜单槽函数
        connect(playNowAction, &QAction::triggered, this, &searchPage::playImmediately);
        connect(playNextAction, &QAction::triggered, this, &searchPage::playNext);
        connect(addToPlaylistAction, &QAction::triggered, this, &searchPage::addToPlaylist);
    }
    // 歌词搜索菜单（m_type=1）
    else {
        QAction* previewLyricAction = new QAction("预览歌词", this);
        QAction* setLyricAction = new QAction("设置歌词", this);

        m_contextMenu->addAction(previewLyricAction);
        m_contextMenu->addAction(setLyricAction);

        // 绑定歌词菜单槽函数
        connect(previewLyricAction, &QAction::triggered, this, &searchPage::previewLyricSlot);
        connect(setLyricAction, &QAction::triggered, this, &searchPage::setLyricSlot);
    }
}

// 右键显示菜单
void searchPage::showContextMenu(const QPoint& pos)
{
    QModelIndex selectedIndex = m_tableView->indexAt(pos);
    if (!selectedIndex.isValid()) return;

    m_currentIndex = selectedIndex;
    // 在鼠标位置显示菜单
    m_contextMenu->exec(m_tableView->viewport()->mapToGlobal(pos));
}

// 单击显示菜单（优化用户体验）
void searchPage::showContextMenuForIndex(const QModelIndex& index)
{
    if (index.isValid()) {
        m_currentIndex = index;
        // 在单元格右上角显示菜单（避免遮挡内容）
        QRect rect = m_tableView->visualRect(index);
        QPoint menuPos = rect.topRight() + QPoint(5, 0);  // 偏移5px，避免紧贴
        m_contextMenu->exec(m_tableView->viewport()->mapToGlobal(menuPos));
    }
}

// -----------------------------------------------------------------------------
// 歌曲搜索菜单槽函数（与播放列表交互逻辑）
// -----------------------------------------------------------------------------
void searchPage::playImmediately()
{
    if (!m_currentIndex.isValid() || !m_musicModel || !m_playListModel) {
        // 替换：播放请求无效警告
        ElaMessageBar::warning(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            "播放请求无效，索引或模型未初始化！",  // 警告内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
        return;
    }

    QString musicId = m_musicModel->getMusicId(m_currentIndex.row());
    if (musicId.isEmpty()) {
        // 替换：歌曲ID为空警告
        ElaMessageBar::warning(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            "歌曲ID为空，无法执行播放操作！",  // 警告内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
        return;
    }

    if (!m_playListModel->addMusicToCurrentPos(musicId)) {
        // 替换：添加歌曲失败错误
        ElaMessageBar::error(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            QString("添加歌曲失败：%1").arg(m_playListModel->getLastError()),  // 错误详情
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
    } else {
        // 替换：点歌成功提示
        ElaMessageBar::success(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放提示",                  // 消息标题
            "点歌成功，正在播放当前歌曲！",  // 成功内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
        m_playListModel->markAsPlayed();
    }
}

void searchPage::playNext()
{
    if (!m_currentIndex.isValid() || !m_musicModel || !m_playListModel) {
        // 替换：下一首请求无效警告
        ElaMessageBar::warning(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            "下一首播放请求无效，索引或模型未初始化！",  // 警告内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
        return;
    }

    QString musicId = m_musicModel->getMusicId(m_currentIndex.row());
    if (musicId.isEmpty()) {
        // 替换：歌曲ID为空警告
        ElaMessageBar::warning(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            "歌曲ID为空，无法添加到下一首！",  // 警告内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
        return;
    }

    // 调用播放列表接口：添加到当前位置后（即下一首）
    if (!m_playListModel->addMusicToCurrentPos(musicId)) {
        // 替换：添加下一首失败错误
        ElaMessageBar::error(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            QString("添加到下一首失败：%1").arg(m_playListModel->getLastError()),  // 错误详情
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
    } else {
        // 替换：添加下一首成功提示
        ElaMessageBar::success(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放提示",                  // 消息标题
            "已添加到下一首播放队列！",  // 成功内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
    }
}

void searchPage::addToPlaylist()
{
    if (!m_currentIndex.isValid() || !m_musicModel || !m_playListModel) {
        // 替换：添加播放列表请求无效警告
        ElaMessageBar::warning(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            "添加到播放列表请求无效，索引或模型未初始化！",  // 警告内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
        return;
    }

    QString musicId = m_musicModel->getMusicId(m_currentIndex.row());
    if (musicId.isEmpty()) {
        // 替换：歌曲ID为空警告
        ElaMessageBar::warning(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            "歌曲ID为空，无法添加到播放列表！",  // 警告内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
        return;
    }

    // 调用播放列表接口：添加到列表末尾
    if (!m_playListModel->addMusicToEnd(musicId)) {
        // 替换：添加到列表末尾失败错误
        ElaMessageBar::error(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放警告",                  // 消息标题
            QString("添加到播放列表末尾失败：%1").arg(m_playListModel->getLastError()),  // 错误详情
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
    } else {
        // 替换：添加到列表末尾成功提示
        ElaMessageBar::success(
            ElaMessageBarType::Top,          // 消息栏位置（顶部）
            "播放提示",                  // 消息标题
            "已添加到播放列表末尾！",  // 成功内容
            3000,                        // 显示时长（3秒）
            this                         // 目标显示组件（当前页面）
            );
    }
}

// -----------------------------------------------------------------------------
// 歌词搜索菜单槽函数（预留实现，与歌词模型交互）
// -----------------------------------------------------------------------------
void searchPage::previewLyricSlot()
{
    QModelIndexList selectedRows = m_tableView->selectionModel()->selectedRows();
    if (selectedRows.size() != 1) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请选中一首歌曲进行修改！", 3000, this);
        return;
    }
    QModelIndex selectedIndex = selectedRows.first();
    if (!selectedIndex.isValid()) return;

    emit fetchLyricByIndex(selectedIndex);
}

void searchPage::setLyricSlot()
{
    QModelIndexList selectedRows = m_tableView->selectionModel()->selectedRows();
    if (selectedRows.size() != 1) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请选中一首歌曲进行修改！", 3000, this);
        return;
    }
    QModelIndex selectedIndex = selectedRows.first();
    if (!selectedIndex.isValid()) return;

    m_lyricModel->saveLyricToMusicLrcTable(m_music_id, selectedIndex.row());
}
