#ifndef MUSICPLAYERBASE_H
#define MUSICPLAYERBASE_H

#include <ElaWidget.h>
#include <ElaSlider.h>
#include <ElaIcon.h>
#include <ElaText.h>
#include <ElaToolButton.h>
#include <ElaToolTip.h>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "playlisttoolbar.h"

class musicplayerbase : public QWidget
{
    Q_OBJECT

public:
    explicit musicplayerbase(QWidget *parent = nullptr);

    // 设置当前播放歌曲信息
    void setSongInfo(const QString &singer, const QString &title, const QPixmap &cover, int pitch);

    // 设置播放进度
    void setProgress(int currentMs, int totalMs);

    // 设置音量
    void setVolume(int value);

    // 设置播放/暂停按钮状态
    void setPlayState(bool isPlaying);

signals:
    // 播放/暂停按钮点击
    void playPauseClicked();

    // 上一首按钮点击
    void prevClicked();

    // 下一首按钮点击
    void nextClicked();

    // 音量改变
    void volumeChanged(int value);

    // 进度条拖动
    void progressChanged(int ms);

    // 升调按钮点击
    void pitchUpClicked();

    // 降调按钮点击
    void pitchDownClicked();

private:
    void onVolumeButtonClicked();
    void onPitchButtonClicked();

    // 播放队列按钮点击
    void onQueueButtonClicked();
    void onProgressSliderReleased();
    void onAudioDeviceButtonClicked();
    void onAudioDevicesChanged();

    void createVolumePopup();
    void createPitchPopup();
    void createAudioDevicePopup();
    void updateMusicCover(const QString &musicId, const QString &newImgName);

    QPixmap getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius);
    QString formatTime(int ms);

    // 播放控制按钮
    ElaToolButton *m_prevBtn;
    ElaToolButton *m_playPauseBtn;
    ElaToolButton *m_nextBtn;

    // 音量控制
    ElaToolButton *m_volumeBtn;
    ElaToolButton *m_tableLyricsBtn;

    // 音调控制
    ElaToolButton *m_pitchBtn;

    // 播放队列
    ElaToolButton *m_queueBtn;

    ElaToolButton* m_audioDeviceBtn;

    // 歌曲信息
    ElaText *m_coverLabel;
    ElaText *m_songNameLabel;
    ElaText *m_songAutherLabel;

    // 进度控制
    ElaSlider *m_progressBar;
    ElaText *m_timeLabel;  // 显示当前时间/总时间

    QWidget *m_volumePopup;
    ElaSlider *m_volumeSlider;

    QWidget *m_pitchPopup;
    ElaSlider *m_pitchSlider;

    playListToolBar *m_playListToolBar;

    QWidget* m_audioDevicePopup;      // 音频设备选择弹窗
    QComboBox* m_deviceComboBox;      // 设备选择下拉框
    bool m_isProgressDragging;        // 进度条拖拽状态标记
};

#endif // MUSICPLAYERBASE_H
