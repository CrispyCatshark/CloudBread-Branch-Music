#ifndef LYRICANDCOVERBATCHFETCHER_H
#define LYRICANDCOVERBATCHFETCHER_H

#include <QThread>
#include <QString>
#include <QStringList>
// 引入 QQMusicApi 头文件（用于调用封装后的接口）
#include "QQMusicApi.h"

class LyricAndCoverBatchFetcher : public QThread
{
    Q_OBJECT

public:
    // 单例模式接口（不变）
    static LyricAndCoverBatchFetcher* getInstance();

    // 启动批量补全任务（不变）
    void startBatchFetch();

signals:
    // 批量任务开始信号（参数：总待处理数量）
    void batchFetchStarted(int totalCount);
    // 批量任务进度信号（参数：当前进度、总数量、当前处理歌曲名）
    void batchFetchProgress(int current, int total, const QString& musicName);
    // 单首音乐处理结果信号（参数：音乐ID、是否成功、处理信息）
    void musicProcessed(const QString& musicId, bool success, const QString& message);
    // 批量任务结束信号（参数：成功数量、失败数量）
    void batchFetchFinished(int successCount, int failCount);

protected:
    // 线程执行入口（不变）
    void run() override;

private:
    // 私有构造函数（单例模式，不变）
    LyricAndCoverBatchFetcher(QObject *parent = nullptr);
    // 私有析构函数（不变）
    ~LyricAndCoverBatchFetcher() override;

    // 禁止拷贝构造和赋值（单例模式安全保障，不变）
    LyricAndCoverBatchFetcher(const LyricAndCoverBatchFetcher&) = delete;
    LyricAndCoverBatchFetcher& operator=(const LyricAndCoverBatchFetcher&) = delete;

    // ------------------------------ 原有函数调整 ------------------------------
    // 1. 获取待处理音乐ID列表（从数据库查询，逻辑不变）
    QStringList getMusicIdsToProcess();
    // 2. 处理单首音乐（核心逻辑，内部改用 QQMusicApi）
    bool processSingleMusic(const QString& musicId);
    // 3. 获取并保存歌词（调整为调用 QQMusicApi，不再依赖网络请求）
    bool fetchAndSaveLyric(const QString& musicId, const QString& songmid);
    // 4. 保存歌词到数据库（逻辑不变）
    bool saveLyricToDB(const LyricData& lyricData, QString musicID);
    // 5. 下载专辑封面（逻辑不变，使用临时网络管理器）
    QString downloadAlbumCover(const QString& musicId, const QString& imgUrl);
    // 6. 更新数据库中的封面路径（逻辑不变）
    bool updateMusicCoverInDB(const QString& musicId, const QString& coverPath);
    // 7. 计算关键词匹配度（逻辑不变，用于筛选最优歌曲结果）
    int calculateMatchDegree(const QString& resultStr, const QString& keyword);

    // ------------------------------ 移除的函数 ------------------------------
    // 1. 同步网络请求（sendSyncRequest）：因改用 QQMusicApi，无需暴露网络细节
    // 2. 按关键词搜索songmid（searchSongmidByKeyword）：功能整合到 processSingleMusic + QQMusicApi::searchMusic

    // ------------------------------ 成员变量调整 ------------------------------
    bool m_isRunning;              // 任务是否正在运行（不变）
    int m_fetchInterval = 1000;    // 成功后停顿间隔（默认5秒，不变）
    // 核心替换：移除 QNetworkAccessManager* m_networkManager，改用 QQMusicApi 实例
    QQMusicApi* m_qqMusicApi;
};

#endif // LYRICANDCOVERBATCHFETCHER_H
