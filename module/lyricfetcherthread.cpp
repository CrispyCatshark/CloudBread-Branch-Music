#include "LyricFetcherThread.h"
#include <QDebug>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <qdir.h>
#include "SqliteManager.h"
#include "MusicPlayer.h"
#include "Tm_musicPlayListModel.h"
#include <QStandardPaths>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include "GlobalConfig.h"
// 引入 QQMusicApi 头文件
#include "QQMusicApi.h"

LyricFetcherThread* LyricFetcherThread::getInstance()
{
    static LyricFetcherThread *instance = new LyricFetcherThread();
    return instance;
}

LyricFetcherThread::LyricFetcherThread(QObject *parent)
    : QThread(parent),
    m_needFetch(false),
    m_isFetching(false),
    // 移除网络管理器，初始化 QQMusicApi 实例
    m_qqMusicApi(new QQMusicApi())
{
    connect(MusicPlayer::getInstance(), &MusicPlayer::lyricSwitched,
            this, &LyricFetcherThread::setMusicID);
}

LyricFetcherThread::~LyricFetcherThread()
{
    // 设置停止标记
    {
        m_needFetch = false;
        m_targetMusicId.clear();
        m_isFetching = false;
    }

    requestInterruption();
    wait();

    // 清理 QQMusicApi 实例（替代原网络管理器清理）
    if (m_qqMusicApi)
    {
        delete m_qqMusicApi;
        m_qqMusicApi = nullptr;
    }
}

void LyricFetcherThread::setMusicID(const QString& musicId)
{
    // 避免重复获取同一首歌的歌词或在获取中时重复设置
    if ((m_targetMusicId == musicId && m_needFetch) || m_isFetching || musicId.isEmpty())
        return;

    m_targetMusicId = musicId;
    m_needFetch = true;
}

LyricData LyricFetcherThread::getMusicLyric() const
{
    return m_currentLyricData;
}

void LyricFetcherThread::run()
{
    while (!isInterruptionRequested())
    {
        QString targetId;
        bool needFetch = false;

        // 临界区：获取目标ID和触发状态
        {
            if (m_needFetch && !m_targetMusicId.isEmpty() && !m_isFetching)
            {
                targetId = m_targetMusicId;
                needFetch = true;
                m_needFetch = false;
                m_isFetching = true;
            }
        }

        // 执行歌词获取流程
        if (needFetch && !targetId.isEmpty())
        {
            qDebug() << "开始获取歌词，musicId:" << targetId;
            LyricData resultData;

            // 步骤1：先查数据库
            resultData = fetchLyricFromDB(targetId);
            if (!resultData.yrc.isEmpty())
            {
                qDebug() << "数据库中存在歌词，直接返回";
                {
                    m_currentLyricData = resultData;
                    m_isFetching = false;
                }
                emit lyricUpdated(resultData);
            }
            // 步骤2：数据库无数据，调用 QQMusicApi 获取（替代原API调用）
            else
            {
                qDebug() << "数据库中无歌词，调用QQ音乐API获取";
                fetchLyricFromAPI(targetId);
            }
        }

        msleep(100);
    }
}

LyricData LyricFetcherThread::fetchLyricFromDB(const QString& musicId)
{
    // 原数据库查询逻辑不变
    LyricData data;
    if (!SqliteManager::getInstance().tableExists("music_lrc"))
    {
        qWarning() << "music_lrc表不存在";
        return data;
    }

    QString sql = "SELECT id, trans, yrc, roma FROM music_lrc WHERE id = ?";
    QVector<QVariant> params;
    params << musicId;

    auto queryResult = SqliteManager::getInstance().querySql(sql, params);
    if (queryResult.isEmpty())
    {
        return data;
    }

    const auto& rowMap = queryResult.first();
    data.trans = rowMap["trans"].toString();
    data.yrc = rowMap["yrc"].toString();
    data.roma = rowMap["roma"].toString();

    return data;
}

