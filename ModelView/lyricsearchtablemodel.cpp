#include "LyricSearchTableModel.h"
#include "SqliteManager.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QtConcurrent/QtConcurrent>
#include "GlobalConfig.h"
// 引入 QQMusicApi 头文件
#include "QQMusicApi.h"

// 初始化静态成员变量
LyricSearchTableModel* LyricSearchTableModel::m_instance = nullptr;
QMutex LyricSearchTableModel::m_instanceMutex;

// 1. 单例模式实现（线程安全）
LyricSearchTableModel* LyricSearchTableModel::getInstance()
{
    if (m_instance == nullptr) {
        QMutexLocker locker(&m_instanceMutex); // 加锁确保线程安全
        if (m_instance == nullptr) {
            m_instance = new LyricSearchTableModel();
        }
    }
    return m_instance;
}

// 2. 构造函数（初始化线程和 QQMusicApi）
LyricSearchTableModel::LyricSearchTableModel(QObject* parent)
    : QAbstractTableModel(parent)
    // 移除网络管理器，初始化 QQMusicApi 实例
    , m_qqMusicApi(new QQMusicApi())
{
    // 初始化子线程并将模型移至子线程
    m_workerThread = new QThread(this);
    this->moveToThread(m_workerThread);
    m_workerThread->start();

    // 连接内部搜索信号与实际搜索函数（线程内调用）
    connect(this, &LyricSearchTableModel::startSearch,
            this, &LyricSearchTableModel::doSearch,
            Qt::QueuedConnection); // 队列连接确保子线程执行
}

// 3. 析构函数（释放资源与停止线程）
LyricSearchTableModel::~LyricSearchTableModel()
{
    // 停止子线程（安全退出）
    if (m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }

    // 释放 QQMusicApi 实例（替代原网络管理器清理）
    delete m_qqMusicApi;
    m_qqMusicApi = nullptr;

    // 释放子线程
    delete m_workerThread;
    m_workerThread = nullptr;
}

// 4. 重写模型行数（返回歌曲数量）
int LyricSearchTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return m_songList.size();
}

// 5. 重写模型列数（返回固定4列：歌名、作者、专辑、发布时间）
int LyricSearchTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return static_cast<int>(SongTableColumn::Column_Count);
}

// 6. 重写数据接口（返回单元格数据）
QVariant LyricSearchTableModel::data(const QModelIndex& index, int role) const
{
    // 校验索引有效性
    if (!index.isValid()
        || index.row() >= m_songList.size()
        || index.column() >= static_cast<int>(SongTableColumn::Column_Count)) {
        return QVariant();
    }

    const musicData& song = m_songList[index.row()];

    // 仅处理显示角色（可扩展为对齐、颜色等角色）
    if (role == Qt::DisplayRole) {
        switch (static_cast<SongTableColumn>(index.column())) {
        case SongTableColumn::Column_SongName:
            return song.name;
        case SongTableColumn::Column_Singer:
            return song.auther;
        case SongTableColumn::Column_Album:
            return song.album;
        case SongTableColumn::Column_ReleaseTime:
            return song.time.isEmpty() ? "未知" : song.time;
        default:
            return QVariant();
        }
    }

    return QVariant();
}

// 7. 重写表头数据（设置列标题）
QVariant LyricSearchTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // 仅处理水平表头（列标题）和显示角色
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (static_cast<SongTableColumn>(section)) {
        case SongTableColumn::Column_SongName:
            return "歌名";
        case SongTableColumn::Column_Singer:
            return "作者";
        case SongTableColumn::Column_Album:
            return "专辑";
        case SongTableColumn::Column_ReleaseTime:
            return "发布时间";
        default:
            return QVariant();
        }
    }

    return QVariant();
}

