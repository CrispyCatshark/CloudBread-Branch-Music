#include "LyricAndCoverBatchFetcher.h"
#include <QDebug>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include "LrcParser.h"
#include "SqliteManager.h"
#include "Tm_musicPlayListModel.h"
#include "GlobalConfig.h"
// 引入 QQMusicApi 头文件
#include "QQMusicApi.h"

LyricAndCoverBatchFetcher* LyricAndCoverBatchFetcher::getInstance()
{
    static LyricAndCoverBatchFetcher* instance = new LyricAndCoverBatchFetcher();
    return instance;
}

LyricAndCoverBatchFetcher::LyricAndCoverBatchFetcher(QObject *parent)
    : QThread(parent),
    m_isRunning(false),
    // 移除网络管理器，初始化 QQMusicApi 实例
    m_qqMusicApi(new QQMusicApi())
{
}

LyricAndCoverBatchFetcher::~LyricAndCoverBatchFetcher()
{
    // 停止任务
    m_isRunning = false;
    requestInterruption();
    wait();

    // 清理 QQMusicApi 实例（替代原网络管理器清理）
    if (m_qqMusicApi) {
        delete m_qqMusicApi;
        m_qqMusicApi = nullptr;
    }
}

void LyricAndCoverBatchFetcher::startBatchFetch()
{
    if (!isRunning()) {
        m_isRunning = true;
        start();
    } else {
        qDebug() << "批量补全任务已在运行中";
    }
}

void LyricAndCoverBatchFetcher::run()
{
    qDebug() << "=== 启动歌词与封面批量补全任务 ===";

    // 移除网络管理器初始化，改用已创建的 QQMusicApi 实例

    // 1. 获取待处理的音乐ID列表
    QStringList targetMusicIds = getMusicIdsToProcess();
    int totalCount = targetMusicIds.size();
    int successCount = 0;
    int failCount = 0;

    emit batchFetchStarted(totalCount);
    qDebug() << "待处理音乐总数:" << totalCount;

    if (totalCount == 0) {
        emit batchFetchFinished(0, 0);
        qDebug() << "无需要补全的音乐，任务结束";
        m_isRunning = false;
        return;
    }

    // 2. 遍历处理每首音乐
    for (int i = 0; i < targetMusicIds.size() && m_isRunning && !isInterruptionRequested(); ++i) {
        QString musicId = targetMusicIds[i];
        MusicDetail music = Tm_musicPlayListModel::getInstance()->getMusicDetail(musicId);
        QString musicInfo = QString("%1 - %2").arg(music.name).arg(music.auther);

        qDebug() << QString("\n处理第 %1/%2 首: %3").arg(i+1).arg(totalCount).arg(musicInfo);
        emit batchFetchProgress(i+1, totalCount, music.name);

        // 处理单首音乐
        bool processSuccess = processSingleMusic(musicId);
        if (processSuccess) {
            successCount++;
            emit musicProcessed(musicId, true, QString("处理成功: %1").arg(musicInfo));
            qDebug() << "处理成功，等待" << m_fetchInterval/1000 << "秒";
            // 成功后停顿指定时间
            for (int j = 0; j < m_fetchInterval/100 && m_isRunning; ++j) {
                msleep(100);
            }
        } else {
            failCount++;
            emit musicProcessed(musicId, false, QString("处理失败: %1").arg(musicInfo));
            qDebug() << "处理失败，继续下一首";
        }
    }

    // 3. 任务结束
    emit batchFetchFinished(successCount, failCount);
    qDebug() << QString("\n=== 批量补全任务结束 ===");
    qDebug() << QString("总处理数: %1, 成功: %2, 失败: %3").arg(totalCount).arg(successCount).arg(failCount);
    m_isRunning = false;
}

