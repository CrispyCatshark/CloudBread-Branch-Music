#include "musicplayer.h"
#include <QUrl>
#include <QImage>
#include <QDebug>
#include <QMediaDevices>
#include <QAudioDevice>
#include <globalconfig.h>

// 静态成员初始化
QPointer<MusicPlayer> MusicPlayer::m_instance = nullptr;
QMutex MusicPlayer::m_mutex;

MusicPlayer::MusicPlayer(QObject *parent)
    : QObject(parent),
    m_player(nullptr),
    m_audioOutput(nullptr),
    m_pitch(0),
    m_isPlaying(false),
    m_skipPositionUpdate(false),
    m_currentDeviceId("")
{

    // 连接线程启动和初始化播放器的信号槽
    connect(&m_workerThread, &QThread::started, this, &MusicPlayer::initPlayer);
    connect(&m_workerThread, &QThread::finished, this, &QObject::deleteLater);

    // 连接播放列表的播放请求信号
    connect(Tm_musicPlayListModel::getInstance(), &Tm_musicPlayListModel::playMusic,
            this, &MusicPlayer::onPlayMusic);
    connect(Tm_musicPlayListModel::getInstance(), &Tm_musicPlayListModel::stopPlayback,
            this, &MusicPlayer::stop);

    // 监听音频设备变化 - 修正：使用静态方法而非instance()
    QMediaDevices *deviceList = new QMediaDevices(this);
    connect (deviceList, &QMediaDevices::audioOutputsChanged, this, &MusicPlayer::audioDevicesChanged);

    // 将当前对象移动到工作线程
    moveToThread(&m_workerThread);
    // 启动工作线程
    m_workerThread.start();
}

MusicPlayer::~MusicPlayer()
{
    // 停止线程
    m_workerThread.quit();
    m_workerThread.wait();

    // 清理播放器
    if (m_player) {
        m_player->stop();
        delete m_player;
        m_player = nullptr;
    }

    if (m_audioOutput) {
        delete m_audioOutput;
        m_audioOutput = nullptr;
    }
}

// initCurrentPlayingMusic()方法实现
void MusicPlayer::initCurrentPlayingMusic()
{
    // 从播放列表模型获取当前正在播放的歌曲
    QString currentPlayingMusicId = Tm_musicPlayListModel::getInstance()->getCurrentPlayingMusicId(); // 需确保模型提供获取播放列表项的接口

    // 若存在正在播放的歌曲，加载其信息但不播放
    if (!currentPlayingMusicId.isEmpty()) {
        MusicDetail detail = Tm_musicPlayListModel::getInstance()->getMusicDetail(currentPlayingMusicId);
        if (!detail.id.isEmpty() && !detail.filePath.isEmpty()) {
            // 加载媒体文件但不播放
            m_player->setSource(QUrl::fromLocalFile(detail.filePath));
            // 更新当前歌曲信息（封面、歌手、歌名）
            updateCurrentSongInfo(currentPlayingMusicId);
            // 初始化进度为0
            emit progressUpdated(0, m_player->duration());
            qDebug() << "启动初始化：已加载当前播放歌曲（未播放），musicId=" << currentPlayingMusicId;
        }
    } else {
        qDebug() << "启动初始化：无当前播放歌曲";
    }
}

MusicPlayer* MusicPlayer::getInstance(QObject *parent)
{
    // 双重检查锁定确保线程安全
    if (!m_instance) {
        QMutexLocker locker(&m_mutex);
        if (!m_instance) {
            m_instance = new MusicPlayer(parent);
        }
    }
    return m_instance;
}

void MusicPlayer::initPlayer()
{
    // 初始化音频输出
    m_audioOutput = new QAudioOutput();

    // 初始化播放器
    m_player = new QMediaPlayer();
    m_player->setAudioOutput(m_audioOutput);

    // 连接播放器信号槽
    connect(m_player, &QMediaPlayer::mediaStatusChanged,
            this, &MusicPlayer::onMediaStatusChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &MusicPlayer::onPositionChanged);
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &MusicPlayer::onDurationChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &MusicPlayer::onErrorOccurred);

    // 加载音频设备配置
    loadAudioDeviceConfig();

    // 设置初始音量
    m_audioOutput->setVolume(GlobalConfig::getInstance().getValue("audio/volume", 0.8).toDouble()); // QAudioOutput的音量范围是0.0-1.0
}

