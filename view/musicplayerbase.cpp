#include "musicplayerbase.h"
#include <QPixmap>
#include <QFrame>
#include <QPainter>
#include <QScreen>
#include <QPainterPath>
#include <QApplication>
#include <QVBoxLayout>
#include <QComboBox>
#include <QEvent>
#include <QAudioDevice>
#include <QMediaDevices>
#include "musicplayer.h"
#include "lyricfetcherthread.h"
#include "tablefloatlyricswidget.h"

musicplayerbase::musicplayerbase(QWidget *parent)
    : m_isProgressDragging(false)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    QHBoxLayout *ContentLayout = new QHBoxLayout();
    QHBoxLayout *MusicInfoLayout = new QHBoxLayout();
    QVBoxLayout *MusicNameLayout = new QVBoxLayout();
    QHBoxLayout *CenterBtnLayout = new QHBoxLayout();
    QHBoxLayout *RightBtnLayout = new QHBoxLayout();

    m_progressBar = new ElaSlider(this);
    m_progressBar->setRange(0, 1000);  // 使用更大范围获得更精确的控制
    m_progressBar->setValue(0);
    // 进度条拖拽状态监听
    connect(m_progressBar, &ElaSlider::sliderPressed, this, [this]() {
        m_isProgressDragging = true;
    });
    connect(m_progressBar, &ElaSlider::sliderReleased, this, &musicplayerbase::onProgressSliderReleased);

    mainLayout->addWidget(m_progressBar);

    m_coverLabel = new ElaText(this);
    m_coverLabel->setFixedSize(60, 60);
    m_coverLabel->setStyleSheet("border-radius: 10px; background-color: #ccc;");

    m_songNameLabel = new ElaText(this);
    m_songNameLabel->setTextPixelSize(14);
    m_songNameLabel->setText("暂无歌曲信息");

    m_songAutherLabel = new ElaText(this);
    m_songAutherLabel->setText("暂无歌曲信息");
    m_songAutherLabel->setTextPixelSize(12);
    m_songAutherLabel->setStyleSheet("color: #838383;");

    MusicNameLayout->addWidget(m_songNameLabel);
    MusicNameLayout->addWidget(m_songAutherLabel);
    MusicNameLayout->addStretch();

    MusicInfoLayout->addWidget(m_coverLabel);
    MusicInfoLayout->addLayout(MusicNameLayout);
    MusicInfoLayout->addStretch();

    m_pitchBtn = new ElaToolButton(this);
    m_pitchBtn->setElaIcon(ElaIconType::DialMed);
    m_pitchBtn->setFixedSize(36, 36);
    m_pitchBtn->setBorderRadius(23);
    m_pitchBtn->setVisible(false);

    m_tableLyricsBtn = new ElaToolButton(this);
    m_tableLyricsBtn->setElaIcon(ElaIconType::SquarePollHorizontal);
    m_tableLyricsBtn->setFixedSize(36, 36);
    m_tableLyricsBtn->setBorderRadius(23);

    m_prevBtn = new ElaToolButton(this);
    m_prevBtn->setElaIcon(ElaIconType::BackwardStep);
    m_prevBtn->setFixedSize(40, 40);
    m_prevBtn->setIconSize(QSize(35, 35));
    m_prevBtn->setBorderRadius(20);

    m_playPauseBtn = new ElaToolButton(this);
    m_playPauseBtn->setElaIcon(ElaIconType::CircleCaretRight);
    m_playPauseBtn->setFixedSize(50, 50);
    m_playPauseBtn->setIconSize(QSize(46, 46));
    m_playPauseBtn->setBorderRadius(25);

    m_nextBtn = new ElaToolButton(this);
    m_nextBtn->setElaIcon(ElaIconType::ForwardStep);
    m_nextBtn->setFixedSize(40, 40);
    m_nextBtn->setIconSize(QSize(35, 35));
    m_nextBtn->setBorderRadius(20);

    m_volumeBtn = new ElaToolButton(this);
    m_volumeBtn->setElaIcon(ElaIconType::Volume);
    m_volumeBtn->setFixedSize(36, 36);
    m_volumeBtn->setBorderRadius(23);

    CenterBtnLayout->addStretch();
    CenterBtnLayout->addWidget(m_pitchBtn);
    CenterBtnLayout->addWidget(m_tableLyricsBtn);
    CenterBtnLayout->addWidget(m_prevBtn);
    CenterBtnLayout->addWidget(m_playPauseBtn);
    CenterBtnLayout->addWidget(m_nextBtn);
    CenterBtnLayout->addWidget(m_volumeBtn);
    CenterBtnLayout->addStretch();

    m_timeLabel = new ElaText("00:00 / 00:00", this);
    m_timeLabel->setTextPixelSize(12);

    m_audioDeviceBtn = new ElaToolButton(this);
    m_audioDeviceBtn->setElaIcon(ElaIconType::Headphones);
    m_audioDeviceBtn->setFixedSize(36, 36);
    m_audioDeviceBtn->setBorderRadius(23);

    m_queueBtn = new ElaToolButton(this);
    m_queueBtn->setElaIcon(ElaIconType::ListUl);
    m_queueBtn->setFixedSize(36, 36);
    m_queueBtn->setBorderRadius(23);

    RightBtnLayout->addStretch();
    RightBtnLayout->addWidget(m_timeLabel);
    RightBtnLayout->addWidget(m_audioDeviceBtn);
    RightBtnLayout->addWidget(m_queueBtn);

    ContentLayout->addLayout(MusicInfoLayout);
    ContentLayout->addLayout(CenterBtnLayout);
    ContentLayout->addLayout(RightBtnLayout);

    mainLayout->addLayout(ContentLayout);
    createVolumePopup();
    createPitchPopup();
    createAudioDevicePopup();  // 初始化音频设备弹窗

    setSongInfo("未知歌手", "暂无数据", QPixmap(":/cloudbread/src/favicon_round.png"), 0);

    m_playListToolBar = new playListToolBar();
    m_playListToolBar->setVisible(false);

    tableFloatLyricsWidget::getInstance();

    // 基础按钮点击事件连接
    connect(m_queueBtn, &ElaToolButton::clicked, this, &musicplayerbase::onQueueButtonClicked);
    connect(m_volumeBtn, &ElaToolButton::clicked, this, &musicplayerbase::onVolumeButtonClicked);
    connect(m_pitchBtn, &ElaToolButton::clicked, this, &musicplayerbase::onPitchButtonClicked);
    connect(m_audioDeviceBtn, &ElaToolButton::clicked, this, &musicplayerbase::onAudioDeviceButtonClicked);
    connect(m_tableLyricsBtn, &ElaToolButton::clicked, tableFloatLyricsWidget::getInstance(), &tableFloatLyricsWidget::toggleLyrics);

    // 播放器控制连接
    connect(m_playPauseBtn, &ElaToolButton::clicked, MusicPlayer::getInstance(), &MusicPlayer::playPause);
    connect(m_prevBtn, &ElaToolButton::clicked, MusicPlayer::getInstance(), &MusicPlayer::previous);
    connect(m_nextBtn, &ElaToolButton::clicked, MusicPlayer::getInstance(), &MusicPlayer::next);
    connect(m_volumeSlider, &ElaSlider::valueChanged, MusicPlayer::getInstance(), &MusicPlayer::setVolume);
    connect(m_pitchSlider, &ElaSlider::valueChanged, MusicPlayer::getInstance(), &MusicPlayer::setPitch);

    // 播放器信号到UI的连接
    connect(MusicPlayer::getInstance(), &MusicPlayer::playStateChanged, this, &musicplayerbase::setPlayState);
    connect(MusicPlayer::getInstance(), &MusicPlayer::progressUpdated, this, [this](qint64 currentMs, qint64 totalMs) {
        // 拖拽时不更新进度条，避免冲突
        if (!m_isProgressDragging) {
            setProgress(static_cast<int>(currentMs), static_cast<int>(totalMs));
        }
    });
    connect(MusicPlayer::getInstance(), &MusicPlayer::songInfoUpdated, this, &musicplayerbase::setSongInfo);
    connect(MusicPlayer::getInstance(), &MusicPlayer::errorOccurred, this, [this](const QString& error) {
        qWarning() << "播放器错误:" << error;
        // 可添加错误提示UI逻辑
    });

    // 音频设备变化监听
    connect(MusicPlayer::getInstance(), &MusicPlayer::audioDevicesChanged, this, &musicplayerbase::onAudioDevicesChanged);

    // 初始化设备列表
    onAudioDevicesChanged();

    LyricFetcherThread::getInstance()->start();

    connect(LyricFetcherThread::getInstance(), &LyricFetcherThread::imgUpdated, this, &musicplayerbase::updateMusicCover);

    connect(m_volumeSlider, &ElaSlider::valueChanged, this, [this]() {
        if (m_volumeSlider->value() <= 0) {
            m_volumeBtn->setElaIcon(ElaIconType::VolumeXmark);
        } else if (m_volumeSlider->value() < 50) {
            m_volumeBtn->setElaIcon(ElaIconType::VolumeLow);
        } else {
            m_volumeBtn->setElaIcon(ElaIconType::VolumeHigh);
        }
    });
}