// 8. 外部搜索接口（供主线程调用，通过信号触发子线程搜索）
void LyricSearchTableModel::searchSongsByKeyword(const QString& keyword)
{
    beginResetModel();  // 通知视图：数据即将重置（清空）
    m_songList.clear(); // 清空数据源列表
    endResetModel();    // 通知视图：数据已重置（视图会刷新为空）

    if (keyword.isEmpty()) {
        qWarning() << "[LyricSearchModel] 搜索关键词为空";
        emit searchFinished(QList<musicData>()); // 返回空结果
        return;
    }

    qDebug() << "[LyricSearchModel] 开始搜索关键词:" << keyword;
    emit startSearch(keyword); // 发送信号，由子线程执行doSearch
}

// 9. 搜索结果处理（更新模型数据并通知视图）
void LyricSearchTableModel::onSearchFinished(const QList<musicData>& songList)
{
    // 模型数据更新必须先通知视图开始重置
    beginResetModel();
    m_songList = songList; // 替换数据源
    endResetModel();       // 通知视图数据已更新

    qDebug() << "[LyricSearchModel] 搜索完成，找到" << songList.size() << "首歌曲";
    emit searchFinished(songList); // 对外发送结果信号
}

// 10. 核心搜索逻辑（运行在子线程，改用 QQMusicApi）
void LyricSearchTableModel::doSearch(const QString& keyword)
{
    QList<musicData> resultList;

    // 调用 QQMusicApi::searchMusic 获取歌曲列表（替代原双API搜索）
    QList<musicData> apiResultList = m_qqMusicApi->searchMusic(keyword);
    if (!apiResultList.isEmpty()) {
        // 将 QQMusicApi 返回的 musicData 转换为本地 SongInfo
        for (const auto& apiData : std::as_const(apiResultList)) {
            // 过滤无效数据（mid为空表示无效歌曲）
            if (!apiData.mid.isEmpty()) {
                resultList.append(apiData);
            }
        }
        qDebug() << "[LyricSearchModel] QQMusicApi搜索成功，返回" << resultList.size() << "条结果";
    } else {
        qWarning() << "[LyricSearchModel] QQMusicApi未搜索到歌曲，关键词:" << keyword;
    }

    // 返回最终结果（空结果表示搜索失败）
    // QMetaObject::invokeMethod(this, "onSearchFinished",
    //                           Qt::QueuedConnection,
    //                           Q_ARG(QList<musicData>, resultList));
    onSearchFinished(resultList);
}

// 11. 通过表格索引获取歌词（外部调用入口，逻辑不变）
void LyricSearchTableModel::fetchLyricByIndex(const QModelIndex& index)
{
    // 1. 校验索引有效性（必须是有效行、属于当前模型、非表头）
    if (!index.isValid()
        || index.row() >= m_songList.size()
        || index.model() != this
        || index.column() < 0) {
        QString errMsg = "[LyricSearchModel] 无效的表格索引，无法获取歌词";
        qWarning() << errMsg;
        emit lyricFetchFailed(errMsg);
        return;
    }

    // 2. 从索引行获取对应的歌曲信息
    const musicData& targetSong = m_songList[index.row()];
    if (targetSong.mid.isEmpty()) {
        QString errMsg = "[LyricSearchModel] 歌曲ID为空，无法获取歌词";
        qWarning() << errMsg;
        emit lyricFetchFailed(errMsg);
        return;
    }

    LyricData cacheLyric = queryLyricFromCache(targetSong.mid);
    if (!cacheLyric.yrc.isEmpty()) {  // 缓存有效（标准歌词非空）
        emit lyricFetched(cacheLyric);
        return;
    }

    qDebug() << "[LyricSearchModel] 开始获取歌词，歌曲:" << targetSong.name
             << "| 歌手:" << targetSong.auther << "| mid:" << targetSong.mid;

    // 3. 调用核心歌词获取逻辑（子线程内直接调用）
    doFetchLyric(targetSong);
}

