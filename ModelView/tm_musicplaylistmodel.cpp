#include "Tm_musicPlayListModel.h"
#include <QUuid>
#include <QDateTime>
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <QBrush>
#include <qicon.h>
#include <qrandom.h>

Tm_musicPlayListModel* Tm_musicPlayListModel::m_instance = nullptr;

// 全局获取SQLite单例
#define DB_INSTANCE SqliteManager::getInstance()

Tm_musicPlayListModel::Tm_musicPlayListModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_db(DB_INSTANCE)
{
    initPlayListTable(); // 初始化播放列表表格
}

Tm_musicPlayListModel::~Tm_musicPlayListModel()
{
    clearImgCache();
}

Tm_musicPlayListModel* Tm_musicPlayListModel::getInstance(QObject *parent)
{
    if (m_instance == nullptr) {
        // 确保仅创建一个实例，parent 仅在首次创建时生效
        m_instance = new Tm_musicPlayListModel(parent);
    }
    return m_instance;
}

// -------------------------- 模型核心接口（QAbstractTableModel 重写）--------------------------
int Tm_musicPlayListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_playItems.size();
}

int Tm_musicPlayListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount; // 固定3列
}

QVariant Tm_musicPlayListModel::data(const QModelIndex &index, int role) const
{
    // 索引有效性检查（保持不变）
    if (!index.isValid()
        || index.row() >= m_playItems.size()
        || index.column() >= ColumnCount) {
        return QVariant();
    }

    const PlayListItem &playItem = m_playItems[index.row()];
    const MusicDetail music = getMusicDetail(playItem.musicId);

    switch (role) {
    // 表格显示文本（调整列索引：Name/Auther/Duration从1/2/3开始）
    case Qt::DisplayRole:
        switch (index.column()) {
        case NameColumn: return music.name;       // 原0→1
        case AutherColumn: return music.auther;   // 原1→2
        case DurationColumn: return formatDuration(music.duration); // 原2→3
        default: return QVariant(); // 状态列无文本
        }

    // 文本对齐方式（状态列居中，其余保持左对齐）
    case Qt::TextAlignmentRole:
        if (index.column() == StatusColumn) {
            return QVariant(Qt::AlignCenter | Qt::AlignVCenter); // 状态图标居中
        } else {
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);  // 其他列左对齐
        }

    // 封面图标+状态图标（核心修改）
    case Qt::DecorationRole:
        // 新增：状态列（根据播放状态返回对应图标）
        if (index.column() == StatusColumn) {
            QIcon icon;
            // 1. 根据状态选择原始图标
            if (playItem.isPlaying) {
                icon = QIcon(":/cloudbread/src/playing.png");
            } else if (playItem.isPlayed) {
                icon = QIcon(":/cloudbread/src/played.png");
            } else {
                icon = QIcon(":/cloudbread/src/inlist.png");
            }
            const QSize iconSize(30, 17);
            QPixmap scaledPixmap = icon.pixmap(iconSize, Qt::SmoothTransformation);
            return scaledPixmap;
        }
        // 原有：歌名列的封面图标（列索引从0→1）
        else if (index.column() == NameColumn && !music.imgName.isEmpty()) {
            // 优先查缓存
            if (m_imgCache.contains(music.id)) {
                return *m_imgCache.object(music.id);
            }
            // 缓存未命中：生成圆角图标并缓存
            QPixmap original(music.imgName);
            if (!original.isNull()) {
                QPixmap rounded = getRoundRectPixmap(original, QSize(38, 38), 8);
                QIcon *icon = new QIcon(rounded);
                const_cast<Tm_musicPlayListModel*>(this)->m_imgCache.insert(music.id, icon);
                return *icon;
            }
        }
        return QVariant();

    // 自定义角色：获取播放列表项ID（保持不变）
    case Qt::UserRole:
        return playItem.id;

    default:
        return QVariant();
    }
}

// -------------------------- 播放列表核心功能 --------------------------
/**
 * @brief 加载播放列表（从 play_list 表）
 * @return 加载成功返回true
 */
bool Tm_musicPlayListModel::loadPlayList()
{
    beginResetModel();
    m_playItems.clear();
    m_lastError.clear();

    // 查询播放列表（按sort_id排序）
    QString sql = "SELECT id, sort_id, music_id, playing, played "
                  "FROM play_list "
                  "ORDER BY sort_id ASC";
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql);

    if (!m_db.getLastError().isEmpty()) {
        m_lastError = "加载播放列表失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        endResetModel();
        return false;
    }

    // 解析结果
    for (const auto &row : std::as_const(result)) {
        PlayListItem item;
        item.id = row["id"].toString();
        item.sortId = row["sort_id"].toInt();
        item.musicId = row["music_id"].toString();
        item.isPlaying = row["playing"].toInt() == 1;
        item.isPlayed = row["played"].toInt() == 1;
        m_playItems.append(item);
    }

    endResetModel();
    qDebug() << "播放列表加载成功，共" << m_playItems.size() << "首歌曲";
    return true;
}

/**
 * @brief 播放指定歌单（清空当前列表并加载歌单内所有音乐）
 * @param groupId 目标歌单ID（关联group_data表的id）
 * @return 操作成功返回true
 */
