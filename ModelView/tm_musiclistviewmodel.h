#ifndef TM_MUSICLISTVIEWMODEL_H
#define TM_MUSICLISTVIEWMODEL_H

#include <QAbstractTableModel>
#include <QCache>
#include "MusicDataStruct.h"
#include "SqliteManager.h"

class Tm_musicListViewModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // 歌曲列表列定义（对应UI表格的列）
    enum MusicColumn {
        IndexColumn,   // 序号列（#）
        NameColumn,    // 歌曲名列
        AutherColumn,  // 歌手列
        DurationColumn,// 时长列
        TimestampColumn,// 导入时间
        PlayColumn,
        ColumnCount    // 列总数
    };

    explicit Tm_musicListViewModel(QObject *parent = nullptr);

    // 重写QAbstractTableModel纯虚函数
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 歌曲增删查改接口
    bool updateMusicCover(const QString &musicId, const QString &newImgName);
    bool loadMusicByGroupId(const QString &groupId); // 按歌单ID加载歌曲
    bool addMusicToGroup(const QString &groupId, const MusicItem &music); // 向歌单添加歌曲
    bool removeMusicFromGroup(const QString &groupId, const QString &musicId); // 从歌单移除歌曲
    bool updateMusic(const MusicItem &music); // 更新歌曲信息（如播放次数、升降调）
    MusicItem getMusicById(const QString &musicId) const; // 按ID获取歌曲
    QString getCurrentGroupId() const; // 获取当前加载的歌单ID
    bool shuffleMusicList();

    void clearImgCache();
    bool searchMusic(const QString &groupId, const QString &keyword);
    void clearSearch();
    bool isSearching() const;

    void sort(int column, Qt::SortOrder order) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QCache<QString, QIcon> m_musicImgCache{1000};

    // 格式化时长（秒 → "mm:ss"）
    QString formatDuration(int seconds) const;
    QPixmap getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius) const;

    bool m_isSearching = false;                  // 是否处于搜索态
    QString m_searchKeyword;                     // 当前搜索关键词
    QVector<MusicItem> m_originalMusicItems;     // 搜索前的原始歌单数据（缓存用）

private:
    QVector<MusicItem> m_musicItems;       // 存储当前歌单的歌曲数据
    QString m_currentGroupId;              // 当前加载的歌单ID
    SqliteManager &m_db = SqliteManager::getInstance(); // 数据库单例
    Qt::SortOrder m_sortOrder{Qt::AscendingOrder}; // 当前排序顺序
    int m_sortColumn{-1};                          // 当前排序列（-1表示未排序）
};

#endif // TM_MUSICLISTVIEWMODEL_H
