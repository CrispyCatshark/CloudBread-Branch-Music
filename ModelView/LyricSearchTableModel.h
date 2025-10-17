#ifndef LYRICSEARCHTABLEMODEL_H
#define LYRICSEARCHTABLEMODEL_H

#include <QAbstractTableModel>
#include <QThread>
#include <QMutex>
#include <QVector>
#include <QString>
#include <QVariant>
#include <QModelIndex>
// 引入 QQMusicApi 头文件（用于调用封装后的接口）
#include "QQMusicApi.h"

// 表格列定义（与QTableView列索引对应，用户提供的枚举）
enum class SongTableColumn {
    Column_SongName = 0,    // 歌名列
    Column_Singer,          // 作者列
    Column_Album,           // 专辑列
    Column_ReleaseTime,     // 发布时间列
    Column_Count            // 列总数（用于计算列数）
};

class LyricSearchTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    // 单例模式接口（线程安全）
    static LyricSearchTableModel* getInstance();

    // 重写QAbstractTableModel纯虚函数
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 外部搜索接口（供主线程调用，触发歌曲搜索）
    void searchSongsByKeyword(const QString& keyword);

    // 通过表格索引获取歌词（外部调用入口，如QTableView选中行时）
    void fetchLyricByIndex(const QModelIndex& index);

    // 将歌词写入music_lrc表（覆盖已有记录）
    bool saveLyricToMusicLrcTable(const QString& music_id, int itemIndex);

signals:
    // 搜索完成信号（返回搜索到的歌曲列表）
    void searchFinished(const QList<musicData>& songList);
    // 歌词获取成功信号（返回歌词数据）
    void lyricFetched(const LyricData& lyricData);
    // 歌词获取失败信号（返回错误信息）
    void lyricFetchFailed(const QString& errorMsg);
    // 歌词写入music_lrc表成功信号
    void lyricSetSuccess();
    // 歌词写入music_lrc表失败信号（返回错误信息）
    void lyricSetError(const QString& errorMsg);

    // 内部信号：触发子线程执行搜索（避免跨线程直接调用）
    void startSearch(const QString& keyword);

protected:
    // 线程执行入口（子线程运行，处理搜索和歌词获取逻辑）
    void run() ;

private:
    // 私有构造函数（单例模式，禁止外部实例化）
    LyricSearchTableModel(QObject* parent = nullptr);
    // 私有析构函数（禁止外部销毁）
    ~LyricSearchTableModel() override;

    // 禁止拷贝构造和赋值（单例模式安全保障）
    LyricSearchTableModel(const LyricSearchTableModel&) = delete;
    LyricSearchTableModel& operator=(const LyricSearchTableModel&) = delete;

    // ------------------------------ 核心业务函数 ------------------------------
    // 1. 子线程搜索逻辑（调用QQMusicApi获取歌曲列表）
    void doSearch(const QString& keyword);
    // 2. 搜索结果处理（更新模型数据并通知视图）
    void onSearchFinished(const QList<musicData>& songList);
    // 3. 子线程歌词获取逻辑（调用QQMusicApi获取歌词）
    // 12. 核心歌词获取逻辑（运行在子线程，改用 QQMusicApi）
    void doFetchLyric(const musicData& songInfo, bool emitPreview = true);
    // 4. 从缓存查询歌词（优先读取本地数据库）
    LyricData queryLyricFromCache(const QString& songMid);
    // 5. 歌词写入缓存（将获取到的歌词保存到本地数据库）
    bool saveLyricToCache(const LyricData& lyric, QString mid);
    // 6. 下载专辑封面（保存到本地并返回路径）
    QString downloadAlbumCover(const QString& musicId, const QString& imgUrl);

    // ------------------------------ 成员变量 ------------------------------
    static LyricSearchTableModel* m_instance;       // 单例实例
    static QMutex m_instanceMutex;                  // 单例互斥锁
    QThread* m_workerThread;                        // 子线程（处理搜索和歌词获取）
    QList<musicData> m_songList;                   // 模型数据源（歌曲列表）
    // 核心替换：移除QNetworkAccessManager，改用QQMusicApi实例
    QQMusicApi* m_qqMusicApi;
};

#endif // LYRICSEARCHTABLEMODEL_H