void MusicPlayer::loadAudioDeviceConfig()
{
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    m_currentDeviceId = defaultDevice.id();

    // 从配置中读取音频输出设备
    QString savedDeviceId = GlobalConfig::getInstance().getValue("audio/deviceID", m_currentDeviceId).toString();

    // 获取所有可用音频输出设备 - 修正：使用静态方法
    const QList<QAudioDevice> audioDevices = QMediaDevices::audioOutputs();

    // 检查保存的设备是否存在
    if (!savedDeviceId.isEmpty()) {
        for (const QAudioDevice &device : audioDevices) {
            if (device.id() == savedDeviceId) {
                m_currentDeviceId = savedDeviceId;
                m_audioOutput->setDevice(device);
                qDebug() << "已加载音频输出设备: " << device.description();
                break;
            }
        }
    }
}

void MusicPlayer::loadAndPlay(const QString& filePath)
{
    if (filePath.isEmpty()) {
        emit errorOccurred("无效的音乐文件路径");
        return;
    }

    // 加载音乐文件前先停止当前播放
    m_player->stop();

    // 标记为跳过即将到来的位置更新（避免短暂的0位置触发UI更新）
    m_skipPositionUpdate = true;

    // 加载音乐文件
    m_player->setSource(QUrl::fromLocalFile(filePath));
    m_player->play();
    m_isPlaying = true;
    emit playStateChanged(true);
}

void MusicPlayer::updateCurrentSongInfo(const QString& musicId)
{
    if (musicId.isEmpty()) return;

    // 获取音乐详情
    MusicDetail detail = Tm_musicPlayListModel::getInstance()->getMusicDetail(musicId);
    if (detail.id.isEmpty()) {
        emit errorOccurred("无法获取音乐详情");
        return;
    }

    // 更新当前音乐ID
    m_currentMusicId = musicId;

    // 加载封面图片
    QPixmap cover;
    if (!detail.imgName.isEmpty()) {
        cover.load(detail.imgName);
    }

    // 发送歌曲信息更新信号
    emit songInfoUpdated(detail.auther, detail.name, cover, -detail.pitch, Tm_musicPlayListModel::getInstance()->isMusicHasLrc(musicId));
    emit lyricSwitched(musicId);
}

bool MusicPlayer::isPlaying() const
{
    return m_isPlaying;
}

int MusicPlayer::volume() const
{
    return 100 * (1.0 - GlobalConfig::getInstance().getValue("audio/volume", 0.8).toDouble());
}

qint64 MusicPlayer::position() const
{
    if (m_player) {
        return m_player->position();
    }
    return 0;
}

qint64 MusicPlayer::duration() const
{
    if (m_player) {
        return m_player->duration();
    }
    return 0;
}

MusicDetail MusicPlayer::currentMusicDetail() const
{
    return Tm_musicPlayListModel::getInstance()->getMusicDetail(m_currentMusicId);
}

QStringList MusicPlayer::getAudioOutputDevices() const
{
    QStringList deviceList;
    // 修正：使用静态方法获取设备列表
    const QList<QAudioDevice> audioDevices = QMediaDevices::audioOutputs();

    for (const QAudioDevice &device : audioDevices) {
        deviceList << device.description();
    }

    return deviceList;
}

QString MusicPlayer::getCurrentOutputDeviceId() const
{
    return m_currentDeviceId;
}

void MusicPlayer::playPause()
{
    if (!m_player) return;

    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        m_isPlaying = false;
        emit playStateChanged(false);
    } else {
        m_player->play();
        m_isPlaying = true;
        emit playStateChanged(true);
    }
}

void MusicPlayer::stop()
{
    if (!m_player) return;

    m_player->stop();
    m_isPlaying = false;
    emit playStateChanged(false);
    emit progressUpdated(0, m_player->duration());
}

void MusicPlayer::next()
{
    // 调用播放列表模型的方法切换到下一首
    Tm_musicPlayListModel::getInstance()->markAsPlayed();
}

