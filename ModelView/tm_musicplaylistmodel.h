#ifndef TM_MUSICPLAYLISTMODEL_H
#define TM_MUSICPLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QCache>
#include <QPixmap>
#include "SqliteManager.h"

// 播放列表项数据结构（对应数据库 play_list 表）
struct PlayListItem {
    QString id;          // 播放列表项ID（char64，UUID）
    int sortId;          // 排序ID
    QString musicId;     // 关联的音乐ID（char64）
    bool isPlaying;      // 是否正在播放（对应数据库 playing=1）
    bool isPlayed;       // 是否已播放（对应数据库 played=1）
};

// 音乐详情数据结构（通过 musicId 从 music_data 表查询）
struct MusicDetail {
    QString id;          // 音乐ID
    QString name;        // 歌名
    QString auther;      // 作者
    int duration;        // 时长（秒）
    QString imgName;     // 封面路径
    int pitch;          // 升降调（int）
    int playCount;       // 播放次数
    QString filePath;    // 文件路径
};

class Tm_musicPlayListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // 单例模式核心接口
    static Tm_musicPlayListModel* getInstance(QObject *parent = nullptr);

    // 禁用拷贝构造和赋值运算符（防止多实例）
    Tm_musicPlayListModel(const Tm_musicPlayListModel&) = delete;
    Tm_musicPlayListModel& operator=(const Tm_musicPlayListModel&) = delete;

    // 表格列定义（固定3列：歌名、作者、时长）
    enum ColumnIndex {
        NameColumn = 0,    // 歌名
        AutherColumn,      // 作者
        DurationColumn,    // 时长
        StatusColumn,      // 状态
        ColumnCount        // 列总数
    };

    ~Tm_musicPlayListModel() override;

    // 模型核心接口（QAbstractTableModel 重写）
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // 播放列表核心功能
    bool loadPlayList();                                  // 加载播放列表（从 play_list 表）
    bool addMusicToCurrentPos(const QString &musicId);    // 当前位置后添加一首（下一首播放）
    bool addMusicToEnd(const QString &musicId);           // 结尾添加一首
    bool switchToMusic(int targetSortId);                 // 切换到指定sortId的歌曲
    bool markAsPlayed();                                  // 标记当前歌曲为已播放，切换下一首
    bool isMusicHasLrc(const QString &musicId) const;

    /**
     * @brief 播放指定歌单（清空当前列表并加载歌单内所有音乐）
     * @param groupId 目标歌单ID（关联group_data表的id）
     * @return 操作成功返回true
     */
    bool playGroupMusic(const QString &groupId);

    /**
     * @brief 根据sort_id删除播放列表中的歌曲
     * @param targetSortId 待删除歌曲的sort_id
     * @return 删除成功返回true
     */
    bool deleteMusicBySortId(int targetSortId);

    /**
     * @brief 一键清空整个播放列表
     * @details 清空表中所有数据，同时重置内存缓存；若存在正在播放的歌曲，清空后播放状态归零
     * @return 清空成功返回true
     */
    bool clearPlayList();

    /**
     * @brief 切换到当前播放歌曲的上一首
     * @details 若当前是第一首，则无操作；否则标记当前为已播放，切换至上一首并更新播放状态
     * @return 切换成功返回true，无有效上一首时返回false
     */
    bool prevMusic();

    /**
     * @brief 设置指定音乐的音调（pitch）
     * @param music_id 目标音乐的ID（关联music_data表的id）
     * @param pitch 待设置的音调值
     * @return 操作成功返回true，失败返回false（可通过getLastError获取原因）
     */
    bool setMusicPitch(const QString &music_id, int pitch);

    /**
     * @brief 从指定歌单中选择指定歌曲点歌（按sort_id定位歌曲）
     * @param groupId 歌单ID（关联music_group表）
     * @param groupMusicSortId 歌单内歌曲的排序ID（music_group表中的排序逻辑）
     * @param addToNext true=添加到下一首播放，false=添加到列表结尾
     * @return 点歌成功返回true，失败返回false（可通过getLastError获取原因）
     */
    bool orderMusicFromGroupBySortId(const QString &groupId, int groupMusicSortId, bool addToNext);

    /**
     * @brief 从指定歌单中随机选择一首歌曲点歌
     * @param groupId 歌单ID（关联music_group表）
     * @param addToNext true=添加到下一首播放，false=添加到列表结尾
     * @return 点歌成功返回true，失败返回false（可通过getLastError获取原因）
     */
    bool orderRandomMusicFromGroup(const QString &groupId, bool addToNext);

    // 辅助接口
    QString getCurrentPlayingMusicId() const;             // 获取当前播放的音乐ID
    QString getLastError() const;                         // 获取最后错误信息
    void clearImgCache();                                 // 清理封面缓存
    PlayListItem getPlayItem(int row) const;
    MusicDetail getMusicDetail(const QString &musicId) const; // 通过musicId查询音乐详情

signals:
    /**
     * @brief 触发音乐播放
     * @param musicId 待播放的音乐ID
     */
    void playMusic(const QString& musicId);

    /**
     * @brief 触发播放停止
     */
    void stopPlayback();

private:
    explicit Tm_musicPlayListModel(QObject *parent = nullptr);

    // 单例实例指针
    static Tm_musicPlayListModel* m_instance;

    // 内部工具函数
    bool initPlayListTable();                             // 初始化 play_list 表（若不存在）
    QString formatDuration(int seconds) const;            // 格式化时长（秒→mm:ss）
    QPixmap getRoundRectPixmap(const QPixmap &src, const QSize &size, int radius) const; // 圆角封面
    bool setMusicAsPlaying(const QString& targetMusicId);
    bool updateMusicPlayCount(const QString& musicId, bool inTransaction);

    /**
     * @brief 私有工具函数：获取指定歌单内所有音乐的ID列表
     * @param groupId 歌单ID
     * @param[out] outMusicIds 输出参数：存储歌单内所有音乐ID的列表
     * @return 获取成功返回true，失败返回false（错误信息存入m_lastError）
     */
    bool getMusicIdsFromGroup(const QString &groupId, QVector<QString> &outMusicIds);

private:
    SqliteManager &m_db;                                  // SQLite管理类（单例引用）
    QVector<PlayListItem> m_playItems;                    // 播放列表数据
    QString m_lastError;                                  // 最后错误信息
    mutable QCache<QString, QIcon> m_imgCache{1000};            // 封面缓存（key:musicId）
};

#endif // TM_MUSICPLAYLISTMODEL_H
