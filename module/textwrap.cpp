#include "TextWrap.h"
#include <QChar>
#include <QVector>
#include <QString>

TextWrap::TextWrap()
{
    // 初始化字符宽度区间
    m_charWidths = {
        {126, 1}, {159, 0}, {687, 1}, {710, 0},
        {711, 1}, {727, 0}, {733, 1}, {879, 0},
        {1154, 1}, {1161, 0}, {4347, 1}, {4447, 2},
        {7467, 1}, {7521, 0}, {8369, 1}, {8426, 0},
        {9000, 1}, {9002, 2}, {11021, 1}, {12350, 2},
        {12351, 1}, {12438, 2}, {12442, 0}, {19893, 2},
        {19967, 1}, {55203, 2}, {63743, 1}, {64106, 2},
        {65039, 1}, {65059, 0}, {65131, 2}, {65279, 1},
        {65376, 2}, {65500, 1}, {65510, 2}, {120831, 1},
        {262141, 2}, {1114109, 1}
    };
}

int TextWrap::getCharWidth(QChar c)
{
    ushort o = c.unicode();
    if (o == 0x0E || o == 0x0F) {
        return 0;
    }

    for (const auto& pair : m_charWidths) {
        if (o <= pair.first) {
            return pair.second;
        }
    }
    return 1;
}

QPair<QString, bool> TextWrap::wrap(const QString& text, int width, bool once)
{
    // 先创建文本的非const副本，再移除所有换行符
    QString processedText = text;  // 创建副本
    processedText.remove(QChar('\n'), Qt::CaseInsensitive);    // 使用remove移除换行符，而非replace
    QString trimmedText = processedText.trimmed();

    if (trimmedText.isEmpty()) {
        return {trimmedText, false};
    }

    int count = 0;
    int lastCount = 0;
    QVector<QChar> chars;
    bool isWrapped = false;
    int breakPos = -1;
    bool isBreakAlpha = true;
    int nInsideBreak = 0;

    for (int i = 0; i < trimmedText.size(); ++i) {
        QChar c = trimmedText.at(i);
        int charWidth = getCharWidth(c);
        count += charWidth;

        if (c == ' ' || charWidth > 1) {
            breakPos = i + nInsideBreak;
            lastCount = count;
            isBreakAlpha = (charWidth == 1);
        }

        // 未达到宽度限制，添加字符
        if (count <= width) {
            chars.append(c);
            continue;
        }

        // 达到宽度限制，执行换行
        if (breakPos != -1 && isBreakAlpha) {
            if (c != ' ') {
                chars[breakPos] = '\n';
                chars.append(c);
                count -= lastCount;
                lastCount = 0;
            } else {
                chars.append('\n');  // 添加换行符
                count = 0;
                lastCount = 0;
            }
        } else {
            chars.append('\n');  // 添加换行符
            chars.append(c);
            count = charWidth;
        }

        isWrapped = true;
        if (once) {
            QString current = QString::fromRawData(chars.data(), chars.size());
            QString result = current + trimmedText.mid(i + 1);
            return {result, isWrapped};
        }

        nInsideBreak++;
    }

    // 拼接所有字符并返回结果
    QString result = QString::fromRawData(chars.data(), chars.size());
    return {result, isWrapped};
}
