#include "musiclistpage.h"
#include "ElaMenu.h"
#include "tm_listviewmodel.h"
#include "tm_musiclistviewmodel.h"
#include <ElaScrollBar.h>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
#include <QUuid>
#include "ElaMessageBar.h"
#include "LyricFetcherThread.h"
#include "MusicPlayer.h"

musicListPage::musicListPage(QWidget* parent)
    : ElaScrollPage(parent) {
    this->setTitleVisible(false);

    m_importThread = new QThread(this);
    m_musicImporter = new MusicImporter();
    m_musicImporter->moveToThread(m_importThread);
    m_importThread->start();

    initPage();

    connect(this, &musicListPage::startImport,
            m_musicImporter, &MusicImporter::importMusic,
            Qt::QueuedConnection);
    connect(m_musicImporter, &MusicImporter::importProgress,
            this, &musicListPage::onImportProgress,
            Qt::QueuedConnection);
    connect(m_musicImporter, &MusicImporter::importFinished,
            this, &musicListPage::onImportFinished,
            Qt::QueuedConnection);
    connect(m_musicImporter, &MusicImporter::importError,
            this, &musicListPage::onImportError,
            Qt::QueuedConnection);
}

musicListPage::~musicListPage() {
    // 安全停止线程
    m_importThread->requestInterruption();
    m_importThread->quit();
    m_importThread->wait(1000);  // 等待线程退出，最多1秒
    delete m_musicImporter;      // 释放Importer
}

