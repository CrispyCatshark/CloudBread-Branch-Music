#include "Tm_musicListViewModel.h"
#include <QUuid>
#include <QTime>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <qdir.h>
#include <qicon.h>
#include <QRandomGenerator>

Tm_musicListViewModel::Tm_musicListViewModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

// 歌曲数量（行数）
int Tm_musicListViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_musicItems.size();
}

// 列数（固定为ColumnCount）
int Tm_musicListViewModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

// 获取指定单元格、指定角色的数据
QVariant Tm_musicListViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_musicItems.size() || index.column() >= ColumnCount) {
        return QVariant();
    }

    const MusicItem &music = m_musicItems[index.row()];
    switch (role) {
    case Qt::DisplayRole: // 表格默认显示文本
        switch (index.column()) {
        case IndexColumn:
            return music.sortId; // 序号（从1开始）
        case NameColumn:
            return music.name;
        case AutherColumn:
            return music.auther;
        case DurationColumn:
            return formatDuration(music.duration);
        case TimestampColumn:  // 新增：显示格式化后的导入时间
            return QDateTime::fromSecsSinceEpoch(music.timestamp).toString("yyyy-MM-dd");
        case PlayColumn:
            return music.count;
        default:
            return QVariant();
        }
    case Qt::TextAlignmentRole:
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::DecorationRole: // 封面图片（仅歌曲名列显示）
        // if (index.column() == NameColumn && !music.imgName.isEmpty()) {
            // return QIcon(getRoundRectPixmap(QPixmap(music.imgName), QSize(38, 38), 8));
        //     return QIcon(QPixmap(music.imgName));
        // }
        if (index.column() == NameColumn && !music.imgName.isEmpty()) {
            // 1. 查缓存（const 函数中可调用 const 成员函数 contains()）
            if (m_musicImgCache.contains(music.id)) {
                return *m_musicImgCache.object(music.id);
            }

            // 2. 缓存未命中：生成并插入缓存（mutable 允许调用 insert()）
            QPixmap originalPix(music.imgName);
            if (!originalPix.isNull()) {
                QPixmap roundedPix = getRoundRectPixmap(originalPix, QSize(38, 38), 8);
                QIcon *roundedIcon = new QIcon(roundedPix);
                const_cast<Tm_musicListViewModel*>(this)->m_musicImgCache.insert(music.id, roundedIcon);
                return *roundedIcon;
            }
        }
        return QVariant();
    case Qt::UserRole: // 自定义角色：获取歌曲ID（供删除等操作）
        return music.id;
    default:
        return QVariant();
    }
}

// 重写支持排序的标志（新增）
Qt::ItemFlags Tm_musicListViewModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    // 序号列和播放次数列可点击排序
    if (index.column() == IndexColumn || index.column() == PlayColumn) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

void Tm_musicListViewModel::clearImgCache()
{
    m_musicImgCache.clear(); // QCache会自动释放内部存储的QIcon指针，无需手动delete
}

