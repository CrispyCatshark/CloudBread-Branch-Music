#ifndef MUSIC_IMPORTER_H
#define MUSIC_IMPORTER_H

#include <QStringList>
#include <QString>
#include <qthread.h>

class MusicImporter : public QObject
{
    Q_OBJECT
public:
    // 构造函数
    explicit MusicImporter(QObject *parent = nullptr);

    // 析构函数
    ~MusicImporter() = default;

    // 导入音乐文件到指定歌单
    // 参数: filePaths - 音乐文件路径列表, playlistId - 歌单ID
    // 返回值: 成功导入的数量
    int importMusic(const QStringList& filePaths, const QString& playlistId);

    // 获取最后一次错误信息
    QString getLastError() const;

signals:
    void importProgress(int importedCount, int totalCount);  // 进度更新（已导入/总数）
    void importFinished(int successCount, int totalCount);   // 导入完成（成功数/总数）
    void importError(const QString& errorMsg);               // 导入错误

private:
    // 生成音乐ID (使用文件路径的哈希值)
    QString generateMusicId(const QString& filePath, QString group_id);

    // 检查音乐是否已存在于数据库中
    bool isMusicExists(const QString& musicId);

    // 创建音乐与歌单的关联表(如果不存在)
    bool createMusicGroupRelationTable();

    // 添加音乐到歌单
    bool addMusicToGroup(const QString& musicId, const QString& groupId);

    bool updateMusicSortIdAndPath(const QString& musicId, int sortId, const QString& newFilePath);

    // 获取指定歌单中最大的排序ID
    int getMaxSortIdInGroup(const QString& groupId);

    // 错误信息
    QString m_lastError;
};

#endif // MUSIC_IMPORTER_H
