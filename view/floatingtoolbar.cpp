#include "floatingtoolbar.h"
#include <QScreen>
#include <QApplication>
#include <QMouseEvent>
#include <QStyle>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>
#include "MusicDataStruct.h"
#include "MusicPlayer.h"
#include "ElaMessageBar.h"
#include "tableFloatLyricsWidget.h"

FloatingToolbar::FloatingToolbar(QWidget *parent)
    : QWidget(parent)
{
    initUI();

    // 设置窗口属性
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int x = screenGeometry.width() - width() - 30;  // 向右偏移20像素
        int y = screenGeometry.height() - height() - 30; // 向下偏移20像素
        move(x, y);
    }

    // 初始化动画
    initAnimations();

    // 初始化延迟计时器，设置为1秒后触发
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true); // 只触发一次
    m_hideTimer->setInterval(300);   // 1000毫秒 = 1秒

    m_playListToolBar = new playListToolBar();
    m_playListToolBar->setVisible(false);

    // 连接计时器信号到收起动画槽函数
    connect(m_hideTimer, &QTimer::timeout, this, &FloatingToolbar::startEnterAnimation);
    connect(m_listBtn, &ElaToolButton::clicked, this, &FloatingToolbar::onQueueButtonClicked);
    connect(m_homeBtn, &ElaToolButton::clicked, this, &FloatingToolbar::showMainPage);
    connect(MusicPlayer::getInstance(), &MusicPlayer::playStateChanged, this, &FloatingToolbar::setPlayState);
    connect(m_playPauseBtn, &ElaToolButton::clicked, MusicPlayer::getInstance(), &MusicPlayer::playPause);
    connect(m_orderBtn, &ElaToolButton::clicked, this, &FloatingToolbar::showOrderSelectMenu);

    m_searchPage = new searchPage(0);
    m_searchPage->hide();

    installEventFilter(this);
}