void LyricFetcherThread::fetchLyricFromAPI(const QString& localMusicId)
{
    // 移除网络管理器校验，改为校验 QQMusicApi 实例
    if (!m_qqMusicApi || localMusicId.isEmpty())
    {
        qWarning() << "QQMusicApi未初始化或本地音乐ID为空，无法发起请求";
        {
            m_isFetching = false;
        }
        return;
    }

    // 1. 获取本地音乐详情（用于构造搜索关键词）
    QString keyword;
    MusicDetail music = Tm_musicPlayListModel::getInstance()->getMusicDetail(localMusicId);
    keyword = music.name;
    if (music.auther != "未知创作者") {
        keyword = music.name + "-" + music.auther;
    }

    m_musicNeedImg = (music.imgName.isEmpty() || music.imgName==":/cloudbread/src/favicon_round.png");
    qDebug() << "搜索关键词:" << keyword;

    // 2. 调用 QQMusicApi::searchMusic 获取歌曲列表（替代原双API搜索）
    QList<musicData> searchResults = m_qqMusicApi->searchMusic(keyword);
    QString songmid;
    if (!searchResults.isEmpty())
    {
        // 沿用原匹配度逻辑，筛选最优结果
        QString mid;
        // int maxMatchDegree = -1;
        QString cover;

        musicData result = searchResults.first();
        mid = result.mid;
        cover = result.cover;

        /*for (const auto& result : std::as_const(searchResults))
        {
            // 构造「歌名-作者」字符串用于匹配
            QString songKey = result.name + "-" + result.auther;
            int currentDegree = calculateMatchDegree(songKey, keyword);
            qDebug() << "QQMusicApi搜索结果 - " << songKey << " 匹配度:" << currentDegree;

            // 更新最优结果
            if (currentDegree > maxMatchDegree)
            {
                maxMatchDegree = currentDegree;
                bestSongmid = result.mid;
                bestAlbumId = result.album; // 假设 musicData 的 album 字段存储专辑ID
            }
        }

        // 确认最优结果有效
        if (!bestSongmid.isEmpty() && maxMatchDegree > 0)
        {
            songmid = bestSongmid;
            qDebug() << "QQMusicApi最优结果 - songmid:" << songmid << " 最高匹配度:" << maxMatchDegree;

            // 下载封面（沿用原逻辑，使用搜索结果中的 cover 字段）
            if (m_musicNeedImg)
            {
                // 遍历找到最优结果对应的封面URL
                for (const auto& result : std::as_const(searchResults))
                {
                    if (result.mid == songmid && !result.cover.isEmpty())
                    {
                        emit imgUpdated(localMusicId, getAlbumImgByID(localMusicId, result.cover));
                        qDebug() << "QQMusicApi最优结果封面:" << result.cover;
                        break;
                    }
                }
            }
        }
        else
        {
            qWarning() << "QQMusicApi无有效匹配结果，最高匹配度:" << maxMatchDegree;
        }*/

        QString resultKey = QString("%1-%2").arg(result.name, result.auther);
        qDebug() << "搜索到:" << mid << " | cover:" << cover << " | 结果关键词：" << resultKey;
        if (m_musicNeedImg)
        {
            // 遍历找到最优结果对应的封面URL
            for (const auto& result : std::as_const(searchResults))
            {
                if (result.mid == songmid && !result.cover.isEmpty())
                {
                    emit imgUpdated(localMusicId, getAlbumImgByID(localMusicId, result.cover));
                    qDebug() << "QQMusicApi最优结果封面:" << result.cover;
                    break;
                }
            }
        }
    }
    else
    {
        qWarning() << "QQMusicApi未搜索到歌曲，关键词:" << keyword;
    }

    // 3. 调用 QQMusicApi::getLyric 获取歌词（替代原双API歌词获取）
    LyricData resultData;
    if (!songmid.isEmpty())
    {
        resultData = m_qqMusicApi->getLyric(songmid);
        saveLyricToDB(resultData, localMusicId);
    }

    // 更新状态并发送结果
    {
        m_currentLyricData = resultData;
        m_isFetching = false;
    }
    emit lyricUpdated(resultData);
}

// 以下函数逻辑不变（封面下载、歌词验证、数据库保存等业务逻辑无需修改）
QString LyricFetcherThread::getAlbumImgByID(const QString& musicid ,const QString& imgURL)
{
    // 原封面下载逻辑不变
    if (musicid.isEmpty()) {
        qWarning() << "专辑ID为空，无法获取封面图片";
        return QString();
    }

    QString coverDir = GlobalConfig::getInstance().getValue("App/DBpath","").toString() + "/AudioCovers/";
    QDir dir(coverDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "无法创建封面保存目录:" << coverDir;
        return QString();
    }

    QString coverPath = coverDir + musicid + ".jpg";

    if (QFile::exists(coverPath)) {
        qDebug() << "封面图片已存在:" << coverPath;
        return coverPath;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(imgURL);
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36");

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "封面图片下载失败:" << reply->errorString() << "URL:" << imgURL;
        reply->deleteLater();
        return QString();
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        qWarning() << "下载的封面图片数据为空，URL:" << imgURL;
        return QString();
    }

    QFile file(coverPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法打开文件写入封面图片:" << file.errorString() << "路径:" << coverPath;
        return QString();
    }

    qint64 bytesWritten = file.write(data);
    file.close();

    if (bytesWritten != data.size()) {
        qWarning() << "封面图片保存不完整，预期写入" << data.size() << "字节，实际写入" << bytesWritten << "字节";
        QFile::remove(coverPath);
        return QString();
    }

    qDebug() << "封面图片下载成功，保存路径:" << coverPath;
    return coverPath;
}

bool LyricFetcherThread::saveLyricToDB(const LyricData& lyricData, QString musicid)
{
    // 原数据库保存逻辑不变
    if (musicid.isEmpty())
    {
        qWarning() << "歌曲ID为空无法保存歌词";
        return false;
    }

    QString sql = "INSERT OR REPLACE INTO music_lrc (id, trans, yrc, roma) "
                  "VALUES (?, ?, ?, ?)";
    QVector<QVariant> params;
    params << musicid
           << lyricData.trans
           << lyricData.yrc
           << lyricData.roma;

    return SqliteManager::getInstance().executeSql(sql, params);
}

bool LyricFetcherThread::isLyricValid(const LyricData& lyricData)
{
    if (lyricData.yrc.isEmpty())
        return false;

    return true;
}

int LyricFetcherThread::calculateMatchDegree(const QString& resultStr, const QString& keyword)
{
    // 原匹配度计算逻辑不变
    if (resultStr.isEmpty() || keyword.isEmpty()) {
        return 0;
    }

    QString lowerResult = resultStr.toLower();
    QString lowerKeyword = keyword.toLower();

    int matchCount = 0;
    QSet<QChar> keywordChars;
    for (const QChar& c : std::as_const(lowerKeyword)) {
        keywordChars.insert(c);
    }

    for (const QChar& c : keywordChars) {
        if (lowerResult.contains(c)) {
            matchCount++;
        }
    }

    return static_cast<int>((static_cast<double>(matchCount) / lowerKeyword.size()) * 100);
}
