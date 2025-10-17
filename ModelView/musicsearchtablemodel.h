#ifndef MUSICSEARCHTABLEMODEL_H
#define MUSICSEARCHTABLEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QMap>
#include <QVariant>

// 音乐数据结构
struct MusicInfo {
    QString id;             // 音乐ID
    int sortId;             // 排序ID
    QString imgName;        // 图片名称
    QString name;           // 歌名
    QString auther;         // 作者
    int duration;           // 时长(秒)
    int timestamp;          // 时间戳
    int pitch;              // 音调
    int count;              // 播放次数
    QString filePath;       // 文件路径
    QString groupName;      // 歌单名称
};

class MusicSearchTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // 列索引枚举
    enum ColumnIndex {
        IndexColumn = 0,     // 序号
        NameColumn,          // 歌名
        AutherColumn,        // 作者
        GroupColumn,         // 歌单
        DurationColumn,      // 时长
        CountColumn,         // 播放次数
        ColumnCount          // 列总数
    };

    explicit MusicSearchTableModel(QObject *parent = nullptr);

    // 重写QAbstractTableModel方法
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 搜索相关方法
    void search(const QString &keyword);
    void clearData();

    // 获取指定行的歌曲ID
    QString getMusicId(int row) const;

private:
    // 格式化时长(秒 -> mm:ss)
    QString formatDuration(int seconds) const;

    QVector<MusicInfo> m_musicList;  // 音乐数据列表
};

#endif // MUSICSEARCHTABLEMODEL_H
