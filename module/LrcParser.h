#ifndef LRCPARSER_H
#define LRCPARSER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QVector>
#include <QPair>

// 逐字歌词结构体（单个字符信息）
struct CharLyric {
    QString character;   // 字符内容
    qint64 startTime;    // 字符开始时间（毫秒，绝对时间）
    qint64 duration;     // 字符显示时长（毫秒）
    qint64 endTime;      // 字符结束时间（毫秒，绝对时间）
};

// 行歌词结构体（整行+逐字信息）
struct LineLyric {
    qint64 lineStartTime;  // 行开始时间（毫秒，绝对时间）
    qint64 lineDuration;   // 行显示时长（毫秒）
    qint64 lineEndTime;    // 行结束时间（毫秒，绝对时间）
    QString lineContent;   // 整行歌词内容
    QVector<CharLyric> charLyrics; // 逐字歌词列表
};

class LrcParser : public QObject
{
    Q_OBJECT
public:
    explicit LrcParser(QObject *parent = nullptr);

    // 从文件解析LRC歌词（支持普通和逐字格式）
    bool parseFromFile(const QString &filePath);

    // 从字符串解析LRC歌词（支持普通和逐字格式）
    bool parseFromString(const QString &lrcContent);

    // 获取歌曲信息
    QString getTitle() const;
    QString getArtist() const;
    QString getAlbum() const;
    QString getLyricist() const;
    int getOffset() const;

    // 普通歌词相关接口
    QVector<LineLyric> getNormalLyrics() const;
    QString getNormalLyricAtTime(qint64 currentTime) const;
    qint64 getNextNormalLyricTime(qint64 currentTime) const;
    qint64 getPrevNormalLyricTime(qint64 currentTime) const;

    // 逐字歌词相关接口（新增）
    QVector<LineLyric> getCharLyrics() const;
    QPair<int, int> getCharLyricIndexAtTime(qint64 currentTime) const; // 返回(行索引, 字索引)
    QString getCharLyricAtTime(qint64 currentTime) const;

    /**
     * @brief 将解析后的歌词转换为标准LRC格式字符串
     * @return 标准格式的LRC歌词字符串（每行以"\n"分隔）
     */
    QString toStandardLrcString() const;

private:
    // 清空所有解析数据
    void clearData();

    // 解析元数据标签（ti/ar/al/by/offset）
    void parseTag(const QString &line);

    // 解析普通歌词行（[mm:ss.xx]歌词内容）
    void parseNormalLyricLine(const QString &line);

    // 解析逐字歌词行（[行开始,行时长]字1(开始,时长)字2(开始,时长)...）
    void parseCharLyricLine(const QString &line);

    // 解析普通时间标签（[mm:ss.xx]）
    qint64 parseNormalTimeTag(const QString &timeTag);

    /**
     * @brief 辅助函数：将毫秒时间转换为 [mm:ss.xx] 格式的字符串
     * @param timeMs 时间（毫秒）
     * @return 格式化的时间标签字符串（如 "00:12.34"）
     */
    QString formatTimeTag(qint64 timeMs) const;

    // 歌曲信息
    QString m_title;      // 标题
    QString m_artist;     // 歌手
    QString m_album;      // 专辑
    QString m_lyricist;   // 歌词作者
    int m_offset;         // 时间偏移（毫秒）

    // 普通歌词存储（时间戳->歌词内容）
    QMap<qint64, QString> m_normalLyrics;

    // 逐字歌词存储（新增）
    QVector<LineLyric> m_charLyrics;
};

#endif // LRCPARSER_H
