#include "MusicImporter.h"
#include "SqliteManager.h"
#include "audiometadatamarser.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include "LyricAndCoverBatchFetcher.h"
#include "QStandardPaths.h"
#include "GlobalConfig.h"

MusicImporter::MusicImporter(QObject *parent) : QObject(parent), m_lastError("")
{
    qDebug() << "MusicImporter initialized";
    // 确保关联表存在
    // bool relationTableCreated = createMusicGroupRelationTable();
    // qDebug() << "Music-group relation table check/creation:" << (relationTableCreated ? "Success" : "Failed - " + m_lastError);
}

int MusicImporter::importMusic(const QStringList& filePaths, const QString& playlistId)
{
    qDebug() << "Starting music import - Files count:" << filePaths.size() << "Target playlist ID:" << playlistId;

    m_lastError.clear();
    int importedCount = 0;
    int totalCount = filePaths.size();

    // 检查歌单是否存在
    if (!SqliteManager::getInstance().tableExists("group_data")) {
        m_lastError = "歌单表不存在";
        qCritical() << "Import failed:" << m_lastError;
        emit importError(m_lastError);
        return 0;
    }

    QString checkGroupSql = "SELECT id FROM group_data WHERE id = ?";
    auto groupResult = SqliteManager::getInstance().querySql(checkGroupSql, {playlistId});
    if (groupResult.isEmpty()) {
        m_lastError = "指定的歌单不存在";
        qCritical() << "Import failed:" << m_lastError << "Playlist ID:" << playlistId;
        emit importError(m_lastError);
        return 0;
    }
    qDebug() << "Verified playlist exists - ID:" << playlistId;

    // 获取当前歌单中最大的排序ID（从music_data表中获取）
    int maxSortId = getMaxSortIdInGroup(playlistId);
    qDebug() << "Current max sort ID in playlist:" << maxSortId;
    int currentSortId = maxSortId + 1;  // 新音乐从最大排序ID+1开始

    // 开始事务
    if (!SqliteManager::getInstance().beginTransaction()) {
        m_lastError = "无法开始数据库事务: " + SqliteManager::getInstance().getLastError();
        qCritical() << "Import failed:" << m_lastError;
        emit importError(m_lastError);
        return 0;
    }
    qDebug() << "Database transaction started";

    // 遍历所有文件路径
    foreach (const QString& filePath, filePaths) {
        QFileInfo fileInfo(filePath);
        qDebug() << "\nProcessing file:" << filePath << "Name:" << fileInfo.fileName();

        if (!fileInfo.exists() || !fileInfo.isFile()) {
            qWarning() << "File does not exist or is not a valid file:" << filePath;
            emit importError("文件不存在或无效: " + fileInfo.fileName());
            continue;
        }

        // 生成音乐ID
        QString musicId = generateMusicId(filePath, playlistId);
        qDebug() << "Generated music ID:" << musicId;

        // -------------------------- 修改：文件复制逻辑（替代移动） --------------------------
        // 1. 构建目标目录（GlobalConfig::getInstance().getValue("App/DBpath","").toString()/AudioFiles/group_id/）
        QString targetDirPath = QString("%1/AudioFiles/%2/")
                                    .arg(GlobalConfig::getInstance().getValue("App/DBpath","").toString())
                                    .arg(playlistId);
        QDir targetDir(targetDirPath);
        // 若目录不存在则创建（包括多级目录）
        if (!targetDir.exists() && !targetDir.mkpath(".")) {
            m_lastError = "创建目标目录失败: " + targetDirPath;
            qCritical() << m_lastError;
            emit importError(m_lastError);
            // 继续处理下一个文件，避免单个目录创建失败阻塞全部
            continue;
        }

        // 2. 构建目标文件路径（music_id.原始后缀）
        QString targetFilePath = targetDirPath + musicId + "." + fileInfo.suffix();
        qDebug() << "Target file path after copy:" << targetFilePath;

        // 3. 复制文件（若目标文件已存在，先删除旧文件）
        QFile sourceFile(filePath);
        if (QFile::exists(targetFilePath)) {
            qDebug() << "Target file already exists, removing old file:" << targetFilePath;
            if (!QFile::remove(targetFilePath)) {
                m_lastError = "删除已存在的目标文件失败: " + targetFilePath;
                qCritical() << m_lastError;
                emit importError(m_lastError);
                continue;
            }
        }

        // 使用copy代替rename，实现文件复制（保留原文件）
        if (!sourceFile.copy(targetFilePath)) {
            m_lastError = "复制文件失败: " + sourceFile.errorString() + " (源:" + filePath + " 目标:" + targetFilePath + ")";
            qCritical() << m_lastError;
            emit importError(m_lastError);
            continue;
        }
        qDebug() << "File copied successfully to target path (original file preserved)";
        // -------------------------- 文件复制逻辑结束 --------------------------

        // 检查音乐是否已存在（基于music_id，与文件位置无关）
        if (isMusicExists(musicId)) {
            qDebug() << "Music already exists in database - ID:" << musicId;

            // 添加到歌单关联（不需要排序ID）
            if (addMusicToGroup(musicId, playlistId)) {
                importedCount++;
                qDebug() << "Added existing music to playlist - Count:" << importedCount << "Sort ID:" << currentSortId;
                emit importProgress(importedCount, totalCount);
                currentSortId++;
            } else {
                qWarning() << "Failed to add existing music to playlist - ID:" << musicId;
            }
            continue;
        }

        // 解析音频元数据（使用复制后的文件路径）
        qDebug() << "Parsing audio metadata...";
        AudioMetadata metadata = AudioMetadataParser::parse(targetFilePath);
        // 元数据默认值填充
        if (!metadata.isParsed) {
            qWarning() << "Failed to parse metadata for file:" << targetFilePath;
            emit importError("解析元数据失败: " + fileInfo.fileName());
            // 填充默认元数据
            metadata.title = fileInfo.baseName();          // 解析失败用原始文件名作为歌名
            metadata.artist = "未知创作者";                 // 默认作者
            metadata.duration = 0;                         // 默认时长
            metadata.coverPath = ":/cloudbread/src/favicon_round.png";  // 默认封面
            metadata.isParsed = true;  // 标记为"已处理"，避免后续被跳过
        } else {
            // 解析成功但部分字段为空时填充默认值
            if (metadata.title.isEmpty() || metadata.coverPath.isEmpty()) metadata.title = fileInfo.baseName();
            if (metadata.artist.isEmpty() || metadata.coverPath.isEmpty()) metadata.artist = "未知创作者";
            if (metadata.duration <= 0) metadata.duration = 0;
            if (metadata.coverPath.isEmpty()) metadata.coverPath = ":/cloudbread/src/favicon_round.png";
        }
        qDebug() << "Metadata (after default fill) - Title:" << metadata.title << "Artist:" << metadata.artist
                 << "Duration:" << metadata.duration << "Cover path:" << metadata.coverPath;

        // 准备插入音乐数据（包含sort_id，filePath为复制后路径）
        QString insertMusicSql = "INSERT INTO music_data ("
                                 "id, sort_id, img_name, name, auther, "
                                 "duration, timestamp, pitch, count, file_path"
                                 ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

        QVector<QVariant> params;
        params << musicId
               << currentSortId                          // 使用当前排序ID
               << metadata.coverPath                     // 默认/解析的封面路径
               << metadata.title                         // 默认/解析的歌名
               << metadata.artist                        // 默认/解析的作者
               << metadata.duration                      // 默认/解析的时长
               << QDateTime::currentSecsSinceEpoch()     // 当前时间戳
               << 0                                      // pitch默认值
               << 0                                      // count默认值
               << targetFilePath;                        // 复制后的文件路径

        // 插入音乐数据
        if (!SqliteManager::getInstance().executeSql(insertMusicSql, params)) {
            m_lastError = "插入音乐数据失败: " + SqliteManager::getInstance().getLastError();
            qCritical() << m_lastError << "SQL:" << insertMusicSql;
            SqliteManager::getInstance().rollbackTransaction();
            qDebug() << "Transaction rolled back due to error";
            emit importError(m_lastError);
            // 回滚文件复制（删除已复制的文件）
            QFile::remove(targetFilePath);
            qDebug() << "Rolled back file copy - Deleted:" << targetFilePath;
            return importedCount;
        }
        qDebug() << "Music data inserted successfully - ID:" << musicId << "Sort ID:" << currentSortId;

        // 将音乐添加到歌单（不需要排序ID）
        if (addMusicToGroup(musicId, playlistId)) {
            importedCount++;
            qDebug() << "Music added to playlist - Total imported:" << importedCount;
            emit importProgress(importedCount, totalCount);
            currentSortId++;  // 排序ID递增
        } else {
            qWarning() << "Failed to add music to playlist - ID:" << musicId;
            // 回滚已插入的音乐数据和文件复制
            SqliteManager::getInstance().executeSql("DELETE FROM music_data WHERE id = ?", {musicId});
            QFile::remove(targetFilePath);
            qDebug() << "Rolled back music data insertion and file copy - ID:" << musicId;
        }
    }

    // 提交事务
    if (!SqliteManager::getInstance().commitTransaction()) {
        m_lastError = "提交数据库事务失败: " + SqliteManager::getInstance().getLastError();
        qCritical() << m_lastError;
        SqliteManager::getInstance().rollbackTransaction();
        qDebug() << "Transaction rolled back";
        emit importError(m_lastError);
        // 回滚所有已复制的文件
        return importedCount;
    }
    qDebug() << "Transaction committed successfully";

    qDebug() << "Import completed - Total imported:" << importedCount << "of" << totalCount;
    emit importFinished(importedCount, totalCount);
    LyricAndCoverBatchFetcher::getInstance()->startBatchFetch();
    return importedCount;
}