void musicplayerbase::updateMusicCover(const QString &musicId, const QString &newImgName)
{
    m_coverLabel->setPixmap(getRoundRectPixmap(QPixmap(newImgName), m_coverLabel->size(), 10));
}

void musicplayerbase::onQueueButtonClicked()
{
    if (m_playListToolBar->isVisible()) {
        m_playListToolBar->hide();
    } else {
        QPoint btnPos = m_queueBtn->mapToGlobal(QPoint(0, 0));
        int x = btnPos.x() + (m_queueBtn->width() - m_playListToolBar->width()) / 2;
        int y = btnPos.y() - m_playListToolBar->height() - 5;

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

// 新增：创建音频设备选择弹窗
void musicplayerbase::createAudioDevicePopup()
{
    m_audioDevicePopup = new QWidget(this, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_audioDevicePopup->setAttribute(Qt::WA_TranslucentBackground);
    m_audioDevicePopup->setFixedSize(200, 150);

    // 弹窗布局
    QVBoxLayout* popupLayout = new QVBoxLayout(m_audioDevicePopup);
    popupLayout->setContentsMargins(10, 10, 10, 10);

    // 设备选择下拉框
    m_deviceComboBox = new QComboBox(m_audioDevicePopup);
    m_deviceComboBox->setFixedSize(180, 30);
    popupLayout->addWidget(m_deviceComboBox);

    // 连接设备选择变更事件
    connect(m_deviceComboBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) {
            const QList<QAudioDevice> audioDevices = QMediaDevices::audioOutputs();
            if (index < audioDevices.size()) {
                MusicPlayer::getInstance()->setAudioOutputDevice(audioDevices[index].id());
            }
        }
    });

    m_audioDevicePopup->hide();
}

// 新增：音频设备按钮点击事件
void musicplayerbase::onAudioDeviceButtonClicked()
{
    if (m_audioDevicePopup->isVisible()) {
        m_audioDevicePopup->hide();
    } else {
        QPoint btnPos = qobject_cast<ElaToolButton*>(sender())->mapToGlobal(QPoint(0, 0));
        int x = btnPos.x() + (sender()->property("width").toInt() - m_audioDevicePopup->width()) / 2;
        int y = btnPos.y() - m_audioDevicePopup->height() - 5;

        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenRect = screen->availableGeometry();
            if (x < screenRect.left()) x = screenRect.left();
            if (x + m_audioDevicePopup->width() > screenRect.right())
                x = screenRect.right() - m_audioDevicePopup->width();
            if (y < screenRect.top()) y = screenRect.top();
        }

        m_audioDevicePopup->move(x, y);
        m_audioDevicePopup->show();
    }
}

