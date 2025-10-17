#include "musicplayerpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <musicplayer.h>
#include "ElaMessageBar.h"

musicPlayerPage::musicPlayerPage(QWidget* parent)
    : ElaScrollPage(parent),
    m_coverLabel(nullptr),
    m_musicTitleLabel(nullptr),
    m_musicAutherLabel(nullptr),
    m_lyricWidget(nullptr),
    m_currentMusicId("")
{
    this->setTitleVisible(false);
    initPage();
    // 初始化后绑定播放器信号（必须在UI控件初始化完成后调用）
    // 获取MusicPlayer单例
    MusicPlayer* player = MusicPlayer::getInstance();
    if (!player)
    {
        qWarning() << "MusicPlayer instance is null, signal binding failed!";
        return;
    }

    // 1. 绑定歌曲信息更新信号（更新标题、歌手、封面）
    connect(player, &MusicPlayer::songInfoUpdated,
            this, &musicPlayerPage::onSongInfoUpdated);

    // 4. 初始化时主动同步一次当前歌曲信息（避免页面显示默认值）
    MusicDetail currentDetail = player->currentMusicDetail();
    if (!currentDetail.id.isEmpty())
    {
        onSongInfoUpdated(currentDetail.auther, currentDetail.name,
                          QPixmap(currentDetail.imgName), -currentDetail.pitch, true);
        m_currentMusicId = currentDetail.id;
    }
}

void musicPlayerPage::initPage()
{
    _musicplayerbase = new musicplayerbase();

    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout *playerLayout = new QHBoxLayout();
    QHBoxLayout *coverHLayout = new QHBoxLayout();
    QVBoxLayout *coverLayout = new QVBoxLayout();

    m_coverLabel = new ElaText(this);
    m_coverLabel->setFixedSize(400, 400);
    m_coverLabel->setStyleSheet("border-radius: 30px;");
    // 初始封面（无歌曲时显示默认图）
    m_coverLabel->setPixmap(getRoundRectPixmap(QPixmap(":/cloudbread/src/favicon_round.png"), m_coverLabel->size(), 30));

    coverHLayout->addStretch();
    coverHLayout->addWidget(m_coverLabel);
    coverHLayout->addStretch();
    coverHLayout->setStretch(0, 2);
    coverHLayout->setStretch(1, 4);
    coverHLayout->setStretch(2, 1);

    coverLayout->addStretch();
    coverLayout->addLayout(coverHLayout);
    coverLayout->addStretch();

    QVBoxLayout *lyricLayout = new QVBoxLayout();
    QVBoxLayout *lyricCountLayout = new QVBoxLayout();

    m_musicTitleLabel = new ElaText(this);
    m_musicTitleLabel->setTextPixelSize(24);
    m_musicTitleLabel->setText("暂无音乐数据");

    m_musicAutherLabel = new ElaText(this);
    m_musicAutherLabel->setTextPixelSize(16);
    m_musicAutherLabel->setText("未知歌手");

    m_lyricWidget = new LyricsWeight(this);

    lyricCountLayout->addWidget(m_musicTitleLabel);
    lyricCountLayout->addWidget(m_musicAutherLabel);
    lyricCountLayout->addSpacing(10);
    lyricCountLayout->addWidget(m_lyricWidget);
    lyricCountLayout->setStretch(0, 0);
    lyricCountLayout->setStretch(1, 0);
    lyricCountLayout->setStretch(2, 1);

    lyricLayout->addStretch();
    lyricLayout->addLayout(lyricCountLayout);
    lyricLayout->addStretch();
    lyricLayout->setStretch(0, 1);
    lyricLayout->setStretch(1, 5);
    lyricLayout->setStretch(2, 1);

    playerLayout->addLayout(coverLayout);
    playerLayout->addLayout(lyricLayout);
    playerLayout->setStretch(0, 1);
    playerLayout->setStretch(1, 1);

    centerLayout->addLayout(playerLayout);
    centerLayout->addWidget(_musicplayerbase);
    centerLayout->setStretch(0, 1);
    centerLayout->setStretch(1, 0);
    addCentralWidget(centralWidget, true, true, 0);

    connect(LyricFetcherThread::getInstance(),&LyricFetcherThread::lyricUpdated, this, &musicPlayerPage::lyricUpdate);
    connect(MusicPlayer::getInstance(), &MusicPlayer::progressUpdated, m_lyricWidget, &LyricsWeight::updateLyrics);
    connect(LyricFetcherThread::getInstance(), &LyricFetcherThread::imgUpdated, this, &musicPlayerPage::updateMusicCover);
}

