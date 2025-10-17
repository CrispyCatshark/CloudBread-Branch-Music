#include "LrcParser.h"
#include <QFile>
#include <QRegularExpression>
#include <QtCore5Compat/QTextCodec>

LrcParser::LrcParser(QObject *parent) : QObject(parent), m_offset(0)
{
}

bool LrcParser::parseFromFile(const QString &filePath)
{
    // 清空所有数据
    clearData();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray fileData = file.readAll();
    file.close();

    // GBK优先解析，失败则UTF-8
    QTextCodec *gbkCodec = QTextCodec::codecForName("GBK");
    QString content;
    if (gbkCodec) {
        content = gbkCodec->toUnicode(fileData);
        bool hasReplacement = content.contains(QChar::ReplacementCharacter);
        if (content.isEmpty() || hasReplacement) {
            content = QString::fromUtf8(fileData);
        }
    } else {
        content = QString::fromUtf8(fileData);
    }

    if (content.isEmpty()) {
        return false;
    }

    return parseFromString(content);
}

bool LrcParser::parseFromString(const QString &lrcContent)
{
    // 清空所有数据
    clearData();

    // 分割行（兼容\r\n和\n）
    QStringList lines = lrcContent.split(QRegularExpression("\\r?\\n"), Qt::SkipEmptyParts);

    // 预编译正则：元数据标签、逐字歌词行
    static const QRegularExpression metaTagRegex("^\\[(ti|ar|al|by|offset):.*\\]$", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression charLyricLineRegex("^\\[(\\d+),(\\d+)\\].*\\(\\d+,\\d+\\).*$"); // 匹配带逐字信息的行

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) {
            continue;
        }

        // 解析元数据标签
        if (metaTagRegex.match(trimmedLine).hasMatch()) {
            parseTag(trimmedLine);
            continue;
        }

        // 解析逐字歌词行（优先处理）
        if (charLyricLineRegex.match(trimmedLine).hasMatch()) {
            parseCharLyricLine(trimmedLine);
            continue;
        }

        // 兼容解析传统整行歌词
        static const QRegularExpression normalLyricRegex("^\\[\\d+:\\d+(\\.\\d+)?\\].*$");
        if (normalLyricRegex.match(trimmedLine).hasMatch()) {
            parseNormalLyricLine(trimmedLine);
            continue;
        }
    }

    // 有有效歌词（整行或逐字）即认为成功
    return !m_charLyrics.isEmpty();
}

// 获取逐字歌词列表（包含逐字和整行歌词，整行歌词的charLyrics为空）
QVector<LineLyric> LrcParser::getCharLyrics() const
{
    return m_charLyrics;
}

// 获取整行歌词列表（仅包含整行歌词，charLyrics为空）
QVector<LineLyric> LrcParser::getNormalLyrics() const
{
    QVector<LineLyric> normalLyrics;
    for (const auto& line : m_charLyrics) {
        // 整行歌词的charLyrics为空，以此为筛选条件
        if (line.charLyrics.isEmpty()) {
            normalLyrics.append(line);
        }
    }
    return normalLyrics;
}

// 根据时间获取当前逐字歌词（返回行索引和字索引）
QPair<int, int> LrcParser::getCharLyricIndexAtTime(qint64 currentTime) const
{
    qint64 adjustedTime = currentTime + m_offset;

    // 查找当前所在的歌词行
    for (int lineIdx = 0; lineIdx < m_charLyrics.size(); ++lineIdx) {
        const LineLyric &line = m_charLyrics[lineIdx];
        // 行的时间范围：[lineStartTime, lineEndTime)
        if (adjustedTime >= line.lineStartTime && adjustedTime < line.lineEndTime) {
            // 若为整行歌词（无逐字信息），返回行索引和-1
            if (line.charLyrics.isEmpty()) {
                return QPair<int, int>(lineIdx, -1);
            }
            // 查找当前行内的字符
            for (int charIdx = 0; charIdx < line.charLyrics.size(); ++charIdx) {
                const CharLyric &cl = line.charLyrics[charIdx];
                // 字符的时间范围：[startTime, endTime)
                if (adjustedTime >= cl.startTime && adjustedTime < cl.endTime) {
                    return QPair<int, int>(lineIdx, charIdx);
                }
            }
            // 时间在当前行内，但未匹配到具体字符（可能是空格/标点），返回行索引和-1
            return QPair<int, int>(lineIdx, -1);
        }
        // 超过当前行，且后续行时间递增，直接跳出
        if (adjustedTime < line.lineStartTime) {
            break;
        }
    }

    // 未找到匹配
    return QPair<int, int>(-1, -1);
}