void LyricSearchTableModel::doFetchLyric(const musicData& songInfo, bool emitPreview)
{
    LyricData resultLyric;
    bool lyricFetchedFaLg = false;

    // 调用 QQMusicApi::getLyric 获取歌词（替代原双API歌词请求）
    LyricData apiLyric = m_qqMusicApi->getLyric(songInfo.mid);
    // 映射 QQMusicApi 返回的 LyricData 到本地 searchLyricData
    resultLyric.trans = apiLyric.trans;
    resultLyric.yrc = apiLyric.yrc;
    resultLyric.roma = apiLyric.roma;

    // 标记为获取成功（lrc非空即有效）
    if (!resultLyric.yrc.isEmpty()) {
        lyricFetchedFaLg = true;
        qDebug() << "[LyricSearchModel] QQMusicApi获取歌词成功";
    } else {
        qWarning() << "[LyricSearchModel] QQMusicApi返回歌词无效，mid:" << songInfo.mid;
    }

    // 网络请求成功后，写入缓存
    if (lyricFetchedFaLg) {
        saveLyricToCache(resultLyric, songInfo.mid);  // 写入缓存（成功/失败不影响返回）
        if (emitPreview) emit lyricFetched(resultLyric); // 正常返回歌词
    } else {
        // 网络请求失败，返回错误
        QString errMsg = QString("[LyricSearchModel] 歌词获取失败，歌曲:%1 | 歌手:%2")
                             .arg(songInfo.name).arg(songInfo.auther);
        qWarning() << errMsg;
        if (emitPreview) emit lyricFetchFailed(errMsg);
    }
}

// 13. 从缓存查询歌词（逻辑不变）
LyricData LyricSearchTableModel::queryLyricFromCache(const QString& songMid)
{
    LyricData cacheLyric;
    SqliteManager& dbMgr = SqliteManager::getInstance();

    // 1. 带参数查询（防止SQL注入，必须用参数化查询）
    QString querySql = R"(
        SELECT mid, trans, yrc, roma
        FROM online_music_lrc
        WHERE mid = ?;
    )";
    QVector<QVariant> params = { songMid };
    QVector<QMap<QString, QVariant>> result = dbMgr.querySql(querySql, params);

    // 2. 解析查询结果（若存在则填充LyricData）
    if (!result.isEmpty()) {
        QMap<QString, QVariant> row = result.first();
        cacheLyric.trans = row["trans"].toString();
        cacheLyric.yrc = row["yrc"].toString();
        cacheLyric.roma = row["roma"].toString();
        qDebug() << "[LyricSearchModel] 缓存命中，歌曲mid:" << songMid;
    } else {
        qDebug() << "[LyricSearchModel] 缓存未命中，歌曲mid:" << songMid;
    }

    return cacheLyric;
}

// 14. 歌词写入缓存（逻辑不变）
bool LyricSearchTableModel::saveLyricToCache(const LyricData& lyric, QString mid)
{
    SqliteManager& dbMgr = SqliteManager::getInstance();
    // 开启事务（确保原子性：不存在则插入，存在则更新）
    if (!dbMgr.beginTransaction()) {
        qWarning() << "[LyricSearchModel] 开启事务失败：" << dbMgr.getLastError();
        return false;
    }

    // 1. 先尝试删除旧数据（避免主键冲突）
    QString deleteSql = "DELETE FROM online_music_lrc WHERE mid = ?;";
    QVector<QVariant> deleteParams = { mid };
    if (!dbMgr.executeSql(deleteSql, deleteParams)) {
        dbMgr.rollbackTransaction();  // 失败回滚
        qWarning() << "[LyricSearchModel] 删除旧缓存失败：" << dbMgr.getLastError();
        return false;
    }

    // 2. 插入新数据
    QString insertSql = R"(
        INSERT INTO online_music_lrc (mid, trans, yrc, roma)
        VALUES (?, ?, ?, ?);
    )";
    QVector<QVariant> insertParams = {
        mid,
        lyric.trans,
        lyric.yrc,
        lyric.roma
    };
    if (!dbMgr.executeSql(insertSql, insertParams)) {
        dbMgr.rollbackTransaction();  // 失败回滚
        qWarning() << "[LyricSearchModel] 插入新缓存失败：" << dbMgr.getLastError();
        return false;
    }

    // 3. 提交事务
    if (!dbMgr.commitTransaction()) {
        qWarning() << "[LyricSearchModel] 提交事务失败：" << dbMgr.getLastError();
        return false;
    }

    qDebug() << "[LyricSearchModel] 歌词缓存写入成功，mid:" << mid;
    return true;
}

