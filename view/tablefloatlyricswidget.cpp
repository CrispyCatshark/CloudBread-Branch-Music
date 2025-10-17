#include "tableFloatlyricswidget.h"
#include "lyricandcoverbatchfetcher.h"
#include <qboxlayout.h>
#include <qevent.h>
#include <qpainter.h>
#include <qpainterpath.h>
#include <QScreen>
#include <QApplication>
#include <qdebug.h>
#include "MusicPlayer.h"
#include "ElaTheme.h"
#include "LrcParser.h"
#include "GlobalConfig.h"  // 引入全局配置类

// 单例实例初始化
tableFloatLyricsWidget* tableFloatLyricsWidget::m_instance = nullptr;

tableFloatLyricsWidget::tableFloatLyricsWidget(QWidget* parent)
    : QWidget(parent),
    m_isDragging(false),
    m_moved(false),
    m_animation(nullptr),
    m_isStateLoaded(false),  // 初始未加载状态
    m_configGroup("FloatLyricsWidget"),  // 配置分组（避免键名冲突）
    m_keyPosX("posX"),                  // 窗口X坐标键
    m_keyPosY("posY"),                  // 窗口Y坐标键
    m_keyIsVisible("isVisible")         // 窗口显示状态键
{
    initPage();
    loadStateFromConfig();  // 初始化后加载历史状态
}

tableFloatLyricsWidget::~tableFloatLyricsWidget()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
    delete m_animation;
    delete m_lyricsWeight;
}

tableFloatLyricsWidget* tableFloatLyricsWidget::getInstance(QWidget* parent)
{
    if (!m_instance) {
        m_instance = new tableFloatLyricsWidget(parent);
    }
    return m_instance;
}

void tableFloatLyricsWidget::initPage()
{
    // 1. 窗口基础配置
    setWindowIcon(QIcon(":/cloudbread/src/favicon_round.png"));
    setFixedSize(400, 500);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::SubWindow);
    setAttribute(Qt::WA_TranslucentBackground);

    // 2. 动画初始化
    m_animation = new QPropertyAnimation(this, "geometry", this);
    m_animation->setDuration(200);

    // 3. 布局与歌词控件
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    m_lyricsWeight = new LyricsWeight(this);
    mainLayout->addWidget(m_lyricsWeight);

    // 4. 主题监听
    m_themeMode = eTheme->getThemeMode();
    connect(eTheme, &ElaTheme::themeModeChanged, this, &tableFloatLyricsWidget::onThemeChanged);

    // 5. 歌词同步
    connect(LyricFetcherThread::getInstance(), &LyricFetcherThread::lyricUpdated,
            this, &tableFloatLyricsWidget::lyricUpdate);
    connect(MusicPlayer::getInstance(), &MusicPlayer::progressUpdated,
            m_lyricsWeight, &LyricsWeight::updateLyrics);
}

// 从配置加载位置与显示状态
void tableFloatLyricsWidget::loadStateFromConfig()
{
    if (m_isStateLoaded) return;

    GlobalConfig& config = GlobalConfig::getInstance();
    QString groupPrefix = m_configGroup + "/";

    // 1. 加载显示状态（默认隐藏，避免启动弹窗）
    bool savedIsVisible = config.getValue(groupPrefix + m_keyIsVisible, false).toBool();

    // 2. 加载位置（无配置时用默认值，适配多屏幕）
    QRect screenGeo = getScreenGeometry();
    int defaultX = screenGeo.width() - 10;    // 默认右侧留10px（部分显示）
    int defaultY = screenGeo.height() / 4;    // 默认垂直居中偏上
    int savedX = config.getValue(groupPrefix + m_keyPosX, defaultX).toInt();
    int savedY = config.getValue(groupPrefix + m_keyPosY, defaultY).toInt();

    // 3. 位置校验（防止多屏幕切换后窗口超出可视范围）
    QRect validScreen = getScreenGeometry();
    // 确保窗口至少5px在屏幕内（避免完全不可见）
    if (savedX + width() < 5 || savedX > validScreen.width() - 5) {
        savedX = defaultX;
    }
    if (savedY + height() < 5 || savedY > validScreen.height() - 5) {
        savedY = defaultY;
    }

    // 4. 应用加载的状态
    move(savedX, savedY);                  // 恢复位置
    savedIsVisible ? showLyrics() : hideLyrics();  // 恢复显示/隐藏

    m_isStateLoaded = true;
    qDebug() << "[FloatLyrics] 加载状态：位置(" << savedX << "," << savedY << "), 显示:" << savedIsVisible;
}

// 核心：保存位置 + 显示状态到配置
void tableFloatLyricsWidget::saveStateToConfig()
{
    GlobalConfig& config = GlobalConfig::getInstance();
    QString groupPrefix = m_configGroup + "/";

    // 1. 保存当前位置（窗口左上角坐标，拖动后实时更新）
    QPoint currentPos = frameGeometry().topLeft();
    config.setValue(groupPrefix + m_keyPosX, currentPos.x());
    config.setValue(groupPrefix + m_keyPosY, currentPos.y());

    // 2. 保存当前显示状态（关键：同步记录显示/隐藏）
    bool currentIsVisible = !isVisible();
    config.setValue(groupPrefix + m_keyIsVisible, currentIsVisible);

    // 3. 强制同步到文件（避免程序异常退出时丢失）
    config.sync();
    qDebug() << "[FloatLyrics] 保存状态：位置(" << currentPos.x() << "," << currentPos.y() << "), 显示:" << currentIsVisible;
    bool savedIsVisible = config.getValue(groupPrefix + m_keyIsVisible, false).toBool();
}