void FloatingToolbar::showOrderSelectMenu()
{
    ElaMenu* menu = new ElaMenu(this);
    menu->setMenuItemHeight(30);

    // 获取歌单列表
    QString sql = "SELECT id, name, img FROM group_data ORDER BY id";
    QVector<QMap<QString, QVariant>> result = SqliteManager::getInstance().querySql(sql);

    QVector<GroupItem> groupItems;
    if (SqliteManager::getInstance().getLastError().isEmpty()) {
        // 解析查询结果
        for (const auto &row : std::as_const(result)) {
            GroupItem group;
            group.id = row["id"].toString();
            group.name = row["name"].toString();
            group.img = row["img"].toString();
            groupItems.append(group);
        }
    } else {
        qWarning() << "加载歌单失败：" << SqliteManager::getInstance().getLastError();
    }

    // 创建"随机一首"主菜单项及二级菜单
    QAction* orderRandAction = menu->addElaIconAction(ElaIconType::Dice, "随机一首");
    ElaMenu* randSubMenu = new ElaMenu(menu);
    randSubMenu->setMenuItemHeight(30);
    orderRandAction->setMenu(randSubMenu);

    // 为每个歌单添加随机点歌二级菜单项，每个二级菜单项包含三级菜单
    for (const auto& group : groupItems) {
        ElaMenu* randGroupSubMenu = new ElaMenu(randSubMenu);
        randGroupSubMenu->setMenuItemHeight(30);

        QAction* groupAction = new QAction(group.name, randSubMenu);
        groupAction->setMenu(randGroupSubMenu);
        randSubMenu->addAction(groupAction);

        // 添加三级菜单：立即播放
        QAction* playNowAction = randGroupSubMenu->addAction("立即播放");
        Tm_musicPlayListModel *playListModel = Tm_musicPlayListModel::getInstance();
        connect(playNowAction, &QAction::triggered, this, [this, playListModel, groupId = group.id]() {
            // emit groupRandomClicked(groupId, PlayMode::PlayNow);
            if (playListModel->orderRandomMusicFromGroup(groupId, true)) {
                playListModel->markAsPlayed();
                qDebug() << "随机点歌成功";
                ElaMessageBar::success(
                    ElaMessageBarType::Top,          // 消息栏位置（顶部）
                    "成功",                  // 消息标题
                    "点歌成功",  // 成功内容
                    3000,                        // 显示时长（3秒）
                    this                         // 目标显示组件（当前页面）
                    );
            } else {
                qDebug() << "随机点歌失败：" << playListModel->getLastError();
                ElaMessageBar::error(
                    ElaMessageBarType::Top,          // 消息栏位置（顶部）
                    "错误",                  // 消息标题
                    "点歌失败",  // 成功内容
                    3000,                        // 显示时长（3秒）
                    this                         // 目标显示组件（当前页面）
                    );
            }
        });

        // 添加三级菜单：下一首播放
        QAction* playNextAction = randGroupSubMenu->addAction("下一首播放");
        connect(playNextAction, &QAction::triggered, this, [this, playListModel, groupId = group.id]() {
            // emit groupRandomClicked(groupId, PlayMode::PlayNext);
            if (playListModel->orderRandomMusicFromGroup(groupId, true)) {
                qDebug() << "随机点歌成功";
                ElaMessageBar::success(
                    ElaMessageBarType::Top,          // 消息栏位置（顶部）
                    "成功",                  // 消息标题
                    "点歌成功",  // 成功内容
                    3000,                        // 显示时长（3秒）
                    this                         // 目标显示组件（当前页面）
                    );
            } else {
                qDebug() << "随机点歌失败：" << playListModel->getLastError();
                ElaMessageBar::error(
                    ElaMessageBarType::Top,          // 消息栏位置（顶部）
                    "错误",                  // 消息标题
                    "点歌失败",  // 成功内容
                    3000,                        // 显示时长（3秒）
                    this                         // 目标显示组件（当前页面）
                    );
            }
        });

        // 添加三级菜单：添加到列表
        QAction* addToListAction = randGroupSubMenu->addAction("添加到列表");
        connect(addToListAction, &QAction::triggered, this, [this, playListModel, groupId = group.id]() {
            // emit groupRandomClicked(groupId, PlayMode::AddToList);
            if (playListModel->orderRandomMusicFromGroup(groupId, false)) {
                qDebug() << "随机点歌成功";
                ElaMessageBar::success(
                    ElaMessageBarType::Top,          // 消息栏位置（顶部）
                    "成功",                  // 消息标题
                    "点歌成功",  // 成功内容
                    3000,                        // 显示时长（3秒）
                    this                         // 目标显示组件（当前页面）
                    );
            } else {
                qDebug() << "随机点歌失败：" << playListModel->getLastError();
                ElaMessageBar::error(
                    ElaMessageBarType::Top,          // 消息栏位置（顶部）
                    "错误",                  // 消息标题
                    "点歌失败",  // 成功内容
                    3000,                        // 显示时长（3秒）
                    this                         // 目标显示组件（当前页面）
                    );
            }
        });
    }

    // 创建"序号点歌"主菜单项及二级菜单
    QAction* orderByIDAction = menu->addElaIconAction(ElaIconType::InputNumeric, "序号点歌");
    ElaMenu* idSubMenu = new ElaMenu(menu);
    idSubMenu->setMenuItemHeight(30);
    orderByIDAction->setMenu(idSubMenu);

    // 为每个歌单添加序号点歌二级菜单项，每个二级菜单项包含三级菜单
    for (const auto& group : groupItems) {
        ElaMenu* idGroupSubMenu = new ElaMenu(idSubMenu);
        idGroupSubMenu->setMenuItemHeight(30);

        QAction* groupAction = new QAction(group.name, idSubMenu);
        groupAction->setMenu(idGroupSubMenu);
        idSubMenu->addAction(groupAction);

        // 添加三级菜单：立即播放
        QAction* playNowAction = idGroupSubMenu->addAction("立即播放");
        connect(playNowAction, &QAction::triggered, this, [this, groupId = group.id, groupName = group.name]() {
            showSongIndexDialog(groupId, groupName, PlayMode::PlayNow);
        });

        // 添加三级菜单：下一首播放
        QAction* playNextAction = idGroupSubMenu->addAction("下一首播放");
        connect(playNextAction, &QAction::triggered, this, [this, groupId = group.id, groupName = group.name]() {
            showSongIndexDialog(groupId, groupName, PlayMode::PlayNext);
        });

        // 添加三级菜单：添加到列表
        QAction* addToListAction = idGroupSubMenu->addAction("添加到列表");
        connect(addToListAction, &QAction::triggered, this, [this, groupId = group.id, groupName = group.name]() {
            showSongIndexDialog(groupId, groupName, PlayMode::AddToList);
        });
    }

    // 搜索点歌菜单项
    QAction* orderByNameAction = menu->addElaIconAction(ElaIconType::MagnifyingGlassChart, "搜索点歌");
    connect(orderByNameAction, &QAction::triggered, this, [this]() {
        m_searchPage->show();
    });

    // 计算菜单显示位置
    QPoint btnPos = m_orderBtn->mapToGlobal(QPoint(0, 0));
    int x = btnPos.x() + (m_orderBtn->width() - menu->width()) / 2;
    int y = btnPos.y() - menu->height() - 70;

    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenRect = screen->availableGeometry();
        if (x < screenRect.left()) x = screenRect.left();
        if (x + menu->width() > screenRect.right())
            x = screenRect.right() - menu->width();
        if (y < screenRect.top()) y = screenRect.top();
    }

    menu->exec(QPoint(x, y));
    menu->deleteLater();
}