// 15. 歌词写入music_lrc表（逻辑不变，仅封面下载保留临时网络请求）
bool LyricSearchTableModel::saveLyricToMusicLrcTable(const QString& music_id, int itemIndex)
{
    // 1. 基础参数校验（避免无效操作）
    if (music_id.isEmpty()) {
        qWarning() << "[LyricSearchModel] music_id为空，无法写入music_lrc表";
        return false;
    }
    if (itemIndex < 0 || itemIndex >= m_songList.size()) {
        qWarning() << "[LyricSearchModel] 条目索引无效（范围：0~" << m_songList.size()-1
                   << "），当前索引：" << itemIndex;
        return false;
    }

    // 2. 获取条目对应的歌曲信息（从搜索结果列表m_songList中提取）
    const musicData& targetSong = m_songList[itemIndex];
    if (targetSong.mid.isEmpty()) {
        qWarning() << "[LyricSearchModel] 条目" << itemIndex << "的歌曲mid为空，无法获取歌词";
        return false;
    }

    // 封面下载（保留原逻辑，使用临时网络管理器）
    QString imgUrl = targetSong.cover;
    QString coverPath = downloadAlbumCover(music_id, imgUrl);
    if (coverPath.isEmpty()) {
        qWarning() << "封面下载失败，ID:" << music_id;
    } else {
        // 更新music_data表中的img_name字段
        QString sql = "UPDATE music_data SET img_name = ? WHERE id = ?";
        QVector<QVariant> params;
        params << coverPath << music_id;
        SqliteManager::getInstance().executeSql(sql, params);
    }

    // 3. 获取歌词数据（优先查缓存，缓存未命中则通过 QQMusicApi 获取）
    LyricData lyricData = queryLyricFromCache(targetSong.mid);  // 先查本地缓存
    if (lyricData.yrc.isEmpty()) {  // 缓存未命中，通过 QQMusicApi 获取歌词
        qDebug() << "[LyricSearchModel] 条目" << itemIndex << "缓存未命中，开始通过QQMusicApi获取歌词";
        LyricData apiLyric = m_qqMusicApi->getLyric(targetSong.mid);
        // 映射并处理歌词
        lyricData.trans = apiLyric.trans;
        lyricData.yrc = apiLyric.yrc;
        lyricData.roma = apiLyric.roma;
        // if (lyricData.lrc.isEmpty() && !lyricData.yrc.isEmpty()) {
        //     LrcParser parser;
        //     parser.parseFromString(lyricData.yrc);
        //     lyricData.lrc = parser.toStandardLrcString();
        // }
        // 写入缓存（后续可复用）
        if (!lyricData.yrc.isEmpty()) {
            saveLyricToCache(lyricData, targetSong.mid);
        }
    }

    // 校验歌词有效性
    if (lyricData.yrc.isEmpty()) {
        qWarning() << "[LyricSearchModel] 条目" << itemIndex << "获取歌词失败，无法写入music_lrc表";
        emit lyricSetError("获取歌词失败，无法写入，请稍后再试");
        return false;
    }

    // 4. 写入music_lrc表（使用事务确保“覆盖”原子性：先删后插）
    SqliteManager& dbMgr = SqliteManager::getInstance();
    // 开启事务（失败直接返回）
    if (!dbMgr.beginTransaction()) {
        qWarning() << "[LyricSearchModel] 开启事务失败：" << dbMgr.getLastError();
        return false;
    }

    // 4.1 先删除已有记录（实现“覆盖”效果）
    QString deleteSql = "DELETE FROM music_lrc WHERE id = ?;";  // 根据music_id删除
    QVector<QVariant> deleteParams = { music_id };
    if (!dbMgr.executeSql(deleteSql, deleteParams)) {
        dbMgr.rollbackTransaction();  // 删除失败回滚
        qWarning() << "[LyricSearchModel] 删除music_lrc表中id=" << music_id << "的记录失败：" << dbMgr.getLastError();
        return false;
    }

    // 4.2 插入新记录（使用music_id作为主键，歌词数据来自lyricData）
    QString insertSql = R"(
        INSERT INTO music_lrc (id, trans, yrc, roma)
        VALUES (?, ?, ?, ?);
    )";
    QVector<QVariant> insertParams = {
        music_id,               // 传入的主键id
        lyricData.trans,        // 翻译歌词（空则存空字符串）
        lyricData.yrc,          // 逐字歌词（空则存空字符串）
        lyricData.roma          // 罗马音歌词（空则存空字符串）
    };
    if (!dbMgr.executeSql(insertSql, insertParams)) {
        dbMgr.rollbackTransaction();  // 插入失败回滚
        qWarning() << "[LyricSearchModel] 向music_lrc表插入id=" << music_id << "的记录失败：" << dbMgr.getLastError();
        return false;
    }

    // 4.3 提交事务（所有操作成功）
    if (!dbMgr.commitTransaction()) {
        qWarning() << "[LyricSearchModel] 提交事务失败：" << dbMgr.getLastError();
        return false;
    }

    // 操作成功
    qDebug() << "[LyricSearchModel] 成功将条目" << itemIndex << "的歌词写入music_lrc表，id=" << music_id;
    emit lyricSetSuccess();
    return true;
}