QStringList LyricAndCoverBatchFetcher::getMusicIdsToProcess()
{
    // 原数据库查询逻辑不变
    QStringList result;

    // 校验数据库表是否存在
    if (!SqliteManager::getInstance().tableExists("music_data") ||
        !SqliteManager::getInstance().tableExists("music_lrc")) {
        qWarning() << "music_data或music_lrc表不存在，无法获取待处理列表";
        return result;
    }

    // SQL查询：获取在music_data中存在但在music_lrc中不存在的音乐ID
    QString sql = R"(
        SELECT md.id
        FROM music_data md
        LEFT JOIN music_lrc ml ON md.id = ml.id
        WHERE ml.id IS NULL
          AND md.id IS NOT NULL
          AND md.name IS NOT NULL
    )";

    auto queryResult = SqliteManager::getInstance().querySql(sql, {});
    for (const auto& rowMap : queryResult) {
        QString musicId = rowMap["id"].toString();
        if (!musicId.isEmpty()) {
            result.append(musicId);
        }
    }

    return result;
}

bool LyricAndCoverBatchFetcher::processSingleMusic(const QString& musicId)
{
    if (musicId.isEmpty()) {
        qWarning() << "音乐ID为空，跳过处理";
        return false;
    }

    // 获取音乐详情
    MusicDetail music = Tm_musicPlayListModel::getInstance()->getMusicDetail(musicId);
    if (music.name.isEmpty()) {
        qWarning() << "音乐名称为空，无法处理，ID:" << musicId;
        return false;
    }

    // 1. 构造搜索关键词
    QString keyword = music.name;
    if (music.auther != "未知创作者" && !music.auther.isEmpty()) {
        keyword = QString("%1-%2").arg(music.name).arg(music.auther);
    }
    qDebug() << "歌词搜索关键词:" << keyword;

    // 2. 调用 QQMusicApi::searchMusic 获取歌曲列表（替代原 searchSongmidByKeyword）
    QList<musicData> searchResults = m_qqMusicApi->searchMusic(keyword);
    QString mid;
    QString cover;
    if (!searchResults.isEmpty()) {
        // 沿用原匹配度逻辑，筛选最优结果
        QString bestMid;
        QString bestCover;
        int maxMatchDegree = -1;

        musicData result = searchResults.first();
        mid = result.mid;
        cover = result.cover;

        /*for (const auto& result : searchResults) {
            // 构造「歌名-作者」字符串用于匹配
            QString resultKey = QString("%1-%2").arg(result.name).arg(result.auther);
            int currentDegree = calculateMatchDegree(resultKey, keyword);
            qDebug() << "QQMusicApi搜索结果 - " << resultKey << " | 匹配度:" << currentDegree;

            // 更新最优结果
            if (currentDegree > maxMatchDegree) {
                maxMatchDegree = currentDegree;
                bestMid = result.mid;
                bestCover = result.cover;
            }
        }

        // 验证最优结果有效性
        if (!bestMid.isEmpty() && maxMatchDegree > 0) {
            mid = bestMid;
            cover = bestCover;
            qDebug() << "QQMusicApi最优songmid:" << bestMid << " | 最高匹配度:" << maxMatchDegree;
        }*/

        QString resultKey = QString("%1-%2").arg(result.name, result.auther);
        qDebug() << "搜索到:" << mid << " | cover:" << cover << " | 结果关键词：" << resultKey;
    }

    if (mid.isEmpty()) {
        qWarning() << "未搜索到songmid，无法获取歌词，ID:" << musicId;
        return false;
    }

    // 3. 调用 QQMusicApi::getLyric 获取歌词（替代原 fetchAndSaveLyric 中的网络请求）
    bool lyricSuccess = fetchAndSaveLyric(musicId, mid);

    // 4. 封面下载与数据库更新（原逻辑不变）
    QString coverPath;
    if (!cover.isEmpty()) {
        coverPath = downloadAlbumCover(musicId, cover);
        if (coverPath.isEmpty()) {
            qWarning() << "封面下载失败，ID:" << musicId;
        }

        // 更新数据库中的封面路径
        if (updateMusicCoverInDB(musicId, coverPath)) {
            qDebug() << "封面补全成功，保存路径:" << coverPath;
        } else {
            qWarning() << "封面路径更新失败，ID:" << musicId;
            QFile::remove(coverPath);  // 删除无效的本地文件
        }
    }

    // 只要歌词成功，就视为整体处理有效
    return lyricSuccess;
}

