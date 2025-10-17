#include "Tm_listViewModel.h"
#include <QUuid>
#include <QDebug>
#include <qdatetime.h>
#include <qdir.h>

Tm_listViewModel::Tm_listViewModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // 初始化时自动加载所有歌单
    loadAllGroups();
}

// 歌单数量（行数）
int Tm_listViewModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0; // 不支持树形结构，父节点无效时返回0
    return m_groupItems.size();
}

// 获取指定索引、指定角色的数据
QVariant Tm_listViewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_groupItems.size()) {
        return QVariant(); // 索引无效或越界，返回空
    }

    const GroupItem &group = m_groupItems[index.row()];
    switch (role) {
    case Qt::DisplayRole:  // 列表默认显示（歌单名称）
    case NameRole:         // 自定义角色：歌单名称
        return group.name;
    case IdRole:           // 自定义角色：歌单ID
        return group.id;
    case ImgRole:          // 自定义角色：歌单封面
        return group.img;
    default:
        return QVariant();
    }
}

// 注册自定义角色（供QML或视图层调用）
QHash<int, QByteArray> Tm_listViewModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "groupId";
    roles[NameRole] = "groupName";
    roles[ImgRole] = "groupImg";
    return roles;
}

// 加载所有歌单（从group_data表查询）
bool Tm_listViewModel::loadAllGroups()
{
    beginResetModel(); // 开始重置模型（避免视图频繁更新）
    m_groupItems.clear();

    // SQL查询：获取所有歌单（按ID排序）
    QString sql = "SELECT id, name, img FROM group_data ORDER BY id";
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql);

    if (m_db.getLastError().isEmpty()) {
        // 解析查询结果，存入m_groupItems
        for (const auto &row : std::as_const(result)) {
            GroupItem group;
            group.id = row["id"].toString();
            group.name = row["name"].toString();
            group.img = row["img"].toString();
            m_groupItems.append(group);
        }
        endResetModel(); // 结束重置模型
        return true;
    } else {
        qWarning() << "加载歌单失败：" << m_db.getLastError();
        endResetModel();
        return false;
    }
}

// 添加歌单（完整参数）
bool Tm_listViewModel::addGroup(const GroupItem &group)
{
    // 1. 检查歌单ID是否已存在（避免重复）
    for (const auto &item : std::as_const(m_groupItems)) {
        if (item.id == group.id) {
            qWarning() << "歌单ID已存在：" << group.id;
            return false;
        }
    }

    // 2. 执行SQL插入（使用参数绑定，避免SQL注入）
    QString sql = "INSERT INTO group_data (id, name, img) VALUES (?, ?, ?)";
    QVector<QVariant> params = {
        group.id,
        group.name,
        group.img.isEmpty() ? ":/cloudbread/src/favicon_round.png" : group.img // 默认封面
    };

    if (m_db.executeSql(sql, params)) {
        // 3. 插入成功，更新模型数据
        beginInsertRows(QModelIndex(), m_groupItems.size(), m_groupItems.size());
        m_groupItems.append(group);
        endInsertRows();
        return true;
    } else {
        qWarning() << "添加歌单失败：" << m_db.getLastError();
        return false;
    }
}

// 添加歌单（简化接口：仅传名称，自动生成ID和默认封面）
bool Tm_listViewModel::addGroup(const QString &name)
{
    GroupItem group;
    group.id = QUuid::createUuid().toString(QUuid::WithoutBraces); // 生成32位UUID
    group.name = name;
    group.img = ":/cloudbread/src/favicon_round.png"; // 默认封面路径（需自行定义资源）
    return addGroup(group);
}