void musicListPage::initPage() {
    // 初始化模型
    m_listModel = new Tm_listViewModel(this);
    m_musicModel = new Tm_musicListViewModel(this);
    _musicplayerbase = new musicplayerbase();

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout* musicListContentLayout = new QHBoxLayout();
    QVBoxLayout* musicListNameLayout = new QVBoxLayout();

    QHBoxLayout* musicListBtnLayout = new QHBoxLayout();

    m_importBtn = new ElaToolButton;
    m_importBtn->setIsTransparent(false);
    m_importBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_importBtn->setText("导入");
    m_importBtn->setElaIcon(ElaIconType::FileImport);
    m_importBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(m_importBtn, &ElaToolButton::clicked, this, &musicListPage::onSongImport);

    m_addListBtn = new ElaToolButton;
    m_addListBtn->setIsTransparent(false);
    m_addListBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_addListBtn->setText("新建");
    m_addListBtn->setElaIcon(ElaIconType::Plus);
    m_addListBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    // 绑定新建歌单按钮
    connect(m_addListBtn, &ElaToolButton::clicked, this, &musicListPage::onMusicListAddNew);

    // 歌单列表初始化
    m_listView = new ElaListView(this);
    m_listView->setAlternatingRowColors(false);
    m_listView->setModel(m_listModel);
    ElaScrollBar* listViewFloatScrollBar = new ElaScrollBar(m_listView->verticalScrollBar(), m_listView);
    listViewFloatScrollBar->setIsAnimation(true);

    // 绑定歌单列表点击事件
    connect(m_listView, &ElaListView::clicked, this, &musicListPage::onMusicListItemClicked);
    // 绑定歌单列表右键菜单
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listView, &ElaListView::customContextMenuRequested, this, &musicListPage::onMusicListContextMenu);

    musicListBtnLayout->addWidget(m_importBtn);
    musicListBtnLayout->addWidget(m_addListBtn);

    musicListNameLayout->addWidget(m_listView);
    musicListNameLayout->addLayout(musicListBtnLayout);
    musicListNameLayout->setStretch(0, 0);
    musicListNameLayout->setStretch(1, 1);

    // 歌单操作区初始化
    m_listOperationArea = new ElaScrollPageArea(this);
    QVBoxLayout* listContentLayout = new QVBoxLayout();
    QHBoxLayout* listOperationLayout = new QHBoxLayout(m_listOperationArea);
    QVBoxLayout* listContentInfoLayout = new QVBoxLayout();
    QHBoxLayout* listContentBtnLayout = new QHBoxLayout();
    m_listOperationArea->setFixedHeight(120);
    m_listOperationArea->setContentsMargins(10, 10, 0, 10);

    m_musicListCoverLabel = new ElaText();
    m_musicListCoverLabel->setFixedSize(80, 80);
    m_musicListCoverLabel->setPixmap(getRoundRectPixmap(QPixmap(":/cloudbread/src/favicon_round.png"), m_musicListCoverLabel->size(), 8));

    m_listNameTitle = new ElaText();
    m_listNameTitle->setTextPixelSize(20);
    m_listNameTitle->setText("默认歌单");

    m_listInfo = new ElaText();
    m_listInfo->setTextPixelSize(12);
    m_listInfo->setText("当前收录 0 首");

    m_listImportBtn = new ElaToolButton();
    m_listImportBtn->setIsTransparent(false);
    m_listImportBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_listImportBtn->setText("导入");
    m_listImportBtn->setFixedHeight(35);
    m_listImportBtn->setElaIcon(ElaIconType::FileImport);
    connect(m_listImportBtn, &ElaToolButton::clicked, this, &musicListPage::onSongImport);

    m_listPlayBtn = new ElaToolButton();
    m_listPlayBtn->setIsTransparent(false);
    m_listPlayBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_listPlayBtn->setText("播放全部");
    m_listPlayBtn->setFixedHeight(35);
    m_listPlayBtn->setElaIcon(ElaIconType::Play);
    connect(m_listPlayBtn, &ElaToolButton::clicked, this, &musicListPage::onMusicListPlay);

    m_disruptSortBtn = new ElaToolButton();
    m_disruptSortBtn->setIsTransparent(false);
    m_disruptSortBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_disruptSortBtn->setText("打乱排序");
    m_disruptSortBtn->setFixedHeight(35);
    m_disruptSortBtn->setElaIcon(ElaIconType::HandFingersCrossed);
    connect(m_disruptSortBtn, &ElaToolButton::clicked, this, &musicListPage::onDisruptSort);

    _searchEdit = new ElaLineEdit(this);
    _searchEdit->setFixedHeight(35);
    _searchEdit->setPlaceholderText("查找功能（支持歌曲名/歌手名模糊搜索）"); // 优化提示文本
    _searchEdit->setClearButtonEnabled(true); // 启用清空按钮
    _searchEdit_lightSearchAction = new QAction(ElaIcon::getInstance()->getElaIcon(ElaIconType::MagnifyingGlass), "Search", this);
    _searchEdit_darkSearchAction = new QAction(ElaIcon::getInstance()->getElaIcon(ElaIconType::MagnifyingGlass, QColor(0xFF, 0xFF, 0xFF)), "Search", this);

    _themeMode = eTheme->getThemeMode();
    connect(eTheme, &ElaTheme::themeModeChanged, this, &musicListPage::onThemeModeChanged);
    if (_themeMode == ElaThemeType::Light)
    {
        _searchEdit->addAction(_searchEdit_lightSearchAction, QLineEdit::TrailingPosition);
    }
    else
    {
        _searchEdit->addAction(_searchEdit_darkSearchAction, QLineEdit::TrailingPosition);
    }

    listContentBtnLayout->addWidget(m_listImportBtn);
    listContentBtnLayout->addWidget(m_listPlayBtn);
    listContentBtnLayout->addWidget(m_disruptSortBtn);
    listContentBtnLayout->addWidget(_searchEdit);
    listContentBtnLayout->addStretch();

    listContentInfoLayout->addWidget(m_listNameTitle);
    listContentInfoLayout->addWidget(m_listInfo);
    listContentInfoLayout->addLayout(listContentBtnLayout);

    listOperationLayout->addWidget(m_musicListCoverLabel);
    listOperationLayout->addSpacing(10);
    listOperationLayout->addLayout(listContentInfoLayout);

    // 歌曲列表初始化
    m_musicListView = new ElaTableView(this);
    QFont tableHeaderFont = m_musicListView->horizontalHeader()->font();
    tableHeaderFont.setPixelSize(16);
    m_musicListView->horizontalHeader()->setFont(tableHeaderFont);
    m_musicListView->setModel(m_musicModel);
    m_musicListView->setAlternatingRowColors(true);
    m_musicListView->setIconSize(QSize(38, 38));
    m_musicListView->verticalHeader()->setHidden(true);
    m_musicListView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_musicListView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_musicListView->horizontalHeader()->setMinimumSectionSize(60);
    m_musicListView->verticalHeader()->setMinimumSectionSize(46);
    m_musicListView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_musicListView->horizontalHeader()->setStretchLastSection(true); //最后一列铺满最后
    m_musicListView->setSortingEnabled(true);

    // 绑定歌曲列表点击事件
    connect(m_musicListView, &ElaTableView::doubleClicked, this, &musicListPage::onSongItemClicked);
    // 绑定歌曲列表右键菜单
    m_musicListView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_musicListView, &ElaTableView::customContextMenuRequested, this, &musicListPage::onSongListContextMenu);

    connect(m_musicListView, &ElaTableView::tableViewShow, this, [=]() {
        m_musicListView->setColumnWidth(0, 30);      // 序号列，设置为靠左显示
        m_musicListView->setColumnWidth(1, 300);     // 歌曲名列
        m_musicListView->setColumnWidth(2, 200);     // 歌手列
        m_musicListView->setColumnWidth(3, 100);     // 时长列
        m_musicListView->setColumnWidth(4, 120);     // 播放次数列，加宽以完整显示
    });

    listContentLayout->addWidget(m_listOperationArea);
    listContentLayout->addWidget(m_musicListView);
    listContentLayout->setStretch(0, 1);
    listContentLayout->setStretch(1, 4);

    musicListContentLayout->addLayout(musicListNameLayout);
    musicListContentLayout->addLayout(listContentLayout);
    musicListContentLayout->setStretch(0, 1);
    musicListContentLayout->setStretch(1, 4);

    centerLayout->addLayout(musicListContentLayout);
    centerLayout->addWidget(_musicplayerbase);
    centerLayout->setStretch(0, 1);
    centerLayout->setStretch(1, 0);
    addCentralWidget(centralWidget, true, true, 0);

    // 初始化默认选中第一个歌单
    if (m_listModel->rowCount() > 0) {
        QModelIndex firstIndex = m_listModel->index(0);
        m_listView->setCurrentIndex(firstIndex);
        QString firstGroupId = firstIndex.data(Tm_listViewModel::IdRole).toString();
        onMusicListItemClicked(firstIndex);
        bool loadSuccess = m_musicModel->loadMusicByGroupId(firstGroupId);
        if (!loadSuccess) {
            ElaMessageBar::error(ElaMessageBarType::Top, "加载失败", "未能加载当前歌单的歌曲，请重试！", 3000, this);
        }
    }
    m_playListModel = Tm_musicPlayListModel::getInstance(this);
    m_playListModel->loadPlayList(); // 加载初始播放列表

    connect(LyricFetcherThread::getInstance(), &LyricFetcherThread::imgUpdated, m_musicModel, &Tm_musicListViewModel::updateMusicCover);

    m_searchPage = new searchPage(1);
    m_searchPage->hide();

    MusicPlayer::getInstance()->initCurrentPlayingMusic();

    // connect(_searchEdit_lightSearchAction, &QAction::triggered, this, &musicListPage::onSearchTextChanged);
    // connect(_searchEdit_darkSearchAction, &QAction::triggered, this, &musicListPage::onSearchTextChanged);
    connect(_searchEdit, &QLineEdit::textChanged, this, &musicListPage::onSearchTextChanged);
}

