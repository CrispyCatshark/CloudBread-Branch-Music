#include "MusicSearchTableModel.h"
#include "SqliteManager.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <QStringList>

MusicSearchTableModel::MusicSearchTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int MusicSearchTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_musicList.size();
}

int MusicSearchTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant MusicSearchTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_musicList.size())
        return QVariant();

    const MusicInfo &music = m_musicList[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case IndexColumn:
            return index.row() + 1;  // 序号从1开始
        case NameColumn:
            return music.name;
        case AutherColumn:
            return music.auther;
        case GroupColumn:
            return music.groupName;
        case DurationColumn:
            return formatDuration(music.duration);
        case CountColumn:
            return music.count;
        default:
            return QVariant();
        }
    }

    return QVariant();
}

QVariant MusicSearchTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case IndexColumn:
            return "序号";
        case NameColumn:
            return "歌名";
        case AutherColumn:
            return "作者";
        case GroupColumn:
            return "歌单";
        case DurationColumn:
            return "时长";
        case CountColumn:
            return "播放次数";
        default:
            return QVariant();
        }
    }
    return QVariant();
}

void MusicSearchTableModel::search(const QString &keyword)
{
    beginResetModel();
    clearData();

    if (keyword.isEmpty()) {
        endResetModel();
        return;
    }

    QString sql = "SELECT md.*, gd.name as group_name "
                  "FROM music_data md "
                  "LEFT JOIN music_group mg ON md.id = mg.music_id "
                  "LEFT JOIN group_data gd ON mg.group_id = gd.id "
                  "WHERE md.name LIKE ? OR md.auther LIKE ? "
                  "GROUP BY md.id "
                  "ORDER BY md.sort_id";

    QVector<QVariant> params;
    params << "%" + keyword + "%" << "%" + keyword + "%";

    QVector<QMap<QString, QVariant>> result = SqliteManager::getInstance().querySql(sql, params);

    for (const auto &row : std::as_const(result)) {
        MusicInfo music;
        music.id = row["id"].toString();
        music.sortId = row["sort_id"].toInt();
        music.imgName = row["img_name"].toString();
        music.name = row["name"].toString();
        music.auther = row["auther"].toString();
        music.duration = row["duration"].toInt();
        music.timestamp = row["timestamp"].toInt();
        music.pitch = row["pitch"].toInt();
        music.count = row["count"].toInt();
        music.filePath = row["file_path"].toString();
        music.groupName = row["group_name"].toString();

        m_musicList.append(music);
    }

    endResetModel();
}

void MusicSearchTableModel::clearData()
{
    m_musicList.clear();
}

QString MusicSearchTableModel::formatDuration(int seconds) const
{
    int minutes = seconds / 60;
    int secs = seconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}

// 实现获取歌曲ID的方法
QString MusicSearchTableModel::getMusicId(int row) const
{
    if (row >= 0 && row < m_musicList.size()) {
        return m_musicList[row].id;  // 返回歌曲ID（不是排序ID）
    }
    return QString();
}