void musicPlayerPage::updateMusicCover(const QString &musicId, const QString &newImgName)
{
    m_coverLabel->setPixmap(getRoundRectPixmap(QPixmap(newImgName), m_coverLabel->size(), 10));
}

void musicPlayerPage::lyricUpdate(const LyricData& lyricData) {
    // 创建歌词解析器实例
    LrcParser parser;

    // 解析原歌词
    QVector<LineLyric> mainLyrics;
    if (!lyricData.yrc.isEmpty() && parser.parseFromString(lyricData.yrc)) {
        mainLyrics = parser.getCharLyrics();
    } else {
        qDebug() << "解析出错";
    }

    // 解析翻译歌词
    QVector<LineLyric> transLyrics;
    if (!lyricData.trans.isEmpty() && parser.parseFromString(lyricData.trans)) {
        transLyrics = parser.getCharLyrics();
    }

    // 解析罗马音歌词
    QVector<LineLyric> romaLyrics;
    if (!lyricData.roma.isEmpty() && parser.parseFromString(lyricData.roma)) {
        romaLyrics = parser.getCharLyrics();
    }

    // 将解析后的歌词数据传入LyricsWeight
    m_lyricWidget->setLyrics(mainLyrics, transLyrics, romaLyrics);
}

// 实现：更新歌曲信息（标题、歌手、封面）
void musicPlayerPage::onSongInfoUpdated(const QString& auther, const QString& name, const QPixmap& cover, int pitch, bool hasLyrics)
{
    if (!hasLyrics) ElaMessageBar::information(ElaMessageBarType::Top, "加载中", "首次获取歌词会稍慢，约15~20s，请耐心等待", 10000);
    QVector<LineLyric> mainLyrics;
    m_lyricWidget->setLyrics(mainLyrics, mainLyrics, mainLyrics);
    // 更新标题（为空时显示默认文本）
    if (name.isEmpty())
        m_musicTitleLabel->setText("未知歌曲");
    else
        m_musicTitleLabel->setText(name);

    // 更新歌手（为空时显示默认文本）
    if (auther.isEmpty())
        m_musicAutherLabel->setText("未知歌手");
    else
        m_musicAutherLabel->setText(auther);

    // 更新封面（为空时显示默认图）
    QPixmap newCover = cover.isNull()
                           ? QPixmap(":/cloudbread/src/favicon_round.png")
                           : cover;
    m_coverLabel->setPixmap(getRoundRectPixmap(newCover, m_coverLabel->size(), 30));

    qDebug() << "Updated song info - Title:" << name << ", Auther:" << auther;
}

QPixmap musicPlayerPage::getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius)
{
    if (srcPixMap.isNull()) {
        return srcPixMap;
    }

    int imageWidth = size.width();
    int imageHeight = size.height();

    QPixmap newPixMap = srcPixMap.scaled(imageWidth, (imageHeight == 0 ? imageWidth : imageHeight),
                                         Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPixmap destImage(imageWidth, imageHeight);
    destImage.fill(Qt::transparent);
    QPainter painter(&destImage);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    QPainterPath path;
    QRect rect(0, 0, imageWidth, imageHeight);
    path.addRoundedRect(rect, radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, imageWidth, imageHeight, newPixMap);
    return destImage;
}