/**
 * @brief 搜索输入框文本变化时触发模糊搜索
 * @param keyword 输入的搜索关键词
 */
void musicListPage::onSearchTextChanged(const QString &keyword)
{
    // 1. 校验当前是否有选中歌单（无歌单则不搜索）
    QString currentGroupId = m_musicModel->getCurrentGroupId();
    if (currentGroupId.isEmpty()) {
        if (!keyword.isEmpty()) { // 有输入但无歌单时提示
            ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "请先选中一个歌单再搜索！", 3000, this);
            _searchEdit->clear(); // 清空输入
        }
        return;
    }

    // 2. 关键词为空时，恢复显示完整歌单
    if (keyword.trimmed().isEmpty()) {
        // 重新加载当前歌单的所有歌曲
        bool loadSuccess = m_musicModel->loadMusicByGroupId(currentGroupId);
        if (loadSuccess) {
            // 更新歌单信息显示
            int totalCount = m_musicModel->rowCount();
            m_listInfo->setText(QString("当前收录 %1 首").arg(totalCount));
        } else {
            ElaMessageBar::error(ElaMessageBarType::Top, "加载失败", "未能恢复歌单显示，请重试！", 3000, this);
        }
        return;
    }

    // 3. 非空关键词，执行模糊搜索
    bool searchSuccess = m_musicModel->searchMusic(currentGroupId, keyword);
    if (!searchSuccess) {
        ElaMessageBar::error(ElaMessageBarType::Top, "搜索失败", "未能获取搜索结果，请重试！", 3000, this);
        return;
    }

    // 4. 显示搜索结果统计
    // int resultCount = m_musicModel->rowCount();
    // ElaMessageBar::information(ElaMessageBarType::Top, "搜索结果",
    //                            QString("找到包含「%1」的歌曲 %2 首").arg(keyword).arg(resultCount),
    //                            2000, this);
}