// 辅助函数：显示歌曲序号输入对话框并处理结果
void FloatingToolbar::showSongIndexDialog(const QString& groupId, const QString& groupName, PlayMode playMode)
{
    // 查询当前歌单的歌曲数量
    QString countSql = QString("SELECT COUNT(*) as count FROM music_group WHERE group_id = '%1'").arg(groupId);
    QVector<QMap<QString, QVariant>> countResult = SqliteManager::getInstance().querySql(countSql);

    int songCount = 0;
    if (!countResult.isEmpty() && SqliteManager::getInstance().getLastError().isEmpty()) {
        songCount = countResult.first()["count"].toInt();
    }

    if (songCount <= 0) {
        QMessageBox::information(this, "提示", groupName + "中没有歌曲");
        return;
    }

    // 显示输入对话框，限制范围为1到歌曲总数
    bool ok;
    int songIndex = QInputDialog::getInt(
        this, "输入序号", QString("请输入在%1中点歌的序号(1-%2)").arg(groupName).arg(songCount),
        1, 1, songCount, 1, &ok
        );

    // 只有用户点击确认按钮时才触发信号
    if (ok) {
        Tm_musicPlayListModel *playListModel = Tm_musicPlayListModel::getInstance();
        if (playListModel->orderMusicFromGroupBySortId(groupId, songIndex, playMode!=PlayMode::AddToList)) {
            if (playMode==PlayMode::PlayNow) playListModel->markAsPlayed();
            qDebug() << "随机点歌成功";
            ElaMessageBar::success(
                ElaMessageBarType::Top,          // 消息栏位置（顶部）
                "成功",                  // 消息标题
                "点歌成功",  // 成功内容
                3000,                        // 显示时长（3秒）
                this                         // 目标显示组件（当前页面）
                );
        } else {
            qDebug() << "随机点歌失败：" << playListModel->getLastError();
            ElaMessageBar::error(
                ElaMessageBarType::Top,          // 消息栏位置（顶部）
                "错误",                  // 消息标题
                "点歌失败",  // 成功内容
                3000,                        // 显示时长（3秒）
                this                         // 目标显示组件（当前页面）
                );
        }
        // emit groupOrderByIdClicked(groupId, songIndex, playMode);  // 发送歌单ID、选中的序号和播放模式
    }
}

void FloatingToolbar::setPlayState(bool isPlaying)
{
    // 根据播放状态更新按钮图标
    if (isPlaying) {
        m_playPauseBtn->setElaIcon(ElaIconType::CirclePause);
    } else {
        m_playPauseBtn->setElaIcon(ElaIconType::CircleCaretRight);
    }
}

FloatingToolbar::~FloatingToolbar()
{
    // 清理动画对象和计时器
    delete m_slideInAnimation;
    delete m_slideOutAnimation;
    delete m_hideTimer;
}

void FloatingToolbar::onQueueButtonClicked()
{
    if (m_playListToolBar->isVisible()) {
        m_playListToolBar->hide();
    } else {
        QPoint btnPos = m_listBtn->mapToGlobal(QPoint(0, 0));
        int x = btnPos.x() + (m_listBtn->width() - m_playListToolBar->width()) / 2;
        int y = btnPos.y() - m_playListToolBar->height() - 10;

        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenRect = screen->availableGeometry();
            if (x < screenRect.left()) x = screenRect.left();
            if (x + m_playListToolBar->width() > screenRect.right())
                x = screenRect.right() - m_playListToolBar->width();
            if (y < screenRect.top()) y = screenRect.top();
        }

        m_playListToolBar->move(x, y);
        m_playListToolBar->show();
    }
}

