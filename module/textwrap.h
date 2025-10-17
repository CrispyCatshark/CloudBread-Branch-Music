#ifndef TEXTWRAP_H
#define TEXTWRAP_H

#include <QString>
#include <QPair>
#include <QVector>
#include <QChar>

class TextWrap
{
public:
    TextWrap();

    /**
     * @brief 按字符宽度自动换行
     * @param text 待换行文本
     * @param width 单行最大宽度（按字符宽度计算：中文2，英文1）
     * @param once 是否只换一次行
     * @return 换行后的文本 + 是否发生换行
     */
    QPair<QString, bool> wrap(const QString& text, int width, bool once = true);

private:
    /**
     * @brief 获取单个字符的显示宽度
     * @param c 目标字符
     * @return 1=英文/数字，2=中文/全角字符，0=控制字符
     */
    int getCharWidth(QChar c);

private:
    // 字符宽度映射表
    QVector<QPair<ushort, int>> m_charWidths;
};

#endif // TEXTWRAP_H