void musicplayerbase::createVolumePopup()
{
    m_volumePopup = new QWidget(this, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_volumePopup->setAttribute(Qt::WA_TranslucentBackground);
    m_volumePopup->setFixedSize(30, 120);

    m_volumeSlider = new ElaSlider(Qt::Vertical, m_volumePopup);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(MusicPlayer::getInstance()->volume());  // 初始化音量为播放器当前值
    m_volumeSlider->setInvertedAppearance(true);
    m_volumeSlider->setGeometry(5, 10, 20, 100);

    m_volumePopup->hide();
}

void musicplayerbase::onVolumeButtonClicked()
{
    if (m_volumePopup->isVisible()) {
        m_volumePopup->hide();
    } else {
        QPoint btnPos = m_volumeBtn->mapToGlobal(QPoint(0, 0));
        int x = btnPos.x() + (m_volumeBtn->width() - m_volumePopup->width()) / 2;
        int y = btnPos.y() - m_volumePopup->height() - 5;

        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenRect = screen->availableGeometry();
            if (x < screenRect.left()) x = screenRect.left();
            if (x + m_volumePopup->width() > screenRect.right())
                x = screenRect.right() - m_volumePopup->width();
            if (y < screenRect.top()) y = screenRect.top();
        }

        m_volumePopup->move(x, y);
        m_volumePopup->show();
    }
}