void musicListPage::onThemeModeChanged(ElaThemeType::ThemeMode themeMode)
{
    _themeMode = themeMode;
    _searchEdit->removeAction(_themeMode == ElaThemeType::Light ? _searchEdit_darkSearchAction : _searchEdit_lightSearchAction);
    _searchEdit->addAction(_themeMode == ElaThemeType::Light ? _searchEdit_lightSearchAction : _searchEdit_darkSearchAction, QLineEdit::TrailingPosition);
    _searchEdit->update();
}

void musicListPage::onDisruptSort()
{
    if (m_musicModel->shuffleMusicList()) {
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "歌单已打乱", 3000, this);
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "错误", "打乱歌单失败！", 3000, this);
    }
}

QPixmap musicListPage::getRoundRectPixmap(QPixmap srcPixMap, const QSize& size, int radius) {
    if (srcPixMap.isNull()) {
        return srcPixMap;
    }

    int imageWidth = size.width();
    int imageHeight = size.height();

    QPixmap newPixMap = srcPixMap.scaled(imageWidth, (imageHeight == 0 ? imageWidth : imageHeight), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap destImage(imageWidth, imageHeight);
    destImage.fill(Qt::transparent);
    QPainter painter(&destImage);
    painter.setRenderHints(QPainter::Antialiasing, true);
    painter.setRenderHints(QPainter::SmoothPixmapTransform, true);
    QPainterPath path;
    QRect rect(0, 0, imageWidth, imageHeight);
    path.addRoundedRect(rect, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, imageWidth, imageHeight, newPixMap);
    return destImage;
}

void musicListPage::updateSelectedGroupInfo(const QString& groupId) {
    GroupItem group = m_listModel->getGroupById(groupId);
    if (group.id.isEmpty()) return;

    // 检查歌单封面是否为空或为默认封面
    bool needUpdateCover = group.img.isEmpty() || group.img == ":/cloudbread/src/favicon_round.png";
    if (needUpdateCover) {
        // 获取该歌单下随机一首歌曲的封面
        QString randomMusicCover = m_listModel->getRandomMusicCoverByGroupId(groupId);
        // 若获取到有效封面，则更新歌单封面
        if (!randomMusicCover.isEmpty()) {
            group.img = randomMusicCover;
            // 更新数据库与模型
            bool updateSuccess = m_listModel->updateGroup(group);
            if (updateSuccess) {
                qDebug() << QString("歌单「%1」封面已更新为随机歌曲封面").arg(group.name);
            } else {
                qWarning() << QString("更新歌单「%1」封面失败").arg(group.name);
            }
        }
    }

    m_listNameTitle->setText(group.name);
    int musicCount = m_musicModel->rowCount();
    m_listInfo->setText(QString("当前收录 %1 首").arg(musicCount));

    QPixmap coverPix;
    if (!group.img.isEmpty() && coverPix.load(group.img)) {
        m_musicListCoverLabel->setPixmap(getRoundRectPixmap(coverPix, m_musicListCoverLabel->size(), 8));
    } else {
        coverPix.load(":/cloudbread/src/favicon_round.png");
        m_musicListCoverLabel->setPixmap(getRoundRectPixmap(coverPix, m_musicListCoverLabel->size(), 8));
    }
}

QString musicListPage::generateMusicId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

int musicListPage::getAudioDuration(const QString& filePath) const {
    Q_UNUSED(filePath);
    return 180;
}

void musicListPage::onMusicListContextMenu(const QPoint& pos) {
    QModelIndex selectedIndex = m_listView->indexAt(pos);
    bool hasSelectedItem = selectedIndex.isValid();

    m_clickedSongColumn = selectedIndex.isValid() ? selectedIndex.column() : -1;
    qDebug() << m_clickedSongColumn;

    ElaMenu* menu = new ElaMenu(this);
    menu->setMenuItemHeight(30);

    if (hasSelectedItem) {
        // 新增：播放歌单选项（置顶显示）
        QAction* playAction = menu->addElaIconAction(ElaIconType::Play, "播放");
        QAction* modifyAction = menu->addElaIconAction(ElaIconType::FontCase, "修改");
        QAction* deleteAction = menu->addElaIconAction(ElaIconType::Trash, "删除");
        menu->addSeparator();

        // 连接播放歌单信号（新增）
        connect(playAction, &QAction::triggered, this, &musicListPage::onMusicListPlay);
        connect(modifyAction, &QAction::triggered, this, &musicListPage::onMusicListModify);
        connect(deleteAction, &QAction::triggered, this, &musicListPage::onMusicListDelete);
    }

    QAction* addNewAction = menu->addElaIconAction(ElaIconType::Plus, "新建");
    connect(addNewAction, &QAction::triggered, this, &musicListPage::onMusicListAddNew);

    menu->exec(m_listView->mapToGlobal(pos));
    menu->deleteLater();
}

void musicListPage::onSongListContextMenu(const QPoint& pos) {
    QModelIndex selectedIndex = m_musicListView->indexAt(pos);
    bool hasSelectedRow = selectedIndex.isValid();

    ElaMenu* menu = new ElaMenu(this);
    menu->setMenuItemHeight(30);

    if (hasSelectedRow) {
        // 新增：播放相关选项（置顶）
        QAction* playNextAction = menu->addElaIconAction(ElaIconType::Play, "下一首播放");
        QAction* addToQueueAction = menu->addElaIconAction(ElaIconType::Plus, "添加到队列");
        menu->addSeparator();

        QAction* deleteAction = menu->addElaIconAction(ElaIconType::Trash, "删除");
        QAction* modifyMusicNameAction = menu->addElaIconAction(ElaIconType::FontCase, "修改歌名");
        QAction* modifyMusicAutherAction = menu->addElaIconAction(ElaIconType::PenField, "修改作家");
        QAction* searchMusicLyricAction = menu->addElaIconAction(ElaIconType::CloudMusic, "搜索歌词");
        menu->addSeparator();

        // 连接新增选项信号（新增）
        connect(playNextAction, &QAction::triggered, this, &musicListPage::onSongPlayNext);
        connect(addToQueueAction, &QAction::triggered, this, &musicListPage::onSongAddToQueue);
        connect(deleteAction, &QAction::triggered, this, &musicListPage::onSongDelete);
        connect(modifyMusicNameAction, &QAction::triggered, this, &musicListPage::onSongNameModify);
        connect(modifyMusicAutherAction, &QAction::triggered, this, &musicListPage::onSongAutherModify);
        connect(searchMusicLyricAction, &QAction::triggered, this, &musicListPage::onSearchMusicLyric);
    }

    QAction* importAction = menu->addElaIconAction(ElaIconType::FileImport, "导入");
    connect(importAction, &QAction::triggered, this, &musicListPage::onSongImport);

    menu->exec(m_musicListView->mapToGlobal(pos));
    menu->deleteLater();
}

void musicListPage::onSearchMusicLyric() {
    QModelIndexList selectedRows = m_musicListView->selectionModel()->selectedRows();
    if (selectedRows.size() != 1) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请选中一首歌曲进行修改！", 3000, this);
        return;
    }
    QModelIndex selectedIndex = selectedRows.first();
    if (!selectedIndex.isValid()) return;

    QString musicId = selectedIndex.data(Qt::UserRole).toString();
    MusicItem currentMusic = m_musicModel->getMusicById(musicId);
    if (currentMusic.id.isEmpty()) {
        ElaMessageBar::error(ElaMessageBarType::Top, "获取失败", "未找到选中的歌曲信息！", 3000, this);
        return;
    }

    m_searchPage->setMusicID(musicId);
    m_searchPage->show();
    m_searchPage->activateWindow();
}