bool Tm_musicPlayListModel::playGroupMusic(const QString &groupId)
{
    m_lastError.clear();

    // 1. 校验歌单ID合法性
    if (groupId.isEmpty()) {
        m_lastError = "歌单ID不能为空";
        qWarning() << m_lastError;
        return false;
    }

    // 2. 查询歌单对应的所有音乐ID（关联music_data表，按music_data.sort_id升序排序）
    // 核心修改：JOIN music_data 表获取sort_id，按sort_id排序（而非原music_id）
    QString queryMusicSql = "SELECT mg.music_id, md.sort_id "  // 同时查询music_id和对应的sort_id
                            "FROM music_group mg "
                            "JOIN music_data md ON mg.music_id = md.id "  // 关联音乐数据表
                            "WHERE mg.group_id = ? "
                            "ORDER BY md.sort_id ASC";  // 按music_data的sort_id升序，保证顺序正确
    QVector<QVariant> queryParams = {groupId};
    QVector<QMap<QString, QVariant>> musicResult = m_db.querySql(queryMusicSql, queryParams);

    if (!m_db.getLastError().isEmpty()) {
        m_lastError = "查询歌单音乐失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 3. 校验歌单是否有音乐
    if (musicResult.isEmpty()) {
        m_lastError = "歌单无音乐数据（group_id=" + groupId + "）";
        qWarning() << m_lastError;
        return false;
    }

    // 4. 开启事务（清空旧列表+批量添加新音乐+设置播放状态，确保原子性）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 5. 步骤1：清空当前播放列表
    QString clearSql = "DELETE FROM play_list;";
    if (!m_db.executeSql(clearSql)) {
        m_db.rollbackTransaction();
        m_lastError = "清空旧播放列表失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 6. 步骤2：批量添加歌单音乐到播放列表（按music_data.sort_id顺序）
    QString insertSql = "INSERT INTO play_list (id, sort_id, music_id, playing, played) VALUES (?, ?, ?, ?, ?)";
    int playListSortId = 1;  // 播放列表的sort_id（从1开始递增，与music_data.sort_id顺序一致）
    QString musicIDfirst;    // 记录第一首音乐ID（用于后续设置播放状态）

    for (const auto &row : std::as_const(musicResult)) {
        QString musicId = row["music_id"].toString();
        // 记录第一首音乐ID（按music_data.sort_id排序后的第一首）
        if (playListSortId == 1) {
            musicIDfirst = musicId;
        }

        // 构造插入参数（播放列表sort_id按顺序递增，第一首设为playing=1）
        QString playItemId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVector<QVariant> params = {
            playItemId,
            playListSortId,                  // 播放列表sort_id（与查询顺序一致）
            musicId,
            (playListSortId == 1) ? 1 : 0,   // 第一首设为正在播放
            0                                // 初始均为未播放状态
        };

        // 执行插入操作
        if (!m_db.executeSql(insertSql, params)) {
            m_db.rollbackTransaction();
            m_lastError = "添加音乐失败（music_id=" + musicId + "）：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }

        playListSortId++;  // 播放列表sort_id递增（保证顺序与查询结果一致）
    }

    // 7. 提交事务
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 8. 刷新模型数据（清空缓存+加载新列表+通知视图更新）
    clearImgCache();
    loadPlayList();
    emit dataChanged(index(0, 0), index(rowCount()-1, ColumnCount-1));
    qDebug() << "歌单播放成功，共加载" << musicResult.size() << "首音乐（group_id=" << groupId
             << "），顺序遵循music_data.sort_id";

    // 9. 同步数据库播放状态（确保第一首为正在播放）
    if (setMusicAsPlaying(musicIDfirst)) {
        qDebug() << "歌单首曲播放状态设置成功（music_id=" << musicIDfirst << "）";
    } else {
        qWarning() << "歌单首曲播放状态设置失败：" << getLastError();
    }

    return true;
}

/**
 * @brief 当前位置后添加一首（下一首播放）
 * @param musicId 待添加的音乐ID
 * @return 添加成功返回true
 */
bool Tm_musicPlayListModel::addMusicToCurrentPos(const QString &musicId)
{
    m_lastError.clear();

    // 1. 校验音乐是否存在
    if (getMusicDetail(musicId).id.isEmpty()) {
        m_lastError = "音乐不存在：" + musicId;
        qWarning() << m_lastError;
        return false;
    }

    // 2. 查找当前播放歌曲的sort_id（默认追加到开头）
    int currentSortId = 0;
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.isPlaying) {
            currentSortId = item.sortId;
            break;
        }
    }

    // 3. 开启事务（更新排序+插入新项）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 4. 调整后续歌曲的sort_id（+1）
    QString updateSql = "UPDATE play_list SET sort_id = sort_id + 1 WHERE sort_id > ?";
    QVector<QVariant> updateParams = {currentSortId};
    if (!m_db.executeSql(updateSql, updateParams)) {
        m_db.rollbackTransaction();
        m_lastError = "调整排序失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 5. 插入新播放项（sort_id = currentSortId + 1，默认未播放）
    PlayListItem newItem;
    newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces); // 生成char64 ID
    newItem.sortId = currentSortId + 1;
    newItem.musicId = musicId;
    newItem.isPlaying = false;
    newItem.isPlayed = false;

    QString insertSql = "INSERT INTO play_list (id, sort_id, music_id, playing, played) "
                        "VALUES (?, ?, ?, ?, ?)";
    QVector<QVariant> insertParams = {
        newItem.id,
        newItem.sortId,
        newItem.musicId,
        0, // playing默认0
        0  // played默认0
    };
    if (!m_db.executeSql(insertSql, insertParams)) {
        m_db.rollbackTransaction();
        m_lastError = "插入播放项失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 6. 提交事务并更新模型
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 新增：若添加前列表为空，触发播放新添加的音乐（sortId=1）
    bool wasEmptyBeforeAdd = (m_playItems.isEmpty());

    // 7. 刷新模型
    loadPlayList();

    // 修改：先更新数据库，再触发播放信号
    if (wasEmptyBeforeAdd) {
        if (setMusicAsPlaying(musicId)) {
            // 新增：更新新增音乐的播放次数
            if (updateMusicPlayCount(musicId, false)) {
                emit playMusic(musicId);
            } else {
                qWarning() << "添加音乐播放次数更新失败，未触发播放：" << getLastError();
            }
        } else {
            qWarning() << "添加音乐播放状态设置失败：" << getLastError();
        }
    }

    return true;
}

/**
 * @brief 结尾添加一首
 * @param musicId 待添加的音乐ID
 * @return 添加成功返回true
 */
bool Tm_musicPlayListModel::addMusicToEnd(const QString &musicId)
{
    m_lastError.clear();

    // 1. 校验音乐是否存在
    if (getMusicDetail(musicId).id.isEmpty()) {
        m_lastError = "音乐不存在：" + musicId;
        qWarning() << m_lastError;
        return false;
    }

    // 2. 计算最大sort_id（新项sort_id = 最大+1，若无则为1）
    int maxSortId = 0;
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.sortId > maxSortId) {
            maxSortId = item.sortId;
        }
    }

    // 3. 插入新播放项
    PlayListItem newItem;
    newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    newItem.sortId = maxSortId + 1;
    newItem.musicId = musicId;
    newItem.isPlaying = false;
    newItem.isPlayed = false;

    QString insertSql = "INSERT INTO play_list (id, sort_id, music_id, playing, played) "
                        "VALUES (?, ?, ?, ?, ?)";
    QVector<QVariant> insertParams = {
        newItem.id,
        newItem.sortId,
        newItem.musicId,
        0,
        0
    };
    if (!m_db.executeSql(insertSql, insertParams)) {
        m_lastError = "插入播放项失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 新增：若添加前列表为空，触发播放新添加的音乐（sortId=1）
    bool wasEmptyBeforeAdd = (m_playItems.isEmpty());

    // 7. 刷新模型
    loadPlayList();

    // 修改：先更新数据库，再触发播放信号
    if (wasEmptyBeforeAdd) {
        if (setMusicAsPlaying(musicId)) {
            // 新增：更新新增音乐的播放次数
            if (updateMusicPlayCount(musicId, false)) {
                emit playMusic(musicId);
            } else {
                qWarning() << "添加音乐播放次数更新失败，未触发播放：" << getLastError();
            }
        } else {
            qWarning() << "添加音乐播放状态设置失败：" << getLastError();
        }
    }

    return true;
}

/**
 * @brief 切换到指定sortId的歌曲
 * @param targetSortId 目标歌曲的sort_id
 * @return 切换成功返回true
 */
bool Tm_musicPlayListModel::switchToMusic(int targetSortId)
{
    m_lastError.clear();

    // 1. 校验目标sort_id是否存在
    bool targetExists = false;
    QString targetMusicId; // 新增：存储目标歌曲的musicId，用于触发播放信号
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.sortId == targetSortId) {
            targetExists = true;
            targetMusicId = item.musicId; // 提前获取目标音乐ID
            break;
        }
    }
    if (!targetExists) {
        m_lastError = "目标歌曲不存在（sort_id=" + QString::number(targetSortId) + "）";
        qWarning() << m_lastError;
        return false;
    }

    // 2. 开启事务（重置旧播放状态+设置新播放状态）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 3. 重置所有歌曲的playing状态（设为0），并将旧播放歌曲标记为played（1）
    QString resetSql = "UPDATE play_list "
                       "SET playing = 0, "
                       "    played = CASE WHEN playing = 1 THEN 1 ELSE played END";
    if (!m_db.executeSql(resetSql)) {
        m_db.rollbackTransaction();
        m_lastError = "重置播放状态失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 4. 设置目标歌曲为playing=1，played=0
    QString setSql = "UPDATE play_list "
                     "SET playing = 1, played = 0 "
                     "WHERE sort_id = ?";
    QVector<QVariant> setParams = {targetSortId};
    if (!m_db.executeSql(setSql, setParams)) {
        m_db.rollbackTransaction();
        m_lastError = "设置目标播放状态失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    if (!updateMusicPlayCount(targetMusicId, true)) {
        m_db.rollbackTransaction(); // 失败回滚整个事务
        return false;
    }

    // 5. 提交事务并刷新模型
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 6. 刷新模型
    loadPlayList();
    emit dataChanged(index(0, 0), index(rowCount()-1, ColumnCount-1)); // 通知视图更新

    // 新增：触发播放信号（手动切歌后播放目标歌曲）
    emit playMusic(targetMusicId);
    qDebug() << "手动切歌成功，触发播放信号（musicId=" << targetMusicId << "）";

    return true;
}