// 根据时间获取当前逐字歌词内容
QString LrcParser::getCharLyricAtTime(qint64 currentTime) const
{
    QPair<int, int> idxPair = getCharLyricIndexAtTime(currentTime);
    if (idxPair.first == -1 || idxPair.second == -1) {
        return QString();
    }
    return m_charLyrics[idxPair.first].charLyrics[idxPair.second].character;
}

// 根据时间获取当前整行歌词内容
QString LrcParser::getNormalLyricAtTime(qint64 currentTime) const
{
    qint64 adjustedTime = currentTime + m_offset;

    // 遍历所有歌词行，查找当前时间所在的整行歌词
    for (const auto& line : m_charLyrics) {
        // 筛选整行歌词（charLyrics为空），并判断时间范围
        if (line.charLyrics.isEmpty() && adjustedTime >= line.lineStartTime && adjustedTime < line.lineEndTime) {
            return line.lineContent;
        }
        // 时间未到当前行，后续行时间递增，跳出循环
        if (adjustedTime < line.lineStartTime) {
            break;
        }
    }

    // 未找到或当前时间不在任何整行歌词区间内
    return QString();
}

// 获取下一句整行歌词的时间
qint64 LrcParser::getNextNormalLyricTime(qint64 currentTime) const
{
    qint64 adjustedTime = currentTime + m_offset;

    // 遍历所有歌词行，查找第一个晚于当前时间的整行歌词
    for (const auto& line : m_charLyrics) {
        if (line.charLyrics.isEmpty() && line.lineStartTime > adjustedTime) {
            return line.lineStartTime - m_offset; // 转换回原始时间（减去offset）
        }
    }

    // 无下一句
    return -1;
}

// 获取上一句整行歌词的时间
qint64 LrcParser::getPrevNormalLyricTime(qint64 currentTime) const
{
    qint64 adjustedTime = currentTime + m_offset;
    qint64 prevTime = -1;

    // 遍历所有歌词行，记录最后一个早于当前时间的整行歌词
    for (const auto& line : m_charLyrics) {
        if (line.charLyrics.isEmpty() && line.lineStartTime < adjustedTime) {
            prevTime = line.lineStartTime;
        } else if (line.lineStartTime >= adjustedTime) {
            // 后续行时间递增，无需继续遍历
            break;
        }
    }

    return prevTime == -1 ? -1 : (prevTime - m_offset); // 转换回原始时间
}

QString LrcParser::getTitle() const { return m_title; }
QString LrcParser::getArtist() const { return m_artist; }
QString LrcParser::getAlbum() const { return m_album; }
QString LrcParser::getLyricist() const { return m_lyricist; }
int LrcParser::getOffset() const { return m_offset; }

// 解析逐字歌词行（格式：[行开始时间,行时长]字1(字1开始,字1时长)字2(字2开始,字2时长)...）
void LrcParser::parseCharLyricLine(const QString &line)
{
    // qDebug()<<line;

    // 新增步骤：先将所有 (数字,数字) 格式替换为 <数字,数字>
    QString modifiedLine = line;
    QRegularExpression replaceRegex("\\((\\d+),(\\d+)\\)");
    modifiedLine.replace(replaceRegex, "<\\1,\\2>");

    // 1. 提取行时间信息：[行开始时间,行时长]
    QRegularExpression lineTimeRegex("^\\[(\\d+),(\\d+)\\]");
    QRegularExpressionMatch lineTimeMatch = lineTimeRegex.match(modifiedLine);
    if (!lineTimeMatch.hasMatch()) {
        return;
    }

    // 解析行时间（应用offset，与原逻辑一致）
    qint64 lineStartTime = lineTimeMatch.captured(1).toLongLong() + m_offset;
    qint64 lineDuration = lineTimeMatch.captured(2).toLongLong();
    qint64 lineEndTime = lineStartTime + lineDuration;

    // 2. 提取歌词内容和逐字时间（去除行时间前缀）
    QString contentPart = modifiedLine.mid(lineTimeMatch.capturedLength()).trimmed();

    // qDebug()<<contentPart;

    // 3. 提取逐字信息：支持 "<绝对时间,时长>" 或 "字符<绝对时间,时长>" 两种格式
    // 正则表达式修改为匹配尖括号格式
    QRegularExpression charRegex("([^<>]+)<(\\d+),(\\d+)>");
    QRegularExpressionMatchIterator charMatchIter = charRegex.globalMatch(contentPart);

    LineLyric lineLyric;
    lineLyric.lineStartTime = lineStartTime;
    lineLyric.lineDuration = lineDuration;
    lineLyric.lineEndTime = lineEndTime;

    // 拼接整行内容，同时解析每个字符
    QString lineContent;
    while (charMatchIter.hasNext()) {
        QRegularExpressionMatch charMatch = charMatchIter.next();
        if (charMatch.lastCapturedIndex() < 3) {
            continue;
        }

        // 解析字符信息：captured(1)可能为空（纯时间标签），为空时字符设为空格
        QString character = charMatch.captured(1).isEmpty() ? " " : charMatch.captured(1);
        // 关键修改：字符开始时间是绝对时间，直接转换后加offset
        qint64 charAbsStart = charMatch.captured(2).toLongLong() + m_offset;
        qint64 charDuration = charMatch.captured(3).toLongLong();
        qint64 charAbsEnd = charAbsStart + charDuration;

        // qDebug() << charMatch.captured(1)<<"|"<<charMatch.captured(2)<<"|"<<charMatch.captured(3);

        // 添加到逐字列表
        CharLyric charLyric;
        charLyric.character = character;
        charLyric.startTime = charAbsStart;
        charLyric.duration = charDuration;
        charLyric.endTime = charAbsEnd;
        lineLyric.charLyrics.append(charLyric);

        // 拼接整行内容（空字符用空格占位，避免内容为空）
        lineContent += character;
    }

    lineLyric.lineContent = lineContent;

    // 关键修改：即使逐字列表为空（纯时间行），也添加到列表（原逻辑需逐字列表非空）
    m_charLyrics.append(lineLyric);
}