// 表格表头数据（水平表头：列名称）
QVariant Tm_musicListViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case IndexColumn:
            return "#";
        case NameColumn:
            return "歌曲名";
        case AutherColumn:
            return "歌手";
        case DurationColumn:
            return "时长";
        case TimestampColumn:
            return "导入时间";
        case PlayColumn:
            return "播放次数";
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool Tm_musicListViewModel::loadMusicByGroupId(const QString &groupId)
{
    beginResetModel();
    m_musicItems.clear();
    m_currentGroupId = groupId;

    // 新增：切换歌单时清空搜索状态和原始数据缓存
    m_isSearching = false;
    m_searchKeyword.clear();
    m_originalMusicItems.clear();

    // 以下为原代码（不变）
    QString sql = "SELECT m.id, m.sort_id, m.img_name, m.name, m.auther, "
                  "m.duration, m.timestamp, m.pitch, m.count, m.file_path "
                  "FROM music_data m "
                  "INNER JOIN music_group mg ON m.id = mg.music_id "
                  "WHERE mg.group_id = ? "
                  "ORDER BY m.sort_id ASC";
    QVector<QVariant> params = {groupId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    if (m_db.getLastError().isEmpty()) {
        for (const auto &row : std::as_const(result)) {
            MusicItem item;
            item.id = row["id"].toString();
            item.sortId = row["sort_id"].toInt();
            item.imgName = row["img_name"].toString();
            item.name = row["name"].toString();
            item.auther = row["auther"].toString();
            item.duration = row["duration"].toInt();
            item.timestamp = row["timestamp"].toInt();
            item.pitch = row["pitch"].toInt();
            item.count = row["count"].toInt();
            item.filePath = "";
            item.targetPath = row["file_path"].toString();
            m_musicItems.append(item);
        }
        endResetModel();
        return true;
    } else {
        qWarning() << "加载歌单歌曲失败：" << m_db.getLastError();
        endResetModel();
        return false;
    }
}

// 向歌单添加歌曲（需先确保歌曲在music_data中存在）
bool Tm_musicListViewModel::addMusicToGroup(const QString &groupId, const MusicItem &music)
{
    // 1. 检查歌曲是否已在当前歌单中（避免重复添加）
    QString checkSql = "SELECT COUNT(*) AS count FROM music_group "
                       "WHERE group_id = ? AND music_id = ?";
    QVector<QVariant> checkParams = {groupId, music.id};
    QVector<QMap<QString, QVariant>> checkResult = m_db.querySql(checkSql, checkParams);

    if (!checkResult.isEmpty() && checkResult.first()["count"].toInt() > 0) {
        qWarning() << "歌曲已在歌单中：" << music.name;
        return false;
    }

    // 2. 开启事务（确保歌曲数据和关联关系同时插入）
    if (!m_db.beginTransaction()) {
        qWarning() << "开启事务失败：" << m_db.getLastError();
        return false;
    }

    // 3. 先检查music_data中是否已有该歌曲，无则插入
    QString checkMusicSql = "SELECT id FROM music_data WHERE id = ?";
    QVector<QVariant> checkMusicParams = {music.id};
    QVector<QMap<QString, QVariant>> musicResult = m_db.querySql(checkMusicSql, checkMusicParams);

    if (musicResult.isEmpty()) {
        // 插入新歌曲到music_data：新增file_path参数，忽略filePath
        QString insertMusicSql = "INSERT INTO music_data ("
                                 "id, sort_id, img_name, name, auther, "
                                 "duration, timestamp, pitch, count, file_path) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        QVector<QVariant> insertMusicParams = {
            music.id, music.sortId, music.imgName, music.name, music.auther,
            music.duration, music.timestamp, music.pitch, music.count,
            music.targetPath  // 插入目标路径（数据库唯一存储的路径）
        };
        if (!m_db.executeSql(insertMusicSql, insertMusicParams)) {
            m_db.rollbackTransaction();
            qWarning() << "插入歌曲数据失败：" << m_db.getLastError();
            return false;
        }
    }

    // 4. 插入关联关系到music_group
    QString insertRelSql = "INSERT INTO music_group (music_id, group_id) VALUES (?, ?)";
    QVector<QVariant> insertRelParams = {music.id, groupId};
    if (m_db.executeSql(insertRelSql, insertRelParams)) {
        m_db.commitTransaction();

        // 5. 如果当前加载的是目标歌单，更新模型
        if (groupId == m_currentGroupId) {
            beginInsertRows(QModelIndex(), m_musicItems.size(), m_musicItems.size());
            m_musicItems.append(music);
            endInsertRows();
        }
        return true;
    } else {
        m_db.rollbackTransaction();
        qWarning() << "插入歌单关联失败：" << m_db.getLastError();
        return false;
    }
}

