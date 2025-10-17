#ifndef QQMUSICAPI_H
#define QQMUSICAPI_H

#include <QObject>
#include <QList>
#include <QString>
#include <hv/requests.h>

// 音乐数据结构
struct musicData {
    QString mid;       // 音乐ID
    QString name;      // 音乐名称
    QString auther;    // 歌手
    QString album;     // 专辑
    QString cover;     // 封面URL
    QString time;      // 发行时间
};

// 歌词数据结构
struct LyricData {
    QString yrc;       // 原歌词
    QString roma;      // 罗马音
    QString trans;     // 翻译
};

class QQMusicApi : public QObject
{
    Q_OBJECT
public:
    explicit QQMusicApi(QObject *parent = nullptr);

    // 设置认证密钥
    void setAppSecret(const QString &secret);

    // 搜索音乐
    QList<musicData> searchMusic(QString keyword, int page = 1, int pageSize = 10, int searchType = 0);

    // 获取歌词
    LyricData getLyric(QString mid);

    // 获取歌曲信息
    musicData getSongInfo(QString mid);

private:
    // 生成签名
    QString generateSign(const QMap<QString, QString> &params);

    // 生成当前时间戳
    QString generateTimestamp();

    // 发送带认证的GET请求
    QString sendAuthenticatedGetRequest(const QString &baseUrl, const QMap<QString, QString> &params);

private:
    QString m_appSecret;  // 客户端密钥，需与服务端一致
    QString m_headerPrefix = "x-CloudBreadMusic-";  // 认证头部前缀
};

#endif // QQMUSICAPI_H