void musicListPage::onMusicListModify() {
    QModelIndex selectedIndex = m_listView->currentIndex();
    if (!selectedIndex.isValid()) return;

    QString groupId = selectedIndex.data(Tm_listViewModel::IdRole).toString();
    GroupItem group = m_listModel->getGroupById(groupId);
    if (group.id.isEmpty()) return;

    bool ok;
    QString newName = QInputDialog::getText(this, "修改歌单", "请输入新名称：", QLineEdit::Normal, group.name, &ok);

    if (ok && !newName.trimmed().isEmpty() && newName != group.name) {
        group.name = newName.trimmed();
        bool success = m_listModel->updateGroup(group);
        if (success) {
            ElaMessageBar::success(ElaMessageBarType::Top, "修改成功", "歌单名称已更新", 2000, this);
        } else {
            ElaMessageBar::error(ElaMessageBarType::Top, "修改失败", "未能更新歌单名称，请重试！", 3000, this);
        }
    }
}

void musicListPage::onMusicListDelete() {
    QModelIndex selectedIndex = m_listView->currentIndex();
    if (!selectedIndex.isValid()) return;

    QString groupId = selectedIndex.data(Tm_listViewModel::IdRole).toString();
    GroupItem group = m_listModel->getGroupById(groupId);
    if (group.id.isEmpty()) return;

    bool ok = QMessageBox::question(this, "确认删除",
        QString("确定要删除歌单「%1」吗？\n删除后将移除该歌单下的所有歌曲关联。").arg(group.name),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;

    if (ok) {
        bool success = m_listModel->deleteGroup(groupId);
        if (success) {
            ElaMessageBar::success(ElaMessageBarType::Top, "删除成功", QString("歌单「%1」已删除").arg(group.name), 2000, this);

            if (m_listModel->rowCount() > 0) {
                int nextRow = qMin(selectedIndex.row(), m_listModel->rowCount() - 1);
                QModelIndex nextIndex = m_listModel->index(nextRow);
                m_listView->setCurrentIndex(nextIndex);
                onMusicListItemClicked(nextIndex);
                m_musicModel->loadMusicByGroupId(nextIndex.data(Tm_listViewModel::IdRole).toString());
            } else {
                m_listNameTitle->setText("暂无歌单");
                m_listInfo->setText("请新建歌单");
                m_musicListCoverLabel->setPixmap(QPixmap());
                m_musicModel->loadMusicByGroupId("");
            }
        } else {
            ElaMessageBar::error(ElaMessageBarType::Top, "删除失败", "未能删除歌单，请重试！", 3000, this);
        }
    }
}

void musicListPage::onMusicListAddNew() {
    bool ok;
    QString name = QInputDialog::getText(this, "新建歌单", "请输入歌单名称：", QLineEdit::Normal, "", &ok);

    if (ok && !name.trimmed().isEmpty()) {
        bool success = m_listModel->addGroup(name.trimmed());
        if (success) {
            ElaMessageBar::success(ElaMessageBarType::Top, "创建成功", QString("歌单「%1」已创建").arg(name.trimmed()), 2000, this);

            int lastRow = m_listModel->rowCount() - 1;
            QModelIndex newIndex = m_listModel->index(lastRow);
            m_listView->setCurrentIndex(newIndex);
            onMusicListItemClicked(newIndex);
            m_musicModel->loadMusicByGroupId(newIndex.data(Tm_listViewModel::IdRole).toString());
        } else {
            ElaMessageBar::error(ElaMessageBarType::Top, "创建失败", "新建歌单失败，请重试！", 3000, this);
        }
    }
}

void musicListPage::onSongImport() {
    // 检查导入状态，避免重复导入
    if (m_isImporting) {
        ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "正在导入中，请稍后！", 3000, this);
        return;
    }

    QString currentGroupId = m_musicModel->getCurrentGroupId();
    if (currentGroupId.isEmpty())    {
        ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "请先选中一个歌单再导入歌曲！", 3000, this);
        return;
    }

    // 选择音频文件
    QStringList filters;
    filters << "音频文件 (*.mp3 *.flac *.wav *.m4a *.ogg)"
            << "所有文件 (*.*)";
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this, "选择音频文件", QDir::homePath(), filters.join(";;")
        );

    if (filePaths.isEmpty()) return;

    // 更新导入状态，启动线程
    m_isImporting = true;
    m_importBtn->setEnabled(false);  // 禁用导入按钮，防止重复点击
    m_listImportBtn->setEnabled(false);

    // 发射信号触发子线程导入（避免直接调用子线程对象的方法）
    emit startImport(filePaths, currentGroupId);
}

