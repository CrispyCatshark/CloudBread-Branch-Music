#include "qqmusicapi.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>
#include <QDateTime>
#include <QCryptographicHash>
#include <QDebug>
#include <QByteArray>

QQMusicApi::QQMusicApi(QObject *parent) : QObject(parent)
{
    m_appSecret = "T3KqiMKLdynixcXk7mfaoWPlFozDNJlV0p5vvKtgaET46uEHyO6wNzAT5GhvHDyfd49tnYyGRQwbukdJxIfX7iIT6I0zGXNwYJVs9NpiouL3DuFaKadOc4xe89V96ZJX";
    m_headerPrefix = "x-CloudBreadMusic-";
}

void QQMusicApi::setAppSecret(const QString &secret)
{
    m_appSecret = secret;
}

QString QQMusicApi::generateTimestamp()
{
    // 生成当前时间戳（秒），确保与服务端时间戳格式一致
    return QString::number(QDateTime::currentSecsSinceEpoch());
}

QString QQMusicApi::generateSign(const QMap<QString, QString> &params)
{
    if (m_appSecret.isEmpty()) {
        qWarning() << "AppSecret未设置，请先调用setAppSecret";
        return "";
    }

    // 1. 按参数名ASCII升序排序（与Python sorted保持一致）
    QStringList sortedKeys = params.keys();
    sortedKeys.sort(); // 使用默认排序，确保与Python的sorted行为一致

    // 2. 拼接成"key=value"格式的字符串（参数值需URL编码）
    QStringList paramList;
    foreach (const QString &key, sortedKeys) {
        // 严格排除sign参数，不区分大小写
        if (key.compare("sign", Qt::CaseInsensitive) == 0)
            continue;

        QString value = params[key];
        // URL编码，严格匹配Python的urllib.parse.quote_plus行为
        // 保留字母、数字和特定符号：._~，其他字符都编码
        QByteArray encodedValue = QUrl::toPercentEncoding(value, "._~");
        // 将空格编码为+而不是%20，与Python的quote_plus保持一致
        encodedValue.replace("%20", "+");
        paramList.append(QString("%1=%2").arg(key).arg(QString(encodedValue)));
    }

    QString paramStr = paramList.join("&");

    // 3. 末尾拼接app_secret
    QString signStr = paramStr + m_appSecret;
    qDebug() << "生成签名的原始字符串:" << signStr;

    // 4. MD5哈希计算（小写）
    QByteArray md5Bytes = QCryptographicHash::hash(signStr.toUtf8(), QCryptographicHash::Md5);
    QString sign = md5Bytes.toHex().toLower();
    qDebug() << "生成的签名:" << sign;

    return sign;
}

QString QQMusicApi::sendAuthenticatedGetRequest(const QString &baseUrl, const QMap<QString, QString> &params)
{
    // 1. 准备所有参数（包含时间戳）
    QMap<QString, QString> allParams = params;
    QString timestamp = generateTimestamp();
    allParams["timestamp"] = timestamp;  // 添加时间戳参数

    // 2. 生成签名
    QString sign = generateSign(allParams);
    if (sign.isEmpty()) {
        qWarning() << "生成签名失败";
        return "";
    }

    // 3. 构建完整URL（包含所有参数）
    QStringList urlParams;
    foreach (const QString &key, allParams.keys()) {
        // 对参数值进行URL编码
        QByteArray encodedValue = QUrl::toPercentEncoding(allParams[key], "._~");
        encodedValue.replace("%20", "+");
        urlParams.append(QString("%1=%2").arg(key).arg(QString(encodedValue)));
    }
    urlParams.append(QString("sign=%1").arg(sign));

    QString fullUrl = baseUrl + "?" + urlParams.join("&");
    qDebug() << "请求URL:" << fullUrl;

    // 4. 设置请求头（包含认证信息）
    http_headers headers;
    headers["Content-Type"] = "application/json;charset=utf-8";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36";
    headers["Accept"] = "application/json, text/plain, */*";
    headers[(m_headerPrefix + "Sign").toStdString()] = sign.toStdString();
    headers[(m_headerPrefix + "Timestamp").toStdString()] = timestamp.toStdString();

    // 5. 发送请求
    auto resp = requests::get(fullUrl.toStdString().c_str(), headers);

    if (resp == nullptr) {
        qWarning() << "请求失败，无法连接到服务器";
        return "";
    }

    // 6. 处理响应
    if (resp->status_code != 200) {
        qWarning() << "请求失败，状态码:" << resp->status_code
                   << "响应内容:" << QString::fromStdString(resp->body);
        return "";
    }

    return QString::fromStdString(resp->body);
}

