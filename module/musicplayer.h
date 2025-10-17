#ifndef MUSICPLAYER_H
#define MUSICPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QThread>
#include <QMutex>
#include <QPointer>
#include <QStringList>
#include <QSettings>
#include <qmediadevices.h>
#include "Tm_musicPlayListModel.h"

class MusicPlayer : public QObject
{
    Q_OBJECT
public:
    // 单例模式接口
    static MusicPlayer* getInstance(QObject *parent = nullptr);

    // 禁止拷贝
    MusicPlayer(const MusicPlayer&) = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    // 播放器状态查询
    bool isPlaying() const;
    int volume() const;
    qint64 position() const;
    qint64 duration() const;
    MusicDetail currentMusicDetail() const;

    // 获取音频输出设备列表
    QStringList getAudioOutputDevices() const;
    QString getCurrentOutputDeviceId() const;

    void initCurrentPlayingMusic();

signals:
    // 播放状态变更信号
    void playStateChanged(bool isPlaying);
    // 进度更新信号
    void progressUpdated(qint64 positionMs, qint64 durationMs);
    // 歌曲信息更新信号
    void songInfoUpdated(const QString& singer, const QString& title, const QPixmap& cover, int pitch, bool hasLyrics);
    // 错误发生信号
    void errorOccurred(const QString& errorString);
    // 音频设备列表变更信号
    void audioDevicesChanged();
    // 更新歌词
    void lyricSwitched(const QString& musicId);

public slots:
    // 播放控制槽函数
    void playPause();
    void stop();
    void next();
    void previous();
    void playIndex(int index);

    // 音量和进度控制
    void setVolume(int volume);
    void setPosition(qint64 positionMs);
    void setPitch(int pitch);

    // 音频输出设备设置
    void setAudioOutputDevice(const QString& deviceId);

private slots:
    // 初始化播放器（在工作线程中执行）
    void initPlayer();

    // 媒体状态变更处理
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPositionChanged(qint64 position);
    void onDurationChanged(qint64 duration);
    void onErrorOccurred(QMediaPlayer::Error error);

    // 播放指定音乐
    void onPlayMusic(const QString& musicId);

private:
    // 私有构造函数
    explicit MusicPlayer(QObject *parent = nullptr);
    // 析构函数
    ~MusicPlayer() override;
    // 加载并播放音乐
    void loadAndPlay(const QString& filePath);
    // 更新当前歌曲信息
    void updateCurrentSongInfo(const QString& musicId);
    // 加载音频设备配置
    void loadAudioDeviceConfig();

    // 单例实例
    static QPointer<MusicPlayer> m_instance;
    static QMutex m_mutex;

    // 播放器组件
    QMediaPlayer* m_player;
    QAudioOutput* m_audioOutput;
    QThread m_workerThread;

    // 播放状态变量
    QString m_currentMusicId;
    int m_pitch;
    bool m_isPlaying;
    bool m_skipPositionUpdate;

    // 音频设备信息
    QString m_currentDeviceId;
};

#endif // MUSICPLAYER_H