// 从歌单移除歌曲（删除关联+条件删除歌曲数据、音频文件、封面文件）
bool Tm_musicListViewModel::removeMusicFromGroup(const QString &groupId, const QString &musicId)
{
    // 1. 先获取歌曲完整信息（包含音频路径和封面路径）
    MusicItem music = getMusicById(musicId);
    if (music.id.isEmpty()) {
        qWarning() << "歌曲不存在：" << musicId;
        return false;
    }

    // 2. 开启事务确保操作原子性（关联、数据、文件删除一致性）
    if (!m_db.beginTransaction()) {
        qWarning() << "开启事务失败：" << m_db.getLastError();
        return false;
    }

    // 3. 删除当前歌单与歌曲的关联关系
    QString deleteRelSql = "DELETE FROM music_group WHERE group_id = ? AND music_id = ?";
    QVector<QVariant> relParams = {groupId, musicId};
    if (!m_db.executeSql(deleteRelSql, relParams)) {
        m_db.rollbackTransaction();
        qWarning() << "删除歌单关联失败：" << m_db.getLastError();
        return false;
    }

    // 4. 检查歌曲是否存在于其他歌单
    QString checkSql = "SELECT COUNT(*) AS count FROM music_group WHERE music_id = ?";
    QVector<QVariant> checkParams = {musicId};
    QVector<QMap<QString, QVariant>> checkResult = m_db.querySql(checkSql, checkParams);

    bool isMusicInOtherGroups = !(checkResult.isEmpty() || checkResult.first()["count"].toInt() == 0);

    // 5. 仅当歌曲不在任何歌单时，删除音频文件、封面文件和数据库记录
    if (!isMusicInOtherGroups) {
        // 5.1 删除音频文件
        if (!music.targetPath.isEmpty()) {
            QFile audioFile(music.targetPath);
            if (audioFile.exists() && !audioFile.remove()) {
                qWarning() << "删除音频文件失败：" << audioFile.errorString() << "路径：" << music.targetPath;
                // 此处不中断，继续尝试删除封面和数据库记录
            } else if (audioFile.exists()) {
                qDebug() << "成功删除音频文件：" << music.targetPath;
            }
        }

        // 5.2 删除封面图片文件（核心新增逻辑）
        if (!music.imgName.isEmpty() && !music.imgName.startsWith(":/")) {
            QFile coverFile(music.imgName);
            if (coverFile.exists() && !coverFile.remove()) {
                qWarning() << "删除封面文件失败：" << coverFile.errorString() << "路径：" << music.imgName;
            } else if (coverFile.exists()) {
                qDebug() << "成功删除封面文件：" << music.imgName;
            }
        }

        // 5.3 删除 music_data 中的歌曲记录
        QString deleteMusicSql = "DELETE FROM music_data WHERE id = ?";
        QVector<QVariant> musicParams = {musicId};
        if (!m_db.executeSql(deleteMusicSql, musicParams)) {
            m_db.rollbackTransaction();
            qWarning() << "删除歌曲数据失败：" << m_db.getLastError();
            return false;
        }
    }

    // 6. 提交事务
    if (!m_db.commitTransaction()) {
        qWarning() << "提交事务失败：" << m_db.getLastError();
        return false;
    }

    // 7. 更新模型数据并清理封面缓存
    if (groupId == m_currentGroupId) {
        for (int i = 0; i < m_musicItems.size(); ++i) {
            if (m_musicItems[i].id == musicId) {
                beginRemoveRows(QModelIndex(), i, i);
                m_musicItems.removeAt(i);
                // 清理封面缓存（即使封面文件删除失败，也移除内存缓存）
                m_musicImgCache.remove(musicId);
                endRemoveRows();
                break;
            }
        }
    }

    return true;
}

// 更新歌曲信息（如播放次数、升降调、封面等）
bool Tm_musicListViewModel::updateMusic(const MusicItem &music)
{
    // 1. 执行更新SQL：新增file_path字段更新
    QString sql = "UPDATE music_data SET "
                  "sort_id = ?, img_name = ?, name = ?, auther = ?, "
                  "duration = ?, timestamp = ?, pitch = ?, count = ?, file_path = ? "
                  "WHERE id = ?";
    QVector<QVariant> params = {
        music.sortId, music.imgName, music.name, music.auther,
        music.duration, music.timestamp, music.pitch, music.count,
        music.targetPath,  // 新增：更新目标路径
        music.id
    };

    if (m_db.executeSql(sql, params)) {
        // 2. 如果歌曲在当前加载的歌单中，更新模型
        for (int i = 0; i < m_musicItems.size(); ++i) {
            if (m_musicItems[i].id == music.id) {
                MusicItem updatedItem = music;
                updatedItem.filePath = ""; // 确保原始路径为空
                m_musicItems[i] = updatedItem;
                // 通知视图：当前行数据已更新
                emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
                return true;
            }
        }
        return true;
    } else {
        qWarning() << "更新歌曲信息失败：" << m_db.getLastError();
        return false;
    }
}