// 16. 封面下载（保留原逻辑，使用临时网络管理器）
QString LyricSearchTableModel::downloadAlbumCover(const QString& musicId, const QString& imgUrl)
{
    if (musicId.isEmpty() || imgUrl.isEmpty()) {
        qWarning() << "下载封面参数无效";
        return "";
    }

    // 确保封面保存目录存在
    QString coverDir = GlobalConfig::getInstance().getValue("App/DBpath","").toString() + "/AudioCovers/";
    QDir dir(coverDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "无法创建封面目录:" << coverDir;
        return "";
    }

    // 构建封面保存路径（用音乐ID作为文件名，避免重复）
    QString coverPath = coverDir + musicId + ".jpg";

    // 若文件已存在，直接返回路径
    if (QFile::exists(coverPath)) {
        qDebug() << "封面已存在:" << coverPath;
        return coverPath;
    }

    // 发送下载请求（使用临时网络管理器，避免依赖全局网络对象）
    QNetworkAccessManager tempManager;
    QNetworkRequest request((QUrl(imgUrl)));
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");

    QNetworkReply* reply = tempManager.get(request);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    // 处理下载结果
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "封面下载错误:" << reply->errorString() << "URL:" << imgUrl;
        reply->deleteLater();
        return "";
    }

    QByteArray imgData = reply->readAll();
    reply->deleteLater();

    if (imgData.isEmpty()) {
        qWarning() << "下载的封面数据为空，URL:" << imgUrl;
        return "";
    }

    // 保存封面到本地
    QFile file(coverPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法打开封面文件:" << file.errorString() << "路径:" << coverPath;
        return "";
    }

    qint64 bytesWritten = file.write(imgData);
    file.close();

    if (bytesWritten != imgData.size()) {
        qWarning() << "封面保存不完整，预期:" << imgData.size() << "实际:" << bytesWritten;
        QFile::remove(coverPath);
        return "";
    }

    return coverPath;
}