// 解析传统整行歌词（格式：[mm:ss.xx]歌词内容）
void LrcParser::parseNormalLyricLine(const QString &line)
{
    QString lineCopy = line;
    QVector<qint64> lineStartTimes; // 存储当前行的所有开始时间（支持一行多时间标签）

    // 提取时间标签（[mm:ss.xx]）
    while (lineCopy.startsWith('[')) {
        int closeBracketIdx = lineCopy.indexOf(']');
        if (closeBracketIdx == -1) {
            break;
        }

        QString timeTag = lineCopy.mid(0, closeBracketIdx + 1);
        qint64 timeMs = parseNormalTimeTag(timeTag);
        if (timeMs != -1) {
            lineStartTimes.append(timeMs + m_offset); // 应用offset，得到行开始时间
        }

        lineCopy = lineCopy.mid(closeBracketIdx + 1).trimmed();
    }

    if (lineStartTimes.isEmpty() || lineCopy.isEmpty()) {
        return;
    }

    // 计算行结束时间：当前行开始时间 = 下一行开始时间（若存在），否则设为极大值
    for (int i = 0; i < lineStartTimes.size(); ++i) {
        LineLyric lineLyric;
        lineLyric.lineStartTime = lineStartTimes[i];
        lineLyric.lineContent = lineCopy;
        // charLyrics保持为空（符合需求）

        // 计算行结束时间：取下一个时间标签的开始时间，若无则设为INT64_MAX
        if (i < lineStartTimes.size() - 1) {
            lineLyric.lineEndTime = lineStartTimes[i + 1];
        } else {
            // 查找后续歌词行的第一个开始时间作为当前行的结束时间
            qint64 nextLineStartTime = INT64_MAX;
            for (const auto& existingLine : m_charLyrics) {
                if (existingLine.lineStartTime > lineLyric.lineStartTime && existingLine.lineStartTime < nextLineStartTime) {
                    nextLineStartTime = existingLine.lineStartTime;
                }
            }
            lineLyric.lineEndTime = nextLineStartTime;
        }

        // 计算行时长
        lineLyric.lineDuration = lineLyric.lineEndTime - lineLyric.lineStartTime;

        // 添加到统一歌词列表
        m_charLyrics.append(lineLyric);
    }

    // 对歌词列表按开始时间排序（确保时间顺序正确，处理一行多时间标签或乱序情况）
    std::sort(m_charLyrics.begin(), m_charLyrics.end(), [](const LineLyric& a, const LineLyric& b) {
        return a.lineStartTime < b.lineStartTime;
    });
}

// 解析元数据标签（ti/ar/al/by/offset）
void LrcParser::parseTag(const QString &line)
{
    int closeBracketIdx = line.indexOf(']');
    if (closeBracketIdx <= 1) {
        return;
    }

    QString tagPart = line.mid(1, closeBracketIdx - 1);
    int colonIdx = tagPart.indexOf(':');
    if (colonIdx <= 0) {
        return;
    }

    QString tagName = tagPart.left(colonIdx).toLower();
    QString tagValue = tagPart.mid(colonIdx + 1).trimmed();

    if (tagName == "ti") {
        m_title = tagValue;
    } else if (tagName == "ar") {
        m_artist = tagValue;
    } else if (tagName == "al") {
        m_album = tagValue;
    } else if (tagName == "by") {
        m_lyricist = tagValue;
    } else if (tagName == "offset") {
        bool ok;
        int offset = tagValue.toInt(&ok);
        if (ok) {
            m_offset = offset;
        }
    }
}