bool LyricAndCoverBatchFetcher::fetchAndSaveLyric(const QString& musicId, const QString& songmid)
{
    // 调整为基于 QQMusicApi 获取歌词
    LyricData lyricData;

    // 调用 QQMusicApi::getLyric 获取歌词数据
    auto qqMusicLyric = m_qqMusicApi->getLyric(songmid);
    // 映射 QQMusicApi 返回的 LyricData 到本地 LyricData（补充 lrc 字段，通过 yrc 转换）
    lyricData.trans = qqMusicLyric.trans;
    lyricData.yrc = qqMusicLyric.yrc;
    lyricData.roma = qqMusicLyric.roma;

    // 若普通歌词为空，尝试从逐字歌词转换（原逻辑不变）
    // if (lyricData.lrc.isEmpty() && !lyricData.yrc.isEmpty()) {
    //     LrcParser parser;
    //     parser.parseFromString(lyricData.yrc);
    //     lyricData.lrc = parser.toStandardLrcString();
    //     qDebug() << "逐字转逐行歌词成功";
    // }

    // 验证歌词有效性
    bool lyricFetched = !lyricData.yrc.isEmpty();
    if (lyricFetched) {
        qDebug() << "QQMusicApi获取歌词成功";
    } else {
        qWarning() << "QQMusicApi返回歌词无效，ID:" << musicId;
        return false;
    }

    // 保存歌词到数据库（原逻辑不变）
    return saveLyricToDB(lyricData, musicId);
}

bool LyricAndCoverBatchFetcher::saveLyricToDB(const LyricData& lyricData, QString musicID)
{
    // 原数据库保存逻辑不变
    if (musicID.isEmpty())
    {
        qWarning() << "歌曲ID为空无法保存歌词";
        return false;
    }

    // 插入/更新歌词
    QString sql = "INSERT OR REPLACE INTO music_lrc (id, trans, yrc, roma) "
                  "VALUES (?, ?, ?, ?)";
    QVector<QVariant> params;
    params << musicID
           << lyricData.trans
           << lyricData.yrc
           << lyricData.roma;

    return SqliteManager::getInstance().executeSql(sql, params);
}

QString LyricAndCoverBatchFetcher::downloadAlbumCover(const QString& musicId, const QString& imgUrl)
{
    // 原封面下载逻辑不变（保留网络请求，因封面URL来自QQMusicApi，需下载到本地）
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

bool LyricAndCoverBatchFetcher::updateMusicCoverInDB(const QString& musicId, const QString& coverPath)
{
    // 原数据库更新逻辑不变
    if (musicId.isEmpty() || coverPath.isEmpty()) {
        qWarning() << "更新封面参数为空";
        return false;
    }

    // 更新music_data表中的img_name字段
    QString sql = "UPDATE music_data SET img_name = ? WHERE id = ?";
    QVector<QVariant> params;
    params << coverPath << musicId;

    return SqliteManager::getInstance().executeSql(sql, params);
}

int LyricAndCoverBatchFetcher::calculateMatchDegree(const QString& resultStr, const QString& keyword)
{
    // 原匹配度计算逻辑不变
    if (resultStr.isEmpty() || keyword.isEmpty()) {
        return 0;
    }

    // 统一转为小写，忽略大小写差异
    QString lowerResult = resultStr.toLower();
    QString lowerKeyword = keyword.toLower();

    // 提取关键词的唯一字符集合（避免重复字符影响匹配度）
    QSet<QChar> keywordUniqueChars;
    for (const QChar& c : lowerKeyword) {
        keywordUniqueChars.insert(c);
    }

    // 统计重合字符数
    int matchedCharCount = 0;
    for (const QChar& c : keywordUniqueChars) {
        if (lowerResult.contains(c)) {
            matchedCharCount++;
        }
    }

    // 计算归一化匹配度（0-100），确保分母不为0
    if (keywordUniqueChars.isEmpty()) {
        return 0;
    }
    return static_cast<int>((static_cast<double>(matchedCharCount) / keywordUniqueChars.size()) * 100);
}