MusicItem Tm_musicListViewModel::getMusicById(const QString &musicId) const
{
    for (const auto &music : m_musicItems) {
        if (music.id == musicId) {
            return music;
        }
    }

    // SQL查询：补充 m.file_path 字段
    QString sql = "SELECT id, sort_id, img_name, name, auther, "
                  "duration, timestamp, pitch, count, file_path "  // 新增 file_path
                  "FROM music_data WHERE id = ?";
    QVector<QVariant> params = {musicId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    if (!result.isEmpty() && m_db.getLastError().isEmpty()) {
        MusicItem item;
        const auto &row = result.first();
        item.id = row["id"].toString();
        item.sortId = row["sort_id"].toInt();
        item.imgName = row["img_name"].toString();
        item.name = row["name"].toString();
        item.auther = row["auther"].toString();
        item.duration = row["duration"].toInt();
        item.timestamp = row["timestamp"].toInt();  // timestamp 正常读取
        item.pitch = row["pitch"].toInt();
        item.count = row["count"].toInt();
        item.filePath = "";
        item.targetPath = row["file_path"].toString(); //  now 可正确读取
        return item;
    }
    return MusicItem();
}

// 获取当前加载的歌单ID
QString Tm_musicListViewModel::getCurrentGroupId() const
{
    return m_currentGroupId;
}

// 格式化时长（秒 → "mm:ss" 格式）
QString Tm_musicListViewModel::formatDuration(int seconds) const
{
    int minutes = seconds / 60;
    int secs = seconds % 60;
    // 补零确保格式统一（如 1分5秒 → "01:05"）
    return QString("%1:%2").arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QPixmap Tm_musicListViewModel::getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius) const
{
    //不处理空数据或者错误数据
    if (srcPixMap.isNull()) {
        return srcPixMap;
    }

    //获取图片尺寸
    int imageWidth = size.width();
    int imageHeight = size.height();

    //处理大尺寸的图片,保证图片显示区域完整
    QPixmap newPixMap = srcPixMap.scaled(imageWidth, (imageHeight == 0 ? imageWidth : imageHeight),Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

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

bool Tm_musicListViewModel::shuffleMusicList()
{
    // 1. 校验：当前是否已加载歌单
    if (m_currentGroupId.isEmpty()) {
        qWarning() << "未加载任何歌单，无法打乱顺序";
        return false;
    }
    if (m_musicItems.isEmpty()) {
        qDebug() << "歌单为空，无需打乱";
        return true;
    }

    // 2. 开启数据库事务（确保内存与数据库排序一致）
    if (!m_db.beginTransaction()) {
        qWarning() << "开启事务失败：" << m_db.getLastError();
        return false;
    }

    // 3. 生成打乱后的sort_id映射（核心逻辑）
    // 3.1 提取当前歌单所有歌曲ID
    QVector<QString> musicIds;
    for (const auto& item : std::as_const(m_musicItems)) {
        musicIds.append(item.id);
    }
    // 3.2 随机打乱ID顺序（使用QRandomGenerator确保随机性，Qt 5.10+支持）
    unsigned seed = std::chrono::system_clock::now ().time_since_epoch ().count ();
    std::shuffle(musicIds.begin(),musicIds.end(),std::default_random_engine(seed));

    // 4. 批量更新数据库中的sort_id
    bool updateSuccess = true;
    for (int i = 0; i < musicIds.size(); ++i) {
        const QString& musicId = musicIds[i];
        int newSortId = i + 1; // 新序号从1开始连续

        // 执行SQL更新
        QString sql = "UPDATE music_data SET sort_id = ? WHERE id = ?";
        QVector<QVariant> params = {newSortId, musicId};
        if (!m_db.executeSql(sql, params)) {
            qWarning() << "更新歌曲sort_id失败（ID:" << musicId << "）：" << m_db.getLastError();
            updateSuccess = false;
            break;
        }
    }

    // 5. 事务提交/回滚
    if (!updateSuccess) {
        m_db.rollbackTransaction();
        return false;
    }
    if (!m_db.commitTransaction()) {
        qWarning() << "提交事务失败：" << m_db.getLastError();
        return false;
    }

    // 6. 更新内存模型（同步新的sort_id并排序）
    beginResetModel(); // 通知视图：模型数据将全量重置
    for (int i = 0; i < musicIds.size(); ++i) {
        const QString& musicId = musicIds[i];
        // 找到内存中对应的歌曲并更新sort_id
        for (auto& item : m_musicItems) {
            if (item.id == musicId) {
                item.sortId = i + 1;
                break;
            }
        }
    }
    // 按新的sort_id重新排序内存列表（确保模型顺序与序号一致）
    std::sort(m_musicItems.begin(), m_musicItems.end(), [](const MusicItem& a, const MusicItem& b) {
        return a.sortId < b.sortId;
    });
    endResetModel(); // 通知视图：模型已更新，刷新显示

    qDebug() << "歌单（ID:" << m_currentGroupId << "）打乱顺序成功，共" << m_musicItems.size() << "首歌曲";
    return true;
}

// 设置排序：指定列和顺序
void Tm_musicListViewModel::sort(int column, Qt::SortOrder order)
{
    // 仅支持序号列(0)和播放次数列(5)排序
    if (column != IndexColumn && column != PlayColumn) {
        return;
    }

    beginResetModel();
    m_sortColumn = column;
    m_sortOrder = order;

    // 根据指定列排序
    if (column == IndexColumn) {
        // 按序号(sortId)排序
        std::sort(m_musicItems.begin(), m_musicItems.end(), [this](const MusicItem& a, const MusicItem& b) {
            return m_sortOrder == Qt::AscendingOrder ? (a.sortId < b.sortId) : (a.sortId > b.sortId);
        });
    } else if (column == PlayColumn) {
        // 按播放次数(count)排序
        std::sort(m_musicItems.begin(), m_musicItems.end(), [this](const MusicItem& a, const MusicItem& b) {
            return m_sortOrder == Qt::AscendingOrder ? (a.count < b.count) : (a.count > b.count);
        });
    }

    endResetModel();
}

/**
 * @brief 通过歌曲ID更新音乐封面图片路径
 * @param musicId 目标歌曲的唯一标识ID
 * @param newImgName 新的封面图片路径（本地路径或资源路径）
 * @return 成功返回true，失败返回false
 */
bool Tm_musicListViewModel::updateMusicCover(const QString &musicId, const QString &newImgName)
{
    // 1. 校验参数有效性
    if (musicId.isEmpty() || newImgName.isEmpty()) {
        qWarning() << "更新封面失败：歌曲ID或新封面路径为空";
        return false;
    }

    // 2. 校验歌曲是否存在（避免更新不存在的记录）
    MusicItem existingMusic = getMusicById(musicId);
    if (existingMusic.id.isEmpty()) {
        qWarning() << "更新封面失败：歌曲不存在（ID:" << musicId << "）";
        return false;
    }

    // 3. 执行数据库更新（仅更新img_name字段）
    QString updateSql = "UPDATE music_data SET img_name = ? WHERE id = ?";
    QVector<QVariant> params = {newImgName, musicId};

    if (!m_db.executeSql(updateSql, params)) {
        qWarning() << "数据库更新封面失败（ID:" << musicId << "）：" << m_db.getLastError();
        return false;
    }

    // 4. 同步更新内存模型（若歌曲在当前加载的歌单中）
    for (int i = 0; i < m_musicItems.size(); ++i) {
        if (m_musicItems[i].id == musicId) {
            // 更新内存中的封面路径
            m_musicItems[i].imgName = newImgName;

            // 5. 清理旧封面的缓存（避免显示旧图）
            m_musicImgCache.remove(musicId);

            // 6. 通知视图：当前行的封面数据已更新（触发重新加载）
            emit dataChanged(index(i, NameColumn), index(i, NameColumn), {Qt::DecorationRole});
            break;
        }
    }

    qDebug() << "歌曲封面更新成功（ID:" << musicId << "），新封面路径：" << newImgName;
    return true;
}

/**
 * @brief 歌单内模糊搜索（按歌曲名/歌手名）
 * @param groupId 当前歌单ID（确保搜索范围仅限当前歌单）
 * @param keyword 搜索关键词（支持空字符串，空则返回原歌单）
 * @return 搜索成功返回true，失败返回false
 */
bool Tm_musicListViewModel::searchMusic(const QString &groupId, const QString &keyword)
{
    beginResetModel(); // 通知视图：模型数据将全量更新

    // 1. 空关键词：退出搜索态，恢复原歌单数据
    if (keyword.trimmed().isEmpty()) {
        m_isSearching = false;
        m_searchKeyword.clear();
        m_musicItems = m_originalMusicItems; // 恢复原始数据
        endResetModel();
        return true;
    }

    // 2. 非空关键词：进入搜索态，执行模糊查询
    m_isSearching = true;
    m_searchKeyword = keyword.trimmed();
    m_originalMusicItems = m_musicItems; // 缓存当前原始数据（用于后续恢复）

    // 3. SQL模糊查询：使用 LIKE 匹配歌曲名或歌手名（不区分大小写）
    // 注：不同数据库 LIKE 语法可能有差异，此处适配 SQLite（% 通配任意字符）
    QString sql = "SELECT m.id, m.sort_id, m.img_name, m.name, m.auther, "
                  "m.duration, m.timestamp, m.pitch, m.count, m.file_path "
                  "FROM music_data m "
                  "INNER JOIN music_group mg ON m.id = mg.music_id "
                  "WHERE mg.group_id = ? "
                  "AND (m.name LIKE ? OR m.auther LIKE ?) " // 双维度匹配
                  "ORDER BY m.sort_id ASC";

    // 构建模糊匹配参数（% 包裹关键词，支持关键词在任意位置）
    QString likeParam = "%" + m_searchKeyword + "%";
    QVector<QVariant> params = {groupId, likeParam, likeParam};

    // 执行查询
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);
    if (m_db.getLastError().isEmpty()) {
        m_musicItems.clear(); // 清空旧数据，存储搜索结果
        for (const auto &row : std::as_const(result)) {
            MusicItem item;
            item.id = row["id"].toString();
            item.sortId = row["sort_id"].toInt();
            item.imgName = row["img_name"].toString();
            item.name = row["name"].toString();
            item.auther = row["auther"].toString();
            item.duration = row["duration"].toInt();
            item.timestamp = row["timestamp"].toInt();
            item.pitch = row["pitch"].toInt();
            item.count = row["count"].toInt();
            item.filePath = "";
            item.targetPath = row["file_path"].toString();
            m_musicItems.append(item);
        }
        endResetModel();
        return true;
    } else {
        qWarning() << "模糊搜索失败：" << m_db.getLastError();
        // 搜索失败时恢复原始数据
        m_musicItems = m_originalMusicItems;
        endResetModel();
        return false;
    }
}

/**
 * @brief 清空搜索状态，恢复原歌单数据
 */
void Tm_musicListViewModel::clearSearch()
{
    if (m_isSearching) { // 仅在搜索态时执行恢复
        beginResetModel();
        m_isSearching = false;
        m_searchKeyword.clear();
        m_musicItems = m_originalMusicItems;
        endResetModel();
    }
}

/**
 * @brief 判断当前是否处于搜索态
 * @return 搜索态返回true，默认态返回false
 */
bool Tm_musicListViewModel::isSearching() const
{
    return m_isSearching;
}