QString MusicImporter::getLastError() const
{
    return m_lastError;
}

QString MusicImporter::generateMusicId(const QString& filePath, QString group_id)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(filePath.toUtf8());
    QString hashData = hash.result().toHex() + group_id;
    QString musicId = QCryptographicHash::hash(hashData.toLocal8Bit(),QCryptographicHash::Md5).toHex();
    return musicId;
}

bool MusicImporter::isMusicExists(const QString& musicId)
{
    QString sql = "SELECT id FROM music_data WHERE id = ?";
    auto result = SqliteManager::getInstance().querySql(sql, {musicId});
    bool exists = !result.isEmpty();
    return exists;
}

bool MusicImporter::createMusicGroupRelationTable()
{
    if (SqliteManager::getInstance().tableExists("music_group")) {
        return true;
    }

    // 创建音乐与歌单的关联表（不含sort_id字段）
    QString createSql = "CREATE TABLE music_group ("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "music_id CHAR(64) NOT NULL,"
                        "group_id CHAR(64) NOT NULL,"
                        "FOREIGN KEY(music_id) REFERENCES music_data(id) ON DELETE CASCADE,"
                        "FOREIGN KEY(group_id) REFERENCES group_data(id) ON DELETE CASCADE,"
                        "UNIQUE(music_id, group_id)"  // 确保同一首歌在一个歌单中只出现一次
                        ");";

    if (!SqliteManager::getInstance().executeSql(createSql)) {
        m_lastError = "创建音乐歌单关联表失败: " + SqliteManager::getInstance().getLastError();
        qCritical() << m_lastError << "SQL:" << createSql;
        return false;
    }
    qDebug() << "Created music_group table successfully";
    return true;
}