void musicListPage::onSongNameModify() {
    QModelIndexList selectedRows = m_musicListView->selectionModel()->selectedRows();
    if (selectedRows.size() != 1) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请选中一首歌曲进行修改！", 3000, this);
        return;
    }
    QModelIndex selectedIndex = selectedRows.first();
    if (!selectedIndex.isValid()) return;

    QString musicId = selectedIndex.data(Qt::UserRole).toString();
    MusicItem currentMusic = m_musicModel->getMusicById(musicId);
    if (currentMusic.id.isEmpty()) {
        ElaMessageBar::error(ElaMessageBarType::Top, "修改失败", "未找到选中的歌曲信息！", 3000, this);
        return;
    }

    bool ok;
    // 点击其他列（默认：歌曲名）：仅修改歌曲名
    QString newName = QInputDialog::getText(
        this, "修改歌曲名", "请输入新的歌曲名：",
        QLineEdit::Normal, currentMusic.name, &ok
        );
    if (!ok || newName.trimmed().isEmpty()) return;
    currentMusic.name = newName.trimmed();

    // 统一更新数据库
    bool updateSuccess = m_musicModel->updateMusic(currentMusic);
    if (updateSuccess) {
        ElaMessageBar::success(ElaMessageBarType::Top, "修改成功", "歌曲信息已更新！", 2000, this);
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "修改失败", "未能更新歌曲信息，请重试！", 3000, this);
    }

    // 重置点击列索引（避免下次复用错误列信息）
    m_clickedSongColumn = -1;
}

