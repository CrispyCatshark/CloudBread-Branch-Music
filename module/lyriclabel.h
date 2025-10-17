#ifndef LYRICLABEL_H
#define LYRICLABEL_H

#include "ElaText.h"
#include <QLabel>
#include <QVector>

class TextWrap;  // 文本换行工具类（见下文）

class LyricLabel : public ElaText
{
    Q_OBJECT
public:
    explicit LyricLabel(const QVector<QString>& lyricLine, int width = 50, QWidget *parent = nullptr);
    ~LyricLabel() override;

    /**
     * @brief 设置歌词播放状态（控制样式）
     * @param isPlay true=播放中（高亮放大），false=未播放（半透明）
     */
    void setPlay(bool isPlay);

private:
    /**
     * @brief 设置歌词内容（自动换行）
     * @param lyricLines 歌词行列表（如：[罗马音, 原词, 翻译]）
     */
    void setLyric(const QVector<QString>& lyricLines);

private slots:
    void onThemeChanged(ElaThemeType::ThemeMode themeMode);

private:
    ElaThemeType::ThemeMode _themeMode;
    int m_maxCharacters;  // 单行最大字符宽度（中文占2宽度，英文占1宽度）
    bool m_isPlay;        // 当前是否为播放状态
    TextWrap* m_textWrap; // 文本换行工具

protected:
    virtual void paintEvent(QPaintEvent* event) override;
};

#endif // LYRICLABEL_H
