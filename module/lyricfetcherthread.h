#ifndef LYRICFETCHERTHREAD_H
#define LYRICFETCHERTHREAD_H

#include <QThread>
#include <QString>
#include <QList>
// 引入 QQMusicApi 头文件（用于调用封装后的接口）
#include "QQMusicApi.h"

// 假设 MusicDetail 结构体在 Tm_musicPlayListModel.h 中定义，此处仅作兼容声明
struct MusicDetail;
class Tm_musicPlayListModel;

class LyricFetcherThread : public QThread
{
    Q_OBJECT

public:
    // 单例模式接口（不变）
    static LyricFetcherThread* getInstance();

    // 获取当前歌词数据（不变）
    LyricData getMusicLyric() const;

signals:
    // 歌词更新信号（不变）
    void lyricUpdated(const LyricData& lyricData);
    // 封面更新信号（不变）
    void imgUpdated(const QString& localMusicId, const QString& imgPath);

public slots:
    // 设置目标音乐ID（触发歌词获取，不变）
    void setMusicID(const QString& musicId);

protected:
    // 线程执行入口（不变）
    void run() override;

private:
    // 私有构造函数（单例模式，不变）
    LyricFetcherThread(QObject *parent = nullptr);
    // 私有析构函数（不变）
    ~LyricFetcherThread() override;

    // 禁止拷贝构造和赋值（单例模式安全保障，不变）
    LyricFetcherThread(const LyricFetcherThread&) = delete;
    LyricFetcherThread& operator=(const LyricFetcherThread&) = delete;

    // ------------------------------ 原有函数调整 ------------------------------
    // 1. 从数据库获取歌词（逻辑不变，保留声明）
    LyricData fetchLyricFromDB(const QString& musicId);
    // 2. 从 API 获取歌词（核心调整：内部改用 QQMusicApi，不再依赖网络请求）
    void fetchLyricFromAPI(const QString& localMusicId);
    // 3. 下载专辑封面（逻辑不变，保留声明）
    QString getAlbumImgByID(const QString& albumID, const QString &imgURL);
    // 4. 保存歌词到数据库（逻辑不变，保留声明）
    bool saveLyricToDB(const LyricData& lyricData, QString musicid);
    // 5. 校验歌词有效性（逻辑不变，保留声明）
    bool isLyricValid(const LyricData& lyricData);
    // 6. 计算关键词匹配度（逻辑不变，保留声明）
    int calculateMatchDegree(const QString& resultStr, const QString& keyword);

    // ------------------------------ 移除的函数 ------------------------------
    // 1. 同步网络请求（sendSyncRequest）：因改用 QQMusicApi，无需暴露网络细节
    // 2. 按ID获取歌词（searchLyricByID）：功能整合到 fetchLyricFromAPI + QQMusicApi::getLyric
    // 3. 解析API响应（parseLyricApiResponse）：由 QQMusicApi 内部处理解析逻辑

    // ------------------------------ 成员变量调整 ------------------------------
    QString m_targetMusicId;       // 目标音乐ID（不变）
    bool m_needFetch;              // 是否需要获取歌词（不变）
    bool m_isFetching;             // 是否正在获取歌词（不变）
    LyricData m_currentLyricData;  // 当前歌词数据（不变）
    bool m_musicNeedImg;           // 是否需要下载封面（不变）
    QString m_bestImgUrl;          // 最优封面URL（不变）

    // 核心替换：移除 QNetworkAccessManager* m_networkManager，改用 QQMusicApi 实例
    QQMusicApi* m_qqMusicApi;
};

#endif // LYRICFETCHERTHREAD_H