void musicListPage::onSongAutherModify() {
    QModelIndexList selectedRows = m_musicListView->selectionModel()->selectedRows();
    if (selectedRows.size() != 1) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请选中一首歌曲进行修改！", 3000, this);
        return;
    }
    QModelIndex selectedIndex = selectedRows.first();
    if (!selectedIndex.isValid()) return;

    QString musicId = selectedIndex.data(Qt::UserRole).toString();
    MusicItem currentMusic = m_musicModel->getMusicById(musicId);
    if (currentMusic.id.isEmpty()) {
        ElaMessageBar::error(ElaMessageBarType::Top, "修改失败", "未找到选中的歌曲信息！", 3000, this);
        return;
    }

    bool ok;
    // 点击歌手列：仅修改歌手名
    QString newAuther = QInputDialog::getText(
        this, "修改歌手", "请输入新的歌手名：",
        QLineEdit::Normal, currentMusic.auther, &ok
        );
    if (!ok || newAuther.trimmed().isEmpty()) return;
    currentMusic.auther = newAuther.trimmed();

    // 统一更新数据库
    bool updateSuccess = m_musicModel->updateMusic(currentMusic);
    if (updateSuccess) {
        ElaMessageBar::success(ElaMessageBarType::Top, "修改成功", "歌曲信息已更新！", 2000, this);
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "修改失败", "未能更新歌曲信息，请重试！", 3000, this);
    }

    // 重置点击列索引（避免下次复用错误列信息）
    m_clickedSongColumn = -1;
}

void musicListPage::onSongDelete() {
    QModelIndexList selectedRows = m_musicListView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请先选中要删除的歌曲！", 3000, this);
        return;
    }

    int rowCount = selectedRows.size();
    bool ok = QMessageBox::question(
                  this, "确认删除",
                  QString("确定要删除选中的 %1 首歌曲吗？\n歌曲将从当前歌单中移除（不删除源文件）。").arg(rowCount),
                  QMessageBox::Yes | QMessageBox::No
                  ) == QMessageBox::Yes;

    if (!ok) return;

    QString currentGroupId = m_musicModel->getCurrentGroupId();
    int deleteCount = 0;
    for (const QModelIndex& index : selectedRows) {
        if (!index.isValid()) continue;
        QString musicId = index.data(Qt::UserRole).toString();
        if (m_musicModel->removeMusicFromGroup(currentGroupId, musicId)) {
            deleteCount++;
        }
    }

    ElaMessageBar::information(
        ElaMessageBarType::Top,
        "删除完成",
        QString("共选中 %1 首歌曲，成功删除 %2 首！").arg(rowCount).arg(deleteCount),
        3000,
        this
        );
    int remainingCount = m_musicModel->rowCount();
    m_listInfo->setText(QString("当前收录 %1 首").arg(remainingCount));
}

void musicListPage::onMusicListItemClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    // 新增：切换歌单时清空搜索框和搜索状态
    _searchEdit->clear();
    m_musicModel->clearSearch();
    m_musicModel->clearImgCache();

    // 以下为原代码（不变）
    QString groupId = index.data(Tm_listViewModel::IdRole).toString();
    if (groupId.isEmpty()) return;
    updateSelectedGroupInfo(groupId);

    bool loadSuccess = m_musicModel->loadMusicByGroupId(groupId);
    if (!loadSuccess) {
        ElaMessageBar::error(ElaMessageBarType::Top, "加载失败", "未能加载当前歌单的歌曲，请重试！", 3000, this);
    }

    int musicCount = m_musicModel->rowCount();
    m_listInfo->setText(QString("当前收录 %1 首").arg(musicCount));
}

void musicListPage::onSongItemClicked(const QModelIndex& index) {
    if (!index.isValid()) return;

    QString musicId = index.data(Qt::UserRole).toString();
    MusicItem currentMusic = m_musicModel->getMusicById(musicId);
    if (currentMusic.id.isEmpty()) return;

    ElaMessageBar::success(ElaMessageBarType::Top, "信息", "添加到播放列表", 3000, this);

    m_playListModel->addMusicToCurrentPos(currentMusic.id);
    m_playListModel->markAsPlayed();
}

void musicListPage::onImportProgress(int importedCount, int totalCount) {
    // 实时更新导入进度提示
    ElaMessageBar::information(ElaMessageBarType::Top, "导入中",
                               QString("已导入 %1/%2 首歌曲").arg(importedCount).arg(totalCount),
                               1000, this);
}