// 歌词更新逻辑（无修改）
void tableFloatLyricsWidget::lyricUpdate(const LyricData& lyricData)
{
    LrcParser parser;
    m_lyricData = lyricData;

    QVector<LineLyric> mainLyrics;
    if (!lyricData.yrc.isEmpty() && parser.parseFromString(lyricData.yrc)) {
        mainLyrics = parser.getCharLyrics();
    } else {
        qDebug() << "[FloatLyrics] 歌词解析出错";
    }

    QVector<LineLyric> transLyrics;
    if (!lyricData.trans.isEmpty() && parser.parseFromString(lyricData.trans)) {
        transLyrics = parser.getCharLyrics();
    }

    QVector<LineLyric> romaLyrics;
    if (!lyricData.roma.isEmpty() && parser.parseFromString(lyricData.roma)) {
        romaLyrics = parser.getCharLyrics();
    }

    m_lyricsWeight->setLyrics(mainLyrics, transLyrics, romaLyrics);
}

// 显示歌词窗口（无修改）
void tableFloatLyricsWidget::showLyrics()
{
    if (!isVisible()) {
        show();
        lyricUpdate(m_lyricData);
    }
}

// 隐藏歌词窗口（无修改）
void tableFloatLyricsWidget::hideLyrics()
{
    hide();
}

// 切换显示/隐藏（无修改）
void tableFloatLyricsWidget::toggleLyrics()
{
    saveStateToConfig();
    isVisible() ? hideLyrics() : showLyrics();
}

// 鼠标进入事件（无修改）
void tableFloatLyricsWidget::enterEvent(QEnterEvent *event)
{
    hideOrShow(true);
    QWidget::enterEvent(event);
}

// 鼠标离开事件（无修改）
void tableFloatLyricsWidget::leaveEvent(QEvent *event)
{
    hideOrShow(false);
    QWidget::leaveEvent(event);
}

// 窗口绘制（无修改）
void tableFloatLyricsWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QRectF widgetRect = rect();
    QPainterPath path;
    path.addRoundedRect(widgetRect, 10, 10);

    painter.setPen(Qt::NoPen);
    painter.setBrush(ElaThemeColor(m_themeMode, WindowBase));
    painter.drawPath(path);
    painter.setClipPath(path);

    QWidget::paintEvent(event);
}

// 鼠标按下事件（拖动开始，无修改）
void tableFloatLyricsWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragStartGlobalPos = event->globalPos();
        m_windowStartPos = frameGeometry().topLeft();
        // 拖动时停止动画（避免位置冲突）
        if (m_animation->state() == QAbstractAnimation::Running) {
            m_animation->stop();
        }
    }
    QWidget::mousePressEvent(event);
}

// 鼠标移动事件（拖动过程，无修改）
void tableFloatLyricsWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        QPoint dragOffset = event->globalPos() - m_dragStartGlobalPos;
        move(m_windowStartPos + dragOffset);
    }
    QWidget::mouseMoveEvent(event);
}

// 关键修改：鼠标松开时强制保存位置 + 显示状态
void tableFloatLyricsWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isDragging) {
        m_isDragging = false;
        // -------------------------- 核心修改点 --------------------------
        // 鼠标松开时立即保存：当前位置 + 最新显示状态（无论是否拖动成功）
        saveStateToConfig();
        // -----------------------------------------------------------------
        // 拖动后检查是否需要隐藏（原有逻辑保留）
        if (!underMouse()) {
            hideOrShow(false);
        }
    }
    QWidget::mouseReleaseEvent(event);
}

// 主题变化响应（无修改）
void tableFloatLyricsWidget::onThemeChanged(ElaThemeType::ThemeMode themeMode)
{
    m_themeMode = themeMode;
    update();
}

// 获取屏幕几何信息（适配多屏幕，无修改）
QRect tableFloatLyricsWidget::getScreenGeometry() const
{
    QScreen* screen = QApplication::screenAt(pos());
    if (!screen) {
        screen = QApplication::primaryScreen();
    }
    return screen ? screen->geometry() : QRect();
}

// 显示/隐藏逻辑（含动画，无修改）
void tableFloatLyricsWidget::hideOrShow(bool show)
{
    if (!isVisible()) return;

    QRect screenGeo = getScreenGeometry();
    QRect windowGeo = frameGeometry();
    int x = windowGeo.x();
    int y = windowGeo.y();

    // 右侧边缘处理
    if (x + windowGeo.width() > screenGeo.width() - 10) {
        show ? startAnimation(screenGeo.width() - windowGeo.width(), y)
             : startAnimation(screenGeo.width() - 5, y);
        m_moved = !show;
    }
    // 左侧边缘处理
    else if (x < 10) {
        show ? startAnimation(0, y) : startAnimation(5 - windowGeo.width(), y);
        m_moved = !show;
    }
    // 顶部边缘处理
    else if (y < 10) {
        show ? startAnimation(x, 0) : startAnimation(x, 5 - windowGeo.height());
        m_moved = !show;
    }
    else {
        m_moved = false;
    }
}

// 启动位置动画（无修改）
void tableFloatLyricsWidget::startAnimation(int x, int y)
{
    QRect startRect = frameGeometry();
    QRect endRect(x, y, startRect.width(), startRect.height());

    if (m_animation->state() == QAbstractAnimation::Running) {
        m_animation->stop();
    }

    m_animation->setStartValue(startRect);
    m_animation->setEndValue(endRect);
    m_animation->start();
}