void FloatingToolbar::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    QHBoxLayout *BtnLayout = new QHBoxLayout();
    BtnLayout->setContentsMargins(0, 0, 0, 0);

    // 创建按钮容器
    m_btnContainer = new QWidget(this);
    m_btnContainer->setFixedWidth(220);

    QHBoxLayout *btnLayout = new QHBoxLayout(m_btnContainer);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(0);

    // 主页按钮 - 添加悬浮提示
    m_homeBtn = new ElaToolButton(m_btnContainer);
    m_homeBtn->setElaIcon(ElaIconType::House);
    m_homeBtn->setIsTransparent(false);
    m_homeBtn->setFixedSize(40, 40);
    m_homeBtn->setIconSize(QSize(30, 30));
    m_homeBtn->setBorderRadius(5);
    m_homeBtn->setToolTip("返回主页"); // 主页按钮提示

    // 桌面歌词按钮 - 添加悬浮提示
    m_tableLyricsBtn = new ElaToolButton(m_btnContainer);
    m_tableLyricsBtn->setElaIcon(ElaIconType::SquarePollHorizontal);
    m_tableLyricsBtn->setIsTransparent(false);
    m_tableLyricsBtn->setFixedSize(40, 40);
    m_tableLyricsBtn->setIconSize(QSize(30, 30));
    m_tableLyricsBtn->setBorderRadius(5);
    m_tableLyricsBtn->setToolTip("切换桌面歌词"); // 桌面歌词按钮提示

    // 播放/暂停按钮 - 添加悬浮提示
    m_playPauseBtn = new ElaToolButton(m_btnContainer);
    m_playPauseBtn->setElaIcon(ElaIconType::CircleCaretRight);
    m_playPauseBtn->setIsTransparent(false);
    m_playPauseBtn->setFixedSize(40, 40);
    m_playPauseBtn->setIconSize(QSize(30, 30));
    m_playPauseBtn->setBorderRadius(5);
    m_playPauseBtn->setToolTip("播放/暂停"); // 播放暂停按钮提示

    // 点歌按钮 - 添加悬浮提示
    m_orderBtn = new ElaToolButton(m_btnContainer);
    m_orderBtn->setElaIcon(ElaIconType::AlbumCollection);
    m_orderBtn->setIsTransparent(false);
    m_orderBtn->setFixedSize(40, 40);
    m_orderBtn->setIconSize(QSize(30, 30));
    m_orderBtn->setBorderRadius(5);
    m_orderBtn->setToolTip("点歌菜单"); // 点歌按钮提示

    // 播放列表按钮 - 添加悬浮提示
    m_listBtn = new ElaToolButton(m_btnContainer);
    m_listBtn->setElaIcon(ElaIconType::Ballot);
    m_listBtn->setIsTransparent(false);
    m_listBtn->setFixedSize(40, 40);
    m_listBtn->setIconSize(QSize(30, 30));
    m_listBtn->setBorderRadius(5);
    m_listBtn->setToolTip("显示播放列表"); // 播放列表按钮提示

    btnLayout->addWidget(m_homeBtn);
    btnLayout->addWidget(m_tableLyricsBtn);
    btnLayout->addWidget(m_playPauseBtn);
    btnLayout->addWidget(m_orderBtn);
    btnLayout->addWidget(m_listBtn);

    m_coverLabel = new ElaText(this);
    m_coverLabel->setFixedSize(55, 55);
    m_coverLabel->setStyleSheet("border-radius: 10px; background-color: #ccc;");
    m_coverLabel->setPixmap(getRoundRectPixmap(QPixmap(":/cloudbread/src/favicon_round.png"), m_coverLabel->size(), 10));
    // connect(m_coverLabel, &ElaText::mouseDoubleClickEvent, this, &FloatingToolbar::showMainPage);

    setFixedSize(285, 200);

    BtnLayout->addStretch();
    BtnLayout->addWidget(m_btnContainer);
    BtnLayout->addWidget(m_coverLabel);

    mainLayout->addStretch();
    mainLayout->addLayout(BtnLayout);

    connect(m_tableLyricsBtn, &ElaToolButton::clicked, tableFloatLyricsWidget::getInstance(), &tableFloatLyricsWidget::toggleLyrics);

    // 默认隐藏按钮
    // m_btnContainer->setVisible(false);
    outFlag = true;
}