void MusicPlayer::previous()
{
    // 调用播放列表模型的prevMusic()方法切换到上一首
    Tm_musicPlayListModel* model = Tm_musicPlayListModel::getInstance();
    bool success = model->prevMusic();

    if (!success) {
        // 如果切换失败，发送错误信息
        emit errorOccurred(model->getLastError());
    }
}

void MusicPlayer::setVolume(int volume)
{
    if (m_audioOutput) {
        // 转换为0.0-1.0范围
        qreal normalizedVolume = 1 - qBound(0, volume, 100) / 100.0;
        GlobalConfig::getInstance().setValue("audio/volume", normalizedVolume);
        m_audioOutput->setVolume(normalizedVolume);
    }
}

void MusicPlayer::setPosition(qint64 positionMs)
{
    if (m_player) {
        // 防止在切换歌曲时手动设置位置
        if (m_skipPositionUpdate) {
            m_skipPositionUpdate = false;
            return;
        }

        // 确保位置在有效范围内
        if (positionMs >= 0 && positionMs <= m_player->duration()) {
            m_player->setPosition(positionMs);
        }
    }
}

void MusicPlayer::setPitch(int pitch)
{
    m_pitch = -pitch;
    Tm_musicPlayListModel::getInstance()->setMusicPitch(m_currentMusicId, m_pitch);
    // 实际应用中可能需要音频处理库来实现音调调整
    // 这里只是保存音调值
}

void MusicPlayer::setAudioOutputDevice(const QString& deviceId)
{
    if (!m_audioOutput || deviceId.isEmpty()) return;

    // 查找设备ID对应的音频设备 - 修正：使用静态方法
    const QList<QAudioDevice> audioDevices = QMediaDevices::audioOutputs();
    for (const QAudioDevice &device : audioDevices) {
        if (device.id() == deviceId) {
            m_audioOutput->setDevice(device);
            m_currentDeviceId = deviceId;

            GlobalConfig::getInstance().setValue("audio/deviceID", m_currentDeviceId);

            qDebug() << "已切换音频输出设备: " << device.description();
            return;
        }
    }

    emit errorOccurred("无法找到指定的音频输出设备");
}

void MusicPlayer::playIndex(int index)
{
    Tm_musicPlayListModel* model = Tm_musicPlayListModel::getInstance();
    if (index >= 0 && index < model->rowCount()) {
        PlayListItem item = model->getPlayItem(index);
        onPlayMusic(item.musicId);
    }
}

void MusicPlayer::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::EndOfMedia) {
        // 当前歌曲播放完毕，切换到下一首
        next();
    } else if (status == QMediaPlayer::LoadedMedia) {
        // 媒体加载完成，更新进度
        m_skipPositionUpdate = false;
        emit progressUpdated(0, m_player->duration());
    } else if (status == QMediaPlayer::InvalidMedia) {
        // 无效的媒体文件，尝试切换到下一首
        emit errorOccurred("无效的媒体文件，正在尝试下一首...");
        next();
    }
}

void MusicPlayer::onPositionChanged(qint64 position)
{
    // 如果需要跳过这次位置更新，则直接返回
    if (m_skipPositionUpdate) {
        m_skipPositionUpdate = false;
        return;
    }

    // 发送进度更新信号
    emit progressUpdated(position, m_player->duration());
}

void MusicPlayer::onDurationChanged(qint64 duration)
{
    // 发送进度更新信号
    emit progressUpdated(m_player->position(), duration);
}

void MusicPlayer::onErrorOccurred(QMediaPlayer::Error error)
{
    if (error != QMediaPlayer::NoError) {
        emit errorOccurred(m_player->errorString());

        // 对于致命错误，尝试切换到下一首
        if (error == QMediaPlayer::ResourceError ||
            error == QMediaPlayer::FormatError) {
            next();
        }
    }
}

void MusicPlayer::onPlayMusic(const QString& musicId)
{
    if (musicId.isEmpty()) return;

    qDebug() << "=================================================播放："<<musicId;

    // 获取音乐详情
    MusicDetail detail = Tm_musicPlayListModel::getInstance()->getMusicDetail(musicId);
    if (detail.id.isEmpty()) {
        emit errorOccurred("找不到音乐信息: " + musicId);
        return;
    }

    // 更新歌曲信息
    updateCurrentSongInfo(musicId);

    // 加载并播放音乐
    loadAndPlay(detail.filePath);
}