/**
 * @brief 标记当前歌曲为已播放，切换下一首
 * @return 操作成功返回true
 */
bool Tm_musicPlayListModel::markAsPlayed()
{
    m_lastError.clear();

    // 1. 查找当前播放歌曲
    int currentSortId = -1;
    QString currentMusicId;
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.isPlaying) {
            currentSortId = item.sortId;
            currentMusicId = item.musicId;
            break;
        }
    }
    if (currentSortId == -1) {
        m_lastError = "无当前播放歌曲";
        qWarning() << m_lastError;
        return false;
    }

    // 2. 查找下一首歌曲的sort_id和musicId（当前sort_id+1）
    int nextSortId = -1;
    QString nextMusicId;
    int maxSortId = 0; // 新增：获取列表最大sort_id（判断是否为最后一首）
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.sortId == currentSortId + 1) {
            nextSortId = item.sortId;
            nextMusicId = item.musicId;
        }
        if (item.sortId > maxSortId) {
            maxSortId = item.sortId;
        }
    }

    // 3. 开启事务（原子操作：标记已播放+切换下一首/重置+更新播放次数）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 4. 步骤1：将当前歌曲标记为 played=1，playing=0
    QString markCurrentSql = "UPDATE play_list "
                             "SET played = 1, playing = 0 "
                             "WHERE sort_id = ?";
    QVector<QVariant> markCurrentParams = {currentSortId};
    if (!m_db.executeSql(markCurrentSql, markCurrentParams)) {
        m_db.rollbackTransaction();
        m_lastError = "标记当前歌曲为已播放失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 5. 步骤2：更新当前歌曲的播放次数（music_data表 count+1）【修复原逻辑错误：原用nextSortId，应改为currentMusicId对应的条件】
    QString updateCountSql = "UPDATE music_data "
                             "SET count = count + 1 "
                             "WHERE id = ?"; // 原错误：WHERE sort_id = ?
    QVector<QVariant> updateCountParams = {currentMusicId}; // 原错误：nextSortId
    if (!m_db.executeSql(updateCountSql, updateCountParams)) {
        m_db.rollbackTransaction();
        m_lastError = "更新播放次数失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 6. 步骤3：处理下一首/重置逻辑
    bool hasNext = false;
    if (nextSortId != -1) {
        // 存在下一首：设置其为播放状态
        QString setNextSql = "UPDATE play_list "
                             "SET playing = 1, played = 0 "
                             "WHERE sort_id = ?";
        QVector<QVariant> setNextParams = {nextSortId};
        if (!m_db.executeSql(setNextSql, setNextParams)) {
            m_db.rollbackTransaction();
            m_lastError = "切换下一首失败：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }
        hasNext = true;
    } else {
        // 新增：播放到最后一首，重置所有状态并切换到第一首
        qDebug() << "已播放到列表末尾，重置状态并从头开始";

        // 6.1 重置所有歌曲的played=0、playing=0
        QString resetAllSql = "UPDATE play_list "
                              "SET played = 0, playing = 0";
        if (!m_db.executeSql(resetAllSql)) {
            m_db.rollbackTransaction();
            m_lastError = "重置所有播放状态失败：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }

        // 6.2 设置第一首为playing=1（sort_id=1）
        nextSortId = 1;
        QString setFirstSql = "UPDATE play_list "
                              "SET playing = 1, played = 0 "
                              "WHERE sort_id = ?";
        QVector<QVariant> setFirstParams = {nextSortId};
        if (!m_db.executeSql(setFirstSql, setFirstParams)) {
            m_db.rollbackTransaction();
            m_lastError = "设置第一首播放状态失败：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }

        // 6.3 获取第一首的musicId
        for (const auto &item : std::as_const(m_playItems)) {
            if (item.sortId == nextSortId) {
                nextMusicId = item.musicId;
                break;
            }
        }
        hasNext = true; // 强制设为true，触发后续播放信号
    }

    // 7. 提交事务并刷新模型
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 8. 刷新模型数据
    loadPlayList();
    emit dataChanged(index(0, 0), index(rowCount()-1, ColumnCount-1));

    // 9. 触发播放信号
    if (hasNext && !nextMusicId.isEmpty()) {
        emit playMusic(nextMusicId);
        qDebug() << "播放完成，触发播放信号（musicId=" << nextMusicId << "）";
    }

    return true;
}

/**
 * @brief 根据sort_id删除播放列表中的歌曲
 * @details 若删除的是正在播放的歌曲，先切换到下一首，再执行删除+重新排序
 * @param targetSortId 待删除歌曲的sort_id
 * @return 删除成功返回true
 */
bool Tm_musicPlayListModel::deleteMusicBySortId(int targetSortId)
{
    m_lastError.clear();

    // 1. 校验目标歌曲是否存在
    bool targetExists = false;
    bool isTargetPlaying = false; // 标记目标是否正在播放
    QString targetPlayItemId;     // 目标播放项的ID（用于删除）
    int maxSortId = 0;            // 最大sort_id（用于判断是否删除最后一首）

    for (const auto &item : std::as_const(m_playItems)) {
        if (item.sortId == targetSortId) {
            targetExists = true;
            isTargetPlaying = item.isPlaying;
            targetPlayItemId = item.id;
        }
        if (item.sortId > maxSortId) {
            maxSortId = item.sortId;
        }
    }

    if (!targetExists) {
        m_lastError = "目标歌曲不存在（sort_id=" + QString::number(targetSortId) + "）";
        qWarning() << m_lastError;
        return false;
    }

    // 2. 若目标正在播放：先切换到下一首（无下一首则不切换）
    QString nextMusicId; // 新增：存储下一首音乐ID，用于触发播放
    if (isTargetPlaying) {
        int nextSortId = -1;
        // 查找下一首（当前sort_id+1）
        for (const auto &item : std::as_const(m_playItems)) {
            if (item.sortId == targetSortId + 1) {
                nextSortId = item.sortId;
                nextMusicId = item.musicId; // 记录下一首音乐ID
                break;
            }
        }
        // 切换到下一首（无下一首则不执行切换，播放状态会随删除清空）
        if (nextSortId != -1) {
            if (!switchToMusic(nextSortId)) {
                m_lastError = "切换到下一首失败，删除中止：" + getLastError();
                qWarning() << m_lastError;
                return false;
            }
        } else {
            qDebug() << "删除的是最后一首正在播放的歌曲，无需切换下一首";
        }
    }

    // 3. 开启事务（删除目标项 + 调整后续歌曲排序）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 4. 步骤1：删除目标播放项
    QString deleteSql = "DELETE FROM play_list WHERE id = ?";
    QVector<QVariant> deleteParams = {targetPlayItemId};
    if (!m_db.executeSql(deleteSql, deleteParams)) {
        m_db.rollbackTransaction();
        m_lastError = "删除目标歌曲失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 5. 步骤2：调整后续歌曲的sort_id（所有 > targetSortId 的项减1）
    if (targetSortId < maxSortId) { // 只有存在后续项时才需要调整
        QString updateSortSql = "UPDATE play_list SET sort_id = sort_id - 1 WHERE sort_id > ?";
        QVector<QVariant> updateSortParams = {targetSortId};
        if (!m_db.executeSql(updateSortSql, updateSortParams)) {
            m_db.rollbackTransaction();
            m_lastError = "调整排序失败：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }
    }

    // 6. 提交事务
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 7. 刷新模型数据
    loadPlayList();
    emit dataChanged(index(0, 0), index(rowCount()-1, ColumnCount-1));

    // 修改：先更新下一首的播放状态，再触发信号
    if (isTargetPlaying && !nextMusicId.isEmpty()) {
        if (setMusicAsPlaying(nextMusicId)) { // 同步数据库状态
            // 新增：更新下一首歌曲的播放次数
            if (updateMusicPlayCount(nextMusicId, false)) {
                emit playMusic(nextMusicId); // 状态和次数更新成功后触发播放
            } else {
                qWarning() << "删除后切换歌曲播放次数更新失败，未触发播放：" << getLastError();
            }
        } else {
            qWarning() << "删除后切换播放状态设置失败：" << getLastError();
        }
    }

    qDebug() << "删除歌曲成功（sort_id=" << targetSortId << "）";
    return true;
}

/**
 * @brief 一键清空整个播放列表
 * @return 清空成功返回true
 */
bool Tm_musicPlayListModel::clearPlayList()
{
    m_lastError.clear();

    // 1. 若列表已空，直接返回成功（避免无意义的数据库操作）
    if (m_playItems.isEmpty()) {
        qDebug() << "播放列表已为空，无需清空";
        return true;
    }

    // 2. 开启事务（确保清空操作的原子性）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 3. 执行清空操作（删除play_list表中所有数据）
    QString clearSql = "DELETE FROM play_list;";
    if (!m_db.executeSql(clearSql)) {
        m_db.rollbackTransaction(); // 操作失败回滚事务
        m_lastError = "清空播放列表失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 4. 提交事务
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 5. 刷新模型状态
    beginResetModel();       // 通知视图开始重置数据（避免局部更新混乱）
    m_playItems.clear();     // 清空内存中的播放列表数据
    clearImgCache();         // 清空封面图标缓存（释放内存）
    endResetModel();         // 通知视图完成数据重置

    // 新增：触发停止播放信号
    emit stopPlayback();

    qDebug() << "播放列表清空成功，共删除" << (m_playItems.size() + 1) << "首歌曲（原列表长度）";
    return true;
}

// -------------------------- 辅助接口 --------------------------
QString Tm_musicPlayListModel::getCurrentPlayingMusicId() const
{
    for (const auto &item : m_playItems) {
        if (item.isPlaying) {
            return item.musicId;
        }
    }
    return ""; // 无正在播放的歌曲
}

QString Tm_musicPlayListModel::getLastError() const
{
    return m_lastError;
}

void Tm_musicPlayListModel::clearImgCache()
{
    m_imgCache.clear(); // QCache自动释放内部QIcon指针，无需手动delete
}

PlayListItem Tm_musicPlayListModel::getPlayItem(int row) const  {
    if (row >= 0 && row < m_playItems.size()) {
        return m_playItems[row];
    }
    return PlayListItem();  // 无效行返回空对象
}

// -------------------------- 内部工具函数 --------------------------
bool Tm_musicPlayListModel::initPlayListTable()
{
    // 检查play_list表是否存在，不存在则创建
    if (m_db.tableExists("play_list")) {
        return true;
    }

    // 创建play_list表（字段匹配需求：id(char64)、sort_id(int)、music_id(char64)、playing(bool)、played(bool)）
    QString createSql = "CREATE TABLE IF NOT EXISTS play_list ("
                        "id TEXT PRIMARY KEY NOT NULL, "          // char64（UUID）
                        "sort_id INTEGER NOT NULL, "               // 排序ID
                        "music_id TEXT NOT NULL, "                 // 关联音乐ID（char64）
                        "playing INTEGER DEFAULT 0, "              // 0=未播放，1=正在播放
                        "played INTEGER DEFAULT 0, "               // 0=未播放，1=已播放
                        "FOREIGN KEY(music_id) REFERENCES music_data(id) " // 外键关联音乐表
                        ")";

    if (!m_db.executeSql(createSql)) {
        m_lastError = "创建play_list表失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }
    qDebug() << "play_list表初始化成功";
    return true;
}

MusicDetail Tm_musicPlayListModel::getMusicDetail(const QString &musicId) const
{
    MusicDetail detail;
    if (musicId.isEmpty()) {
        return detail;
    }

    // 从music_data表查询音乐详情（匹配原项目表结构）
    QString sql = "SELECT id, name, auther, duration, img_name, file_path, count, pitch "
                  "FROM music_data "
                  "WHERE id = ?";
    QVector<QVariant> params = {musicId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    if (!result.isEmpty() && m_db.getLastError().isEmpty()) {
        const auto &row = result.first();
        detail.id = row["id"].toString();
        detail.name = row["name"].toString();
        detail.auther = row["auther"].toString();
        detail.duration = row["duration"].toInt();
        detail.imgName = row["img_name"].toString();
        detail.pitch = row["pitch"].toInt();
        detail.filePath = row["file_path"].toString();
        detail.playCount = row["count"].toInt();
    }
    return detail;
}

QString Tm_musicPlayListModel::formatDuration(int seconds) const
{
    int minutes = seconds / 60;
    int secs = seconds % 60;
    // 补零确保格式统一（如 5秒→"00:05"，1分3秒→"01:03"）
    return QString("%1:%2").arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QPixmap Tm_musicPlayListModel::getRoundRectPixmap(const QPixmap &src, const QSize &size, int radius) const
{
    if (src.isNull() || size.width() <= 0 || size.height() <= 0) {
        return QPixmap();
    }

    // 缩放原图至目标尺寸（保持平滑）
    QPixmap scaled = src.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QPixmap dest(size.width(), size.height());
    dest.fill(Qt::transparent); // 透明背景

    // 绘制圆角
    QPainter painter(&dest);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    QPainterPath path;
    path.addRoundedRect(QRect(0, 0, size.width(), size.height()), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, scaled);

    return dest;
}

/**
 * @brief 内部工具：设置指定musicId为播放状态（同步数据库）
 * @param targetMusicId 目标音乐ID
 * @return 操作成功返回true
 */
bool Tm_musicPlayListModel::setMusicAsPlaying(const QString& targetMusicId)
{
    // 1. 查找目标音乐对应的sort_id（播放状态通过sort_id更新）
    int targetSortId = -1;
    for (const auto& item : std::as_const(m_playItems)) {
        if (item.musicId == targetMusicId) {
            targetSortId = item.sortId;
            break;
        }
    }
    if (targetSortId == -1) {
        m_lastError = "目标音乐不在播放列表中：" + targetMusicId;
        qWarning() << m_lastError;
        return false;
    }

    // 2. 开启事务（原子性更新：重置旧状态+设置新状态）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 3. 重置所有歌曲的playing状态（置0），旧播放歌曲标记为played（1）
    QString resetSql = "UPDATE play_list "
                       "SET playing = 0, "
                       "    played = CASE WHEN playing = 1 THEN 1 ELSE played END";
    if (!m_db.executeSql(resetSql)) {
        m_db.rollbackTransaction();
        m_lastError = "重置播放状态失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 4. 设置目标歌曲为playing=1、played=0
    QString setSql = "UPDATE play_list "
                     "SET playing = 1, played = 0 "
                     "WHERE sort_id = ?";
    QVector<QVariant> setParams = {targetSortId};
    if (!m_db.executeSql(setSql, setParams)) {
        m_db.rollbackTransaction();
        m_lastError = "设置目标播放状态失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 5. 提交事务
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 6. 刷新内存数据（同步数据库状态）
    loadPlayList();
    emit dataChanged(index(0, 0), index(rowCount()-1, ColumnCount-1));
    return true;
}

/**
 * @brief 内部工具：更新指定音乐的播放次数（count+1）
 * @param musicId 目标音乐ID
 * @param inTransaction 是否处于外部事务中（true=嵌入当前事务，false=单独开启事务）
 * @return 操作成功返回true
 */
bool Tm_musicPlayListModel::updateMusicPlayCount(const QString& musicId, bool inTransaction)
{
    // 1. 校验音乐ID有效性
    if (musicId.isEmpty()) {
        qWarning() << "更新播放次数失败：音乐ID为空";
        return false;
    }

    // 2. 校验音乐是否存在
    MusicDetail detail = getMusicDetail(musicId);
    if (detail.id.isEmpty()) {
        qWarning() << "更新播放次数失败：音乐不存在（musicId=" << musicId << "）";
        return false;
    }

    // 3. 执行更新（count+1）
    QString updateSql = "UPDATE music_data SET count = count + 1 WHERE id = ?";
    QVector<QVariant> params = {musicId};

    // 外部已开启事务：直接执行
    if (inTransaction) {
        if (!m_db.executeSql(updateSql, params)) {
            m_lastError = "更新播放次数失败（musicId=" + musicId + "）：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }
    }
    // 外部无事务：单独开启事务
    else {
        if (!m_db.beginTransaction()) {
            m_lastError = "更新播放次数：开启事务失败：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }

        bool updateSuccess = m_db.executeSql(updateSql, params);
        if (!updateSuccess) {
            m_db.rollbackTransaction();
            m_lastError = "更新播放次数失败（musicId=" + musicId + "）：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }

        if (!m_db.commitTransaction()) {
            m_db.rollbackTransaction();
            m_lastError = "更新播放次数：提交事务失败：" + m_db.getLastError();
            qWarning() << m_lastError;
            return false;
        }
    }

    qDebug() << "音乐播放次数更新成功（musicId=" << musicId << "），当前次数：" << (detail.playCount + 1);
    return true;
}

/**
 * @brief 切换到当前播放歌曲的上一首
 * @return 切换成功返回true，无有效上一首时返回false
 */
bool Tm_musicPlayListModel::prevMusic()
{
    m_lastError.clear();

    // 1. 查找当前播放歌曲的sort_id和musicId
    int currentSortId = -1;
    QString currentMusicId;
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.isPlaying) {
            currentSortId = item.sortId;
            currentMusicId = item.musicId;
            break;
        }
    }

    // 2. 校验当前播放状态：无播放歌曲或已是第一首（sort_id=1），直接返回
    if (currentSortId == -1) {
        m_lastError = "无当前播放歌曲，无法切换上一首";
        qWarning() << m_lastError;
        return false;
    }
    if (currentSortId == 1) {
        m_lastError = "当前已是第一首歌曲，无可用上一首";
        qDebug() << m_lastError; // 仅调试输出，不报错
        return false;
    }

    // 3. 查找上一首歌曲的sort_id和musicId（当前sort_id-1）
    int prevSortId = -1;
    QString prevMusicId;
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.sortId == currentSortId - 1) {
            prevSortId = item.sortId;
            prevMusicId = item.musicId;
            break;
        }
    }

    // 4. 上一首理论上必然存在（currentSortId>1且列表有序），此处做兜底校验
    if (prevSortId == -1) {
        m_lastError = "上一首歌曲不存在（数据异常，sort_id=" + QString::number(currentSortId - 1) + "）";
        qWarning() << m_lastError;
        return false;
    }

    // 5. 开启事务（原子操作：标记当前为已播放+重置旧状态+设置上一首为播放中+更新播放次数）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 6. 步骤1：将当前歌曲标记为 played=1，playing=0（无论是否播放完成，切歌即视为已播放）
    QString markCurrentSql = "UPDATE play_list "
                             "SET played = 1, playing = 0 "
                             "WHERE sort_id = ?";
    QVector<QVariant> markCurrentParams = {currentSortId};
    if (!m_db.executeSql(markCurrentSql, markCurrentParams)) {
        m_db.rollbackTransaction();
        m_lastError = "标记当前歌曲为已播放失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 7. 步骤2：重置所有歌曲的playing状态（避免残留多个播放状态）
    QString resetSql = "UPDATE play_list SET playing = 0 WHERE sort_id != ?";
    QVector<QVariant> resetParams = {prevSortId};
    if (!m_db.executeSql(resetSql, resetParams)) {
        m_db.rollbackTransaction();
        m_lastError = "重置其他歌曲播放状态失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 8. 步骤3：设置上一首歌曲为 playing=1，played=0（覆盖历史played状态）
    QString setPrevSql = "UPDATE play_list "
                         "SET playing = 1, played = 0 "
                         "WHERE sort_id = ?";
    QVector<QVariant> setPrevParams = {prevSortId};
    if (!m_db.executeSql(setPrevSql, setPrevParams)) {
        m_db.rollbackTransaction();
        m_lastError = "设置上一首播放状态失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 9. 步骤4：更新上一首歌曲的播放次数（count+1），嵌入当前事务
    if (!updateMusicPlayCount(prevMusicId, true)) {
        m_db.rollbackTransaction(); // 播放次数更新失败，回滚整个事务
        return false;
    }

    // 10. 提交事务并刷新模型
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 11. 刷新模型数据并通知视图更新
    loadPlayList();
    emit dataChanged(index(0, 0), index(rowCount()-1, ColumnCount-1));

    // 12. 触发播放信号（通知播放器播放上一首）
    emit playMusic(prevMusicId);
    qDebug() << "切换上一首成功，触发播放信号（musicId=" << prevMusicId << "）";

    return true;
}

bool Tm_musicPlayListModel::setMusicPitch(const QString &music_id, int pitch)
{
    m_lastError.clear();

    // 1. 校验音乐ID合法性
    if (music_id.isEmpty()) {
        m_lastError = "音乐ID不能为空";
        qWarning() << m_lastError;
        return false;
    }

    // 2. 校验音乐是否存在于music_data表
    MusicDetail detail = getMusicDetail(music_id);
    if (detail.id.isEmpty()) {
        m_lastError = "目标音乐不存在（music_id=" + music_id + "）";
        qWarning() << m_lastError;
        return false;
    }

    // 3. 执行pitch更新操作（事务保证原子性）
    if (!m_db.beginTransaction()) {
        m_lastError = "开启事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 4. 更新music_data表的pitch字段
    QString updateSql = "UPDATE music_data "
                        "SET pitch = ? "
                        "WHERE id = ?";
    QVector<QVariant> params = {pitch, music_id};
    if (!m_db.executeSql(updateSql, params)) {
        m_db.rollbackTransaction();
        m_lastError = "更新pitch失败（music_id=" + music_id + "）：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    // 5. 提交事务
    if (!m_db.commitTransaction()) {
        m_db.rollbackTransaction();
        m_lastError = "提交事务失败：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }

    qDebug() << "音乐pitch更新成功（music_id=" << music_id << "，新pitch=" << pitch << "）";
    return true;
}

bool Tm_musicPlayListModel::isMusicHasLrc(const QString &musicId) const
{
    if (musicId.isEmpty()) {
        qDebug() << "音乐ID为空，无法判断是否存在歌词";
        return false;
    }

    QString sql = "SELECT yrc "
                  "FROM music_lrc "
                  "WHERE id = ? AND yrc IS NOT NULL AND yrc != ''";
    QVector<QVariant> params = {musicId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    if (!m_db.getLastError().isEmpty()) {
        qWarning() << "查询歌词失败（musicId=" << musicId << "）：" << m_db.getLastError();
        return false;
    }

    return !result.isEmpty();
}

// 调整工具函数：获取指定歌单内所有有效音乐ID（按music_data的sort_id排序）
bool Tm_musicPlayListModel::getMusicIdsFromGroup(const QString &groupId, QVector<QString> &outMusicIds)
{
    m_lastError.clear();
    outMusicIds.clear();

    // 1. 校验歌单ID合法性
    if (groupId.isEmpty()) {
        m_lastError = "歌单ID不能为空";
        qWarning() << m_lastError;
        return false;
    }

    // 2. 从music_group关联music_data查询，按music_data的sort_id排序
    // 注意：通过music_id关联两张表，按music_data的sort_id排序
    QString sql = "SELECT mg.music_id "
                  "FROM music_group mg "
                  "JOIN music_data md ON mg.music_id = md.id "
                  "WHERE mg.group_id = ? "
                  "ORDER BY md.sort_id ASC"; // 按music_data的sort_id排序
    QVector<QVariant> params = {groupId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    // 3. 校验查询结果
    if (!m_db.getLastError().isEmpty()) {
        m_lastError = "查询歌单音乐失败（group_id=" + groupId + "）：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }
    if (result.isEmpty()) {
        m_lastError = "指定歌单无音乐数据（group_id=" + groupId + "）";
        qWarning() << m_lastError;
        return false;
    }

    // 4. 提取音乐ID到输出列表（已按music_data.sort_id排序）
    for (const auto &row : std::as_const(result)) {
        QString musicId = row["music_id"].toString();
        // 过滤无效音乐（理论上JOIN后不会出现无效ID，这里做双重校验）
        if (!getMusicDetail(musicId).id.isEmpty()) {
            outMusicIds.append(musicId);
        } else {
            qWarning() << "歌单包含无效音乐，已过滤（music_id=" << musicId << "，group_id=" << groupId << "）";
        }
    }

    // 5. 校验过滤后是否还有有效音乐
    if (outMusicIds.isEmpty()) {
        m_lastError = "指定歌单内无有效音乐（所有音乐均不存在于music_data表，group_id=" + groupId + "）";
        qWarning() << m_lastError;
        return false;
    }

    return true;
}

// 调整接口1：按歌单ID和music_data的sort_id点歌
bool Tm_musicPlayListModel::orderMusicFromGroupBySortId(const QString &groupId, int musicDataSortId, bool addToNext)
{
    m_lastError.clear();

    // 1. 直接查询歌单中指定sort_id（music_data中的sort_id）的音乐ID
    QString sql = "SELECT mg.music_id "
                  "FROM music_group mg "
                  "JOIN music_data md ON mg.music_id = md.id "
                  "WHERE mg.group_id = ? AND md.sort_id = ?";
    QVector<QVariant> params = {groupId, musicDataSortId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    // 2. 校验查询结果
    if (!m_db.getLastError().isEmpty()) {
        m_lastError = "查询指定歌曲失败（group_id=" + groupId + "，sort_id=" + QString::number(musicDataSortId) + "）：" + m_db.getLastError();
        qWarning() << m_lastError;
        return false;
    }
    if (result.isEmpty()) {
        m_lastError = "歌单中不存在指定sort_id的歌曲（group_id=" + groupId + "，sort_id=" + QString::number(musicDataSortId) + "）";
        qWarning() << m_lastError;
        return false;
    }

    // 3. 获取目标音乐ID
    QString targetMusicId = result.first()["music_id"].toString();

    // 4. 校验目标音乐是否已在播放列表中
    /*bool isAlreadyInList = false;
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.musicId == targetMusicId) {
            isAlreadyInList = true;
            break;
        }
    }
    if (isAlreadyInList) {
        m_lastError = "目标歌曲已在播放列表中（music_id=" + targetMusicId + "，group_id=" + groupId + "）";
        qWarning() << m_lastError;
        return false; // 若允许重复添加，可改为return true
    }*/

    // 5. 根据添加位置参数，调用现有接口完成添加
    bool addSuccess = false;
    if (addToNext) {
        addSuccess = addMusicToCurrentPos(targetMusicId); // 添加到下一首播放
    } else {
        addSuccess = addMusicToEnd(targetMusicId); // 添加到列表结尾
    }

    // 6. 补充操作结果日志
    if (addSuccess) {
        qDebug() << "指定歌单点歌成功（group_id=" << groupId << "，music_data.sort_id=" << musicDataSortId
                 << "，music_id=" << targetMusicId << "，添加位置：" << (addToNext ? "下一首" : "列表结尾") << "）";
    } else {
        m_lastError = "点歌失败：" + m_lastError; // 继承addMusic接口的错误信息
        qWarning() << m_lastError;
    }

    return addSuccess;
}

// 随机点歌接口无需大幅调整（仅依赖getMusicIdsFromGroup的排序逻辑）
bool Tm_musicPlayListModel::orderRandomMusicFromGroup(const QString &groupId, bool addToNext)
{
    m_lastError.clear();

    // 1. 获取指定歌单内所有有效音乐ID（已按music_data.sort_id排序）
    QVector<QString> groupMusicIds;
    if (!getMusicIdsFromGroup(groupId, groupMusicIds)) {
        return false; // 错误信息已在getMusicIdsFromGroup中设置
    }

    // 2. 生成随机索引（从歌单音乐ID列表中随机选择）
    QRandomGenerator randGen = QRandomGenerator::securelySeeded(); // 安全随机种子
    int randomIndex = randGen.bounded(groupMusicIds.size()); // 生成0~size-1的随机索引
    QString targetMusicId = groupMusicIds[randomIndex];

    // 3. 检查目标音乐是否已在播放列表中
    /*bool isAlreadyInList = false;
    for (const auto &item : std::as_const(m_playItems)) {
        if (item.musicId == targetMusicId) {
            isAlreadyInList = true;
            break;
        }
    }
    if (isAlreadyInList) {
        // 若已存在，最多重试5次
        const int maxRetry = 5;
        int retryCount = 0;
        while (isAlreadyInList && retryCount < maxRetry) {
            randomIndex = randGen.bounded(groupMusicIds.size());
            targetMusicId = groupMusicIds[randomIndex];
            isAlreadyInList = false;
            for (const auto &item : std::as_const(m_playItems)) {
                if (item.musicId == targetMusicId) {
                    isAlreadyInList = true;
                    break;
                }
            }
            retryCount++;
        }
        // 多次重试后仍重复，返回失败
        if (isAlreadyInList) {
            m_lastError = "随机点歌失败：歌单内所有有效音乐已在播放列表中（group_id=" + groupId + "）";
            qWarning() << m_lastError;
            return false;
        }
    }*/

    // 4. 根据添加位置参数，调用现有接口完成添加
    bool addSuccess = false;
    if (addToNext) {
        addSuccess = addMusicToCurrentPos(targetMusicId); // 添加到下一首播放
    } else {
        addSuccess = addMusicToEnd(targetMusicId); // 添加到列表结尾
    }

    // 5. 补充操作结果日志
    if (addSuccess) {
        qDebug() << "指定歌单随机点歌成功（group_id=" << groupId << "，随机选中music_id=" << targetMusicId
                 << "，添加位置：" << (addToNext ? "下一首" : "列表结尾") << "）";
    } else {
        m_lastError = "随机点歌失败：" + m_lastError; // 继承addMusic接口的错误信息
        qWarning() << m_lastError;
    }

    return addSuccess;
}