void FloatingToolbar::initAnimations()
{
    // 按钮滑入动画
    m_slideInAnimation = new QPropertyAnimation(m_btnContainer, "pos");
    m_slideInAnimation->setDuration(300);
    m_slideInAnimation->setEasingCurve(QEasingCurve::OutCubic);

    // 按钮滑出动画
    m_slideOutAnimation = new QPropertyAnimation(m_btnContainer, "pos");
    m_slideOutAnimation->setDuration(300);
    m_slideOutAnimation->setEasingCurve(QEasingCurve::InCubic);

    // 记录初始位置
    m_initialBtnPos = QPoint(m_btnContainer->pos().x(), m_btnContainer->pos().y() + 135);
    startEnterAnimation();
}

// 处理鼠标移入事件
void FloatingToolbar::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);

    // 停止计时器，取消收起操作
    m_hideTimer->stop();

    if (!outFlag) return;
    outFlag = false;

    // 如果动画正在运行，先停止
    if (m_slideOutAnimation->state() == QAbstractAnimation::Running) {
        m_slideOutAnimation->stop();
    }

    // 显示按钮容器并执行滑入动画
    m_btnContainer->setVisible(true);

    // 设置动画起始和结束位置
    QPoint startPos = QPoint(m_coverLabel->pos().x(), m_coverLabel->pos().y());
    QPoint endPos = m_initialBtnPos;

    m_slideInAnimation->setStartValue(startPos);
    m_slideInAnimation->setEndValue(endPos);
    m_slideInAnimation->start();
}

// 处理鼠标移出事件
void FloatingToolbar::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    // 启动计时器，1秒后触发收起动画
    m_hideTimer->start();
}

void FloatingToolbar::startEnterAnimation() {
    outFlag = true;
    // 如果动画正在运行，先停止
    if (m_slideInAnimation->state() == QAbstractAnimation::Running) {
        m_slideInAnimation->stop();
    }

    // 设置滑出动画
    QPoint endPos = QPoint(m_coverLabel->pos().x(), m_coverLabel->pos().y());

    m_slideOutAnimation->setStartValue(m_btnContainer->pos());
    m_slideOutAnimation->setEndValue(endPos);

    // 修复信号槽连接语法
    connect(m_slideOutAnimation, &QPropertyAnimation::finished,
            this, [this]() {
                m_btnContainer->setVisible(false);
            }, Qt::SingleShotConnection);

    m_slideOutAnimation->start();
}

bool FloatingToolbar::eventFilter(QObject *watched, QEvent *event)
{
    static QPoint lastPoint;
    static bool isPressed = false;

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        if (this->rect().contains(e->pos()) && (e->button() == Qt::LeftButton)) {
            lastPoint = e->pos();
            isPressed = true;
        }
    } else if (event->type() == QEvent::MouseMove && isPressed) {
        QMouseEvent *e = static_cast<QMouseEvent *>(event);
        int dx = e->pos().x() - lastPoint.x();
        int dy = e->pos().y() - lastPoint.y();

        this->move(this->x() + dx, this->y() + dy);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease && isPressed) {
        isPressed = false;
    }

    return QWidget::eventFilter(watched, event);
}

QPixmap FloatingToolbar::getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius)
{
    //不处理空数据或者错误数据
    if (srcPixMap.isNull()) {
        return srcPixMap;
    }

    //获取图片尺寸
    int imageWidth = size.width();
    int imageHeight = size.height();

    //处理大尺寸的图片,保证图片显示区域完整
    QPixmap newPixMap = srcPixMap.scaled(imageWidth, (imageHeight == 0 ? imageWidth : imageHeight),
                                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap destImage(imageWidth, imageHeight);
    destImage.fill(Qt::transparent);
    QPainter painter(&destImage);
    // 抗锯齿
    painter.setRenderHints(QPainter::Antialiasing, true);
    // 图片平滑处理
    painter.setRenderHints(QPainter::SmoothPixmapTransform, true);
    // 将图片裁剪为圆角
    QPainterPath path;
    QRect rect(0, 0, imageWidth, imageHeight);
    path.addRoundedRect(rect, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, imageWidth, imageHeight, newPixMap);
    return destImage;
}