void musicListPage::onImportFinished(int successCount, int totalCount) {
    qDebug() << "================================================================================================================================================";

    // 恢复UI状态
    m_isImporting = false;
    m_importBtn->setEnabled(true);
    m_listImportBtn->setEnabled(true);

    // 刷新歌曲列表（重新加载当前歌单）
    QString currentGroupId = m_musicModel->getCurrentGroupId();
    QString currentKeyword = _searchEdit->text().trimmed();
    if (m_musicModel->isSearching() && !currentGroupId.isEmpty() && !currentKeyword.isEmpty()) {
        m_musicModel->searchMusic(currentGroupId, currentKeyword);
    } else {
        m_musicModel->loadMusicByGroupId(currentGroupId);
    }

    // 显示导入结果
    ElaMessageBar::information(ElaMessageBarType::Top,
                               "导入完成",
                               QString("共选择 %1 个文件，成功导入 %2 首歌曲！").arg(totalCount).arg(successCount),
                               3000, this);

    // 更新歌单歌曲计数
    int remainingCount = m_musicModel->rowCount();
    m_listInfo->setText(QString("当前收录 %1 首").arg(remainingCount));
}

void musicListPage::onImportError(const QString& errorMsg) {
    // 恢复UI状态
    m_isImporting = false;
    m_importBtn->setEnabled(true);
    m_listImportBtn->setEnabled(true);

    // 显示错误信息
    ElaMessageBar::error(ElaMessageBarType::Top, "导入失败",
                         QString("导入过程中发生错误：%1").arg(errorMsg),
                         3000, this);
}

/**
 * @brief 歌单右键「播放」- 加载并播放整个歌单
 */
void musicListPage::onMusicListPlay() {
    QModelIndex selectedIndex = m_listView->currentIndex();
    if (!selectedIndex.isValid()) return;

    QString groupId = selectedIndex.data(Tm_listViewModel::IdRole).toString();
    GroupItem group = m_listModel->getGroupById(groupId);
    if (group.id.isEmpty()) return;

    // 调用播放列表模型的方法：清空列表并加载歌单音乐
    bool playSuccess = m_playListModel->playGroupMusic(groupId);
    if (playSuccess) {
        ElaMessageBar::success(ElaMessageBarType::Top, "播放成功",
                               QString("开始播放歌单「%1」").arg(group.name), 2000, this);

        // 自动切换到播放列表的第一首歌曲（若存在）
        if (m_playListModel->rowCount() > 0) {
            m_playListModel->switchToMusic(1); // 切换到sort_id=1的歌曲（第一首）
        }
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "播放失败",
                             m_playListModel->getLastError(), 3000, this);
    }
}

/**
 * @brief 歌曲右键「下一首播放」- 插入当前播放歌曲后
 */
void musicListPage::onSongPlayNext() {
    QModelIndexList selectedRows = m_musicListView->selectionModel()->selectedRows();
    if (selectedRows.size() != 1) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请选中一首歌曲", 3000, this);
        return;
    }

    QModelIndex selectedIndex = selectedRows.first();
    QString musicId = selectedIndex.data(Qt::UserRole).toString();
    MusicItem music = m_musicModel->getMusicById(musicId);
    if (music.id.isEmpty()) return;

    // 调用播放列表模型：在当前播放歌曲后添加
    bool addSuccess = m_playListModel->addMusicToCurrentPos(musicId);
    if (addSuccess) {
        ElaMessageBar::success(ElaMessageBarType::Top, "操作成功",
                               QString("歌曲「%1」已设为下一首播放").arg(music.name), 2000, this);
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "操作失败",
                             m_playListModel->getLastError(), 3000, this);
    }
}

/**
 * @brief 歌曲右键「添加到队列」- 追加到播放列表末尾
 */
void musicListPage::onSongAddToQueue() {
    QModelIndexList selectedRows = m_musicListView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        ElaMessageBar::information(ElaMessageBarType::Top, "提示", "请选中至少一首歌曲", 3000, this);
        return;
    }

    int addCount = 0;
    for (const QModelIndex& index : selectedRows) {
        QString musicId = index.data(Qt::UserRole).toString();
        MusicItem music = m_musicModel->getMusicById(musicId);
        if (music.id.isEmpty()) continue;

        // 调用播放列表模型：追加到末尾
        if (m_playListModel->addMusicToEnd(musicId)) {
            addCount++;
        }
    }

    ElaMessageBar::success(ElaMessageBarType::Top, "操作成功",
                           QString("成功添加 %1 首歌曲到播放队列").arg(addCount), 2000, this);
}