QList<musicData> QQMusicApi::searchMusic(QString keyword, int page, int pageSize, int searchType)
{
    QList<musicData> resultList;

    if (keyword.isEmpty()) {
        qWarning() << "搜索关键词不能为空";
        return resultList;
    }

    // 准备请求参数
    QMap<QString, QString> params;
    params["keyword"] = keyword;
    params["pageSize"] = QString::number(pageSize);
    params["page"] = QString::number(page);
    params["highlight"] = "false";

    // 发送带认证的请求
    QString responseData = sendAuthenticatedGetRequest("http://localhost:5330/qqmusic/search", params);
    if (responseData.isEmpty()) {
        return resultList;
    }

    // 解析JSON响应
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData.toUtf8(), &jsonError);

    if (jsonError.error == QJsonParseError::NoError && jsonDoc.isArray()) {
        QJsonArray dataArray = jsonDoc.array();

        // 遍历所有搜索结果
        for (const QJsonValue& songVal : dataArray) {
            if (!songVal.isObject()) continue;

            QJsonObject songObj = songVal.toObject();
            musicData currentData;

            // 提取歌曲基本信息
            currentData.mid = songObj["mid"].toString();
            currentData.name = songObj["name"].toString();

            // 处理歌手信息（可能有多个）
            if (songObj.contains("singer") && songObj["singer"].isArray()) {
                QJsonArray singerArray = songObj["singer"].toArray();
                QStringList singerList;
                for (const QJsonValue& singerVal : singerArray) {
                    if (singerVal.isObject()) {
                        singerList.append(singerVal.toObject()["name"].toString());
                    }
                }
                currentData.auther = singerList.join(" / ");
            }

            // 处理专辑信息
            if (songObj.contains("album") && songObj["album"].isObject()) {
                QJsonObject albumData = songObj["album"].toObject();
                QString albumMid = albumData["mid"].toString();
                currentData.album = albumData["name"].toString();
                currentData.cover = QString("https://y.gtimg.cn/music/photo_new/T002R500x500M000%1.jpg").arg(albumMid);
            }

            // 提取发行时间
            currentData.time = songObj["time_public"].toString();

            resultList.append(currentData);
        }

        qDebug() << "搜索完成，共返回" << resultList.size() << "条结果";
    } else {
        qWarning() << "JSON解析失败，错误:" << jsonError.errorString()
            << "响应数据:" << responseData;
    }

    return resultList;
}

LyricData QQMusicApi::getLyric(QString mid)
{
    LyricData resultLyric;

    if (mid.isEmpty()) {
        qWarning() << "歌曲ID不能为空";
        return resultLyric;
    }

    // 准备请求参数
    QMap<QString, QString> params;
    params["id"] = mid;

    // 发送带认证的请求
    QString responseData = sendAuthenticatedGetRequest("http://localhost:5330/qqmusic/lyric", params);
    if (responseData.isEmpty()) {
        return resultLyric;
    }

    // 解析JSON响应
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData.toUtf8(), &jsonError);

    if (jsonError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
        QJsonObject dataObj = jsonDoc.object();

        resultLyric.yrc = dataObj["lyric"].toString();
        resultLyric.roma = dataObj["roma"].toString();
        resultLyric.trans = dataObj["trans"].toString();

        qDebug() << "获取歌词成功";
    } else {
        qWarning() << "JSON解析失败，错误:" << jsonError.errorString()
            << "响应数据:" << responseData;
    }

    return resultLyric;
}

musicData QQMusicApi::getSongInfo(QString mid)
{
    musicData resultData;

    if (mid.isEmpty()) {
        qWarning() << "歌曲ID不能为空";
        return resultData;
    }

    // 准备请求参数
    QMap<QString, QString> params;
    params["id"] = mid;

    // 发送带认证的请求
    QString responseData = sendAuthenticatedGetRequest("http://localhost:5330/qqmusic/songInfo", params);
    if (responseData.isEmpty()) {
        return resultData;
    }

    // 解析JSON响应
    QJsonParseError jsonError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData.toUtf8(), &jsonError);

    if (jsonError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
        QJsonObject songObj = jsonDoc.object();

        // 提取歌曲信息
        resultData.mid = songObj["mid"].toString();
        resultData.name = songObj["name"].toString();

        // 处理歌手信息
        if (songObj.contains("singer") && songObj["singer"].isArray()) {
            QJsonArray singerArray = songObj["singer"].toArray();
            QStringList singerList;
            for (const QJsonValue& singerVal : singerArray) {
                if (singerVal.isObject()) {
                    singerList.append(singerVal.toObject()["name"].toString());
                }
            }
            resultData.auther = singerList.join(" / ");
        }

        // 处理专辑信息
        if (songObj.contains("album") && songObj["album"].isObject()) {
            QJsonObject albumData = songObj["album"].toObject();
            QString albumMid = albumData["mid"].toString();
            resultData.album = albumData["name"].toString();
            resultData.cover = QString("https://y.gtimg.cn/music/photo_new/T002R500x500M000%1.jpg").arg(albumMid);
        }

        resultData.time = songObj["time_public"].toString();

        qDebug() << "获取歌曲信息成功";
    } else {
        qWarning() << "JSON解析失败，错误:" << jsonError.errorString()
            << "响应数据:" << responseData;
    }

    return resultData;
}
