#include "LyricLabel.h"
#include "ElaTheme.h"
#include "TextWrap.h"  // 文本换行工具
#include <QPalette>
#include <QFont>
#include <qpainter.h>

LyricLabel::LyricLabel(const QVector<QString>& lyricLines, int width, QWidget *parent)
    : ElaText(parent),
    m_maxCharacters(width),  // 与Python一致，单行最大宽度（按字符宽度计算）
    m_isPlay(false),
    m_textWrap(new TextWrap())
{
    this->setAlignment(Qt::AlignCenter);  // 居中对齐
    this->setLyric(lyricLines);  // 初始化歌词内容
    this->setFixedWidth(width);
    this->setWordWrap(true);
    this->adjustSize();

    _themeMode = eTheme->getThemeMode();
    connect(eTheme, &ElaTheme::themeModeChanged, this, &LyricLabel::onThemeChanged);
}

LyricLabel::~LyricLabel()
{
    delete m_textWrap;
}

void LyricLabel::onThemeChanged(ElaThemeType::ThemeMode themeMode)
{
    _themeMode = themeMode;
    update();
}

void LyricLabel::setLyric(const QVector<QString>& lyricLines)
{
    // 对每一行歌词进行自动换行处理
    QString wrappedText;
    for (const QString& line : lyricLines) {
        QString wrappedLine = m_textWrap->wrap(line, m_maxCharacters).first;
        wrappedText += line + "\n";
    }
    wrappedText.chop(1);  // 移除最后一个换行符

    this->setText(wrappedText);
    this->setPlay(false);  // 默认初始为未播放状态
}

void LyricLabel::setPlay(bool isPlay)
{
    m_isPlay = isPlay;

    // 1. 调整字体颜色与透明度
    // QPalette palette = this->palette();
    // 未选中为黑色，正在播放为深蓝色(#0000CD为深蓝色)
    // QColor textColor = m_isPlay ? QColor(0, 0, 205) : QColor(0, 0, 0);
    // palette.setBrush(QPalette::Text, textColor);
    // this->setPalette(palette);

    // 2. 调整字体大小与粗细
    QFont font("Microsoft YaHei");
    int baseSize = 18;  // 基础字体大小
    font.setPixelSize(m_isPlay ? (baseSize * 1.25) : baseSize);  // 播放中放大25%
    this->setFont(font);
    // this->setFixedHeight((m_isPlay ? (baseSize * 1.25) : baseSize) + 5);

    // 3. 调整控件大小（适应文本）
    // this->adjustSize();
    this->update();
}

void LyricLabel::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.save();
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.setPen(m_isPlay ? (_themeMode==ElaThemeType::ThemeMode::Light?QColor(9, 81, 197):QColor(34, 154, 4)) : ElaThemeColor(_themeMode, BasicText));
    painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap | Qt::TextWrapAnywhere, text());
    painter.restore();
}
