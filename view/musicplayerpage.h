#ifndef MUSICPLAYERPAGE_H
#define MUSICPLAYERPAGE_H

#include "ElaScrollPage.h"
#include "lyricwidget.h"
#include "musicplayerbase.h"
#include "LyricFetcherThread.h"

class musicPlayerPage : public ElaScrollPage
{
    Q_OBJECT

public:
    musicPlayerPage(QWidget* parent = nullptr);

private slots:
    // 接收歌曲信息更新信号的槽函数
    void onSongInfoUpdated(const QString& auther, const QString& name, const QPixmap& cover, int pitch, bool hasLyrics);
    void updateMusicCover(const QString &musicId, const QString &newImgName);

private:
    void initPage();
    void lyricUpdate(const LyricData& lyricData);

    QPixmap getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius);

    musicplayerbase *_musicplayerbase;
    ElaText *m_coverLabel;

    ElaText *m_musicTitleLabel;
    ElaText *m_musicAutherLabel;
    LyricsWeight *m_lyricWidget;

    // 保存当前歌曲ID（用于歌词加载等后续操作）
    QString m_currentMusicId;
};

#endif // MUSICPLAYERPAGE_H