// 解析传统时间标签（[mm:ss.xx]）
qint64 LrcParser::parseNormalTimeTag(const QString &timeTag)
{
    if (timeTag.length() < 6 || !timeTag.startsWith('[') || !timeTag.endsWith(']')) {
        return -1;
    }

    QString timeStr = timeTag.mid(1, timeTag.length() - 2);
    QStringList parts = timeStr.split(':');
    if (parts.size() != 2) {
        return -1;
    }

    // 解析分钟
    bool ok;
    int minutes = parts[0].toInt(&ok);
    if (!ok || minutes < 0) {
        return -1;
    }

    // 解析秒和毫秒
    QString secondsPart = parts[1];
    int seconds = 0;
    int milliseconds = 0;

    if (secondsPart.contains('.')) {
        QStringList secParts = secondsPart.split('.');
        if (secParts.size() == 2) {
            seconds = secParts[0].toInt(&ok);
            if (!ok || seconds < 0 || seconds >= 60) {
                return -1;
            }

            // 处理1-3位毫秒
            QString msStr = secParts[1];
            if (msStr.length() == 1) {
                milliseconds = msStr.toInt() * 100;
            } else if (msStr.length() == 2) {
                milliseconds = msStr.toInt() * 10;
            } else if (msStr.length() >= 3) {
                milliseconds = msStr.left(3).toInt();
            }

            if (milliseconds < 0 || milliseconds >= 1000) {
                return -1;
            }
        } else {
            return -1;
        }
    } else {
        seconds = secondsPart.toInt(&ok);
        if (!ok || seconds < 0 || seconds >= 60) {
            return -1;
        }
    }

    return (minutes * 60LL + seconds) * 1000LL + milliseconds;
}

// 清空所有数据（避免解析残留）
void LrcParser::clearData()
{
    m_title.clear();
    m_artist.clear();
    m_album.clear();
    m_lyricist.clear();
    m_offset = 0;
    m_charLyrics.clear(); // 统一使用m_charLyrics存储所有歌词
}

/**
 * 核心方法：生成标准LRC格式字符串
 */
QString LrcParser::toStandardLrcString() const
{
    QString lrcContent;

    // 第一步：输出元数据标签（按 ti -> ar -> al -> by -> offset 顺序）
    if (!m_title.isEmpty()) {
        lrcContent.append(QString("[ti:%1]\n").arg(m_title));
    }
    if (!m_artist.isEmpty()) {
        lrcContent.append(QString("[ar:%1]\n").arg(m_artist));
    }
    if (!m_album.isEmpty()) {
        lrcContent.append(QString("[al:%1]\n").arg(m_album));
    }
    if (!m_lyricist.isEmpty()) {
        lrcContent.append(QString("[by:%1]\n").arg(m_lyricist));
    }
    // offset 始终输出（默认0）
    lrcContent.append(QString("[offset:%1]\n").arg(m_offset));

    // 第二步：输出歌词行（已按时间排序，直接遍历 m_charLyrics 即可）
    for (const auto& lineLyric : m_charLyrics) {
        // 1. 生成时间标签（转换为 mm:ss.xx 格式）
        QString timeTag = formatTimeTag(lineLyric.lineStartTime - m_offset);
        // 注：减去offset，还原为原始LRC时间（与解析时的offset处理对应）

        // 2. 拼接 "时间标签+歌词内容"（空内容保留，对应示例中的空行）
        lrcContent.append(timeTag % lineLyric.lineContent % "\n");
    }

    // 移除末尾多余的换行符（可选，根据需求调整）
    if (!lrcContent.isEmpty()) {
        lrcContent.chop(1);
    }

    return lrcContent;
}

/**
 * 辅助方法：毫秒转 [mm:ss.xx] 格式
 * 例：12345ms → 00:12.34（取两位毫秒）
 */
QString LrcParser::formatTimeTag(qint64 timeMs) const
{
    // 处理异常时间（小于0则返回默认值）
    if (timeMs < 0) {
        return "[00:00.00]";
    }

    // 分解时间：分钟、秒、毫秒
    qint64 totalSeconds = timeMs / 1000;
    qint64 minutes = totalSeconds / 60;
    qint64 seconds = totalSeconds % 60;
    qint64 milliseconds = (timeMs % 1000) / 10; // 取前两位毫秒（00-99）

    // 格式化：确保两位数字（补0）
    return QString("[%1:%2.%3]")
        .arg(minutes, 2, 10, QLatin1Char('0'))   // 分钟（00-99）
        .arg(seconds, 2, 10, QLatin1Char('0'))   // 秒（00-59）
        .arg(milliseconds, 2, 10, QLatin1Char('0')); // 毫秒（00-99）
}