void musicplayerbase::createPitchPopup()
{
    m_pitchPopup = new QWidget(this, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_pitchPopup->setAttribute(Qt::WA_TranslucentBackground);
    m_pitchPopup->setFixedSize(30, 120);

    m_pitchSlider = new ElaSlider(Qt::Vertical, m_pitchPopup);
    m_pitchSlider->setRange(-12, 12);
    m_pitchSlider->setValue(0);
    m_pitchSlider->setInvertedAppearance(true);
    m_pitchSlider->setGeometry(5, 10, 20, 100);

    m_pitchPopup->hide();
}

void musicplayerbase::onPitchButtonClicked()
{
    if (m_pitchPopup->isVisible()) {
        m_pitchPopup->hide();
    } else {
        QPoint btnPos = m_pitchBtn->mapToGlobal(QPoint(0, 0));
        int x = btnPos.x() + (m_pitchBtn->width() - m_pitchPopup->width()) / 2;
        int y = btnPos.y() - m_pitchPopup->height() - 5;

        QScreen *screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenRect = screen->availableGeometry();
            if (x < screenRect.left()) x = screenRect.left();
            if (x + m_pitchPopup->width() > screenRect.right())
                x = screenRect.right() - m_pitchPopup->width();
            if (y < screenRect.top()) y = screenRect.top();
        }

        m_pitchPopup->move(x, y);
        m_pitchPopup->show();
    }
}

void musicplayerbase::setSongInfo(const QString &singer, const QString &title, const QPixmap &cover, int pitch)
{
    m_songNameLabel->setText(title);
    m_songAutherLabel->setText(singer);
    m_coverLabel->setPixmap(getRoundRectPixmap(cover, m_coverLabel->size(), 10));
    m_pitchSlider->setValue(pitch);
}

void musicplayerbase::setProgress(int currentMs, int totalMs)
{
    // 更新进度条
    if (totalMs > 0) {
        int progress = (currentMs * 1000) / totalMs;  // 转换为0-1000范围
        m_progressBar->setValue(progress);
    }

    // 更新时间显示
    QString timeText = QString("%1 / %2")
                           .arg(formatTime(currentMs), formatTime(totalMs));
    m_timeLabel->setText(timeText);
}

void musicplayerbase::setPlayState(bool isPlaying)
{
    // 根据播放状态更新按钮图标
    if (isPlaying) {
        m_playPauseBtn->setElaIcon(ElaIconType::CirclePause);
    } else {
        m_playPauseBtn->setElaIcon(ElaIconType::CircleCaretRight);
    }
}

QString musicplayerbase::formatTime(int ms)
{
    // 将毫秒转换为mm:ss格式
    int seconds = (ms / 1000) % 60;
    int minutes = (ms / 1000) / 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

QPixmap musicplayerbase::getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius)
{
    // 不处理空数据或者错误数据
    if (srcPixMap.isNull()) {
        return srcPixMap;
    }

    // 获取图片尺寸
    int imageWidth = size.width();
    int imageHeight = size.height();

    // 处理大尺寸的图片，保证图片显示区域完整
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

// 新增：进度条拖拽释放后更新播放进度
void musicplayerbase::onProgressSliderReleased()
{
    m_isProgressDragging = false;
    // 获取播放器当前总时长
    qint64 totalMs = MusicPlayer::getInstance()->duration();
    if (totalMs <= 0) return;

    // 计算拖拽后的目标进度（转换为毫秒）
    int sliderValue = m_progressBar->value();
    qint64 targetMs = (sliderValue * totalMs) / 1000;  // 对应进度条0-1000范围

    // 通知播放器跳转进度
    MusicPlayer::getInstance()->setPosition(targetMs);
}

// 新增：音频设备列表变化时更新下拉框
void musicplayerbase::onAudioDevicesChanged()
{
    if (!m_deviceComboBox) return;

    // 保存当前选中的设备ID，避免切换列表后选中状态丢失
    QString currentDeviceId = MusicPlayer::getInstance()->getCurrentOutputDeviceId();

    // 清空并重新填充设备列表
    m_deviceComboBox->clear();
    const QList<QAudioDevice> audioDevices = QMediaDevices::audioOutputs();
    for (const QAudioDevice& device : audioDevices) {
        m_deviceComboBox->addItem(device.description(), device.id());
    }

    // 恢复选中当前设备
    int currentIndex = m_deviceComboBox->findData(currentDeviceId);
    if (currentIndex != -1) {
        m_deviceComboBox->setCurrentIndex(currentIndex);
    }
}