bool MusicImporter::updateMusicSortIdAndPath(const QString& musicId, int sortId, const QString& newFilePath)
{
    QString updateSql = "UPDATE music_data SET sort_id = ?, file_path = ? WHERE id = ?";
    if (!SqliteManager::getInstance().executeSql(updateSql, {sortId, newFilePath, musicId})) {
        m_lastError = "更新音乐排序ID和文件路径失败: " + SqliteManager::getInstance().getLastError();
        qWarning() << m_lastError << "SQL:" << updateSql;
        return false;
    }
    qDebug() << "Updated music sort ID and path - Music ID:" << musicId << "New sort ID:" << sortId << "New path:" << newFilePath;
    return true;
}

bool MusicImporter::addMusicToGroup(const QString& musicId, const QString& groupId)
{
    // 检查关联是否已存在
    QString checkSql = "SELECT id FROM music_group WHERE music_id = ? AND group_id = ?";
    auto result = SqliteManager::getInstance().querySql(checkSql, {musicId, groupId});
    if (!result.isEmpty()) {
        qDebug() << "Music already in group - Music ID:" << musicId << "Group ID:" << groupId;
        return true;  // 已存在视为成功
    }

    // 插入新的关联记录（不含排序ID）
    QString insertSql = "INSERT INTO music_group (music_id, group_id) VALUES (?, ?)";
    if (!SqliteManager::getInstance().executeSql(insertSql, {musicId, groupId})) {
        m_lastError = "添加音乐到歌单失败: " + SqliteManager::getInstance().getLastError();
        qWarning() << m_lastError << "SQL:" << insertSql;
        return false;
    }
    qDebug() << "Added new music-group relation - Music ID:" << musicId << "Group ID:" << groupId;
    return true;
}

int MusicImporter::getMaxSortIdInGroup(const QString& groupId)
{
    // 关联查询获取指定歌单中所有音乐的最大排序ID
    QString sql = "SELECT MAX(m.sort_id) AS max_sort "
                  "FROM music_data m "
                  "JOIN music_group mg ON m.id = mg.music_id "
                  "WHERE mg.group_id = ?";

    auto result = SqliteManager::getInstance().querySql(sql, {groupId});

    if (result.isEmpty() || result[0]["max_sort"].isNull()) {
        qDebug() << "No existing music in group, starting sort ID from 1 - Group ID:" << groupId;
        return 0;  // 没有音乐时从1开始
    }

    int maxSort = result[0]["max_sort"].toInt();
    qDebug() << "Found max sort ID in group - Group ID:" << groupId << "Max sort ID:" << maxSort;
    return maxSort;
}