// 删除歌单（先删除歌单中独有的歌曲及其封面，再删除歌单本身）
bool Tm_listViewModel::deleteGroup(const QString &groupId)
{
    // 1. 先获取该歌单中的所有歌曲ID
    QString getMusicIdsSql = "SELECT music_id FROM music_group WHERE group_id = ?";
    QVector<QVariant> params = {groupId};
    QVector<QMap<QString, QVariant>> musicResult = m_db.querySql(getMusicIdsSql, params);

    if (!m_db.getLastError().isEmpty()) {
        qWarning() << "查询歌单歌曲失败：" << m_db.getLastError();
        return false;
    }

    // 2. 遍历处理每首歌曲
    for (const auto &row : musicResult) {
        QString musicId = row["music_id"].toString();

        // 2.1 检查歌曲是否存在于其他歌单
        QString checkOtherGroupsSql = "SELECT COUNT(*) AS count FROM music_group "
                                      "WHERE music_id = ? AND group_id != ?";
        QVector<QVariant> checkParams = {musicId, groupId};
        QVector<QMap<QString, QVariant>> checkResult = m_db.querySql(checkOtherGroupsSql, checkParams);

        int otherGroupCount = 0;
        if (!checkResult.isEmpty()) {
            otherGroupCount = checkResult.first()["count"].toInt();
        }

        // 2.2 只处理不存在于其他歌单的歌曲
        if (otherGroupCount == 0) {
            // 获取歌曲文件路径和封面路径
            QString getMusicSql = "SELECT file_path, img_name FROM music_data WHERE id = ?";
            QVector<QVariant> musicParams = {musicId};
            QVector<QMap<QString, QVariant>> musicInfo = m_db.querySql(getMusicSql, musicParams);

            if (!musicInfo.isEmpty()) {
                QString filePath = musicInfo.first()["file_path"].toString();
                QString coverPath = musicInfo.first()["img_name"].toString();

                // 删除音频文件
                if (!filePath.isEmpty()) {
                    QFile audioFile(filePath);
                    if (audioFile.exists() && !audioFile.remove()) {
                        qWarning() << "删除音频文件失败：" << audioFile.errorString() << "路径：" << filePath;
                    } else if (audioFile.exists()) {
                        qDebug() << "成功删除音频文件：" << filePath;
                    }
                }

                // 删除封面文件（新增逻辑）
                if (!coverPath.isEmpty() && !coverPath.startsWith(":/")) { // 排除资源文件
                    QFile coverFile(coverPath);
                    if (coverFile.exists() && !coverFile.remove()) {
                        qWarning() << "删除封面文件失败：" << coverFile.errorString() << "路径：" << coverPath;
                    } else if (coverFile.exists()) {
                        qDebug() << "成功删除封面文件：" << coverPath;
                    }
                }

                // 删除歌曲数据记录
                QString delMusicSql = "DELETE FROM music_data WHERE id = ?";
                if (!m_db.executeSql(delMusicSql, {musicId})) {
                    qWarning() << "删除歌曲数据失败：" << m_db.getLastError();
                } else {
                    qDebug() << "成功删除歌曲数据：" << musicId;
                }
            }
        }

        // 2.3 无论歌曲是否在其他歌单，都删除当前歌单与该歌曲的关联
        QString delRelSql = "DELETE FROM music_group WHERE music_id = ? AND group_id = ?";
        if (!m_db.executeSql(delRelSql, {musicId, groupId})) {
            qWarning() << "删除歌曲与歌单关联失败：" << m_db.getLastError();
        }
    }

    // 3. 开启事务删除歌单本身
    if (!m_db.beginTransaction()) {
        qWarning() << "开启事务失败：" << m_db.getLastError();
        return false;
    }

    // 4. 删除歌单记录
    QString sql = "DELETE FROM group_data WHERE id = ?";
    QVector<QVariant> params2 = {groupId};
    if (m_db.executeSql(sql, params2)) {
        m_db.commitTransaction();

        // 5. 更新模型数据
        for (int i = 0; i < m_groupItems.size(); ++i) {
            if (m_groupItems[i].id == groupId) {
                beginRemoveRows(QModelIndex(), i, i);
                m_groupItems.removeAt(i);
                endRemoveRows();
                return true;
            }
        }
    } else {
        m_db.rollbackTransaction();
        qWarning() << "删除歌单失败：" << m_db.getLastError();
        return false;
    }

    return false;
}


// 更新歌单（名称/封面）
bool Tm_listViewModel::updateGroup(const GroupItem &group)
{
    // 1. 执行SQL更新
    QString sql = "UPDATE group_data SET name = ?, img = ? WHERE id = ?";
    QVector<QVariant> params = {group.name, group.img, group.id};

    if (m_db.executeSql(sql, params)) {
        // 2. 更新模型数据
        for (int i = 0; i < m_groupItems.size(); ++i) {
            if (m_groupItems[i].id == group.id) {
                m_groupItems[i] = group;
                // 通知视图：当前行数据已更新
                emit dataChanged(index(i), index(i));
                return true;
            }
        }
        return true;
    } else {
        qWarning() << "更新歌单失败：" << m_db.getLastError();
        return false;
    }
}

// 按ID获取歌单信息
GroupItem Tm_listViewModel::getGroupById(const QString &groupId) const
{
    for (const auto &group : m_groupItems) {
        if (group.id == groupId) {
            return group;
        }
    }
    return GroupItem(); // 未找到返回空结构
}

// 获取歌单内歌曲数量（关联music_group和music_data）
int Tm_listViewModel::getGroupMusicCount(const QString &groupId) const
{
    QString sql = "SELECT COUNT(*) AS count FROM music_group "
                  "INNER JOIN music_data ON music_group.music_id = music_data.id "
                  "WHERE music_group.group_id = ?";
    QVector<QVariant> params = {groupId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    if (!result.isEmpty() && m_db.getLastError().isEmpty()) {
        return result.first()["count"].toInt();
    }
    return 0;
}

QString Tm_listViewModel::getRandomMusicCoverByGroupId(const QString& groupId) {
    // 1. 查询该歌单下所有歌曲的img_name（封面路径）
    QString sql = "SELECT m.img_name "
                  "FROM music_data m "
                  "INNER JOIN music_group mg ON m.id = mg.music_id "
                  "WHERE mg.group_id = ? AND m.img_name IS NOT NULL AND m.img_name != ''";
    QVector<QVariant> params = {groupId};
    QVector<QMap<QString, QVariant>> result = m_db.querySql(sql, params);

    // 2. 若查询失败或无歌曲，返回空
    if (!m_db.getLastError().isEmpty() || result.isEmpty()) {
        qDebug() << "未查询到歌单" << groupId << "下的有效歌曲封面";
        return "";
    }

    // 3. 随机选择一条结果返回
    srand(QTime::currentTime().msecsSinceStartOfDay()); // 初始化随机数种子
    int randomIndex = rand() % result.size();
    return result[randomIndex]["img_name"].toString();
}
