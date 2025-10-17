#include "lyricwidget.h"
#include "lyriclabel.h"
#include <QScrollBar>
#include <QtGlobal>

LyricsWeight::LyricsWeight(QWidget *parent)
    : QScrollArea(parent),
    m_lyricsNowPoint(0)
{
    // 初始化滚动区域属性
    this->setWidgetResizable(true);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->setStyleSheet("border: none; background-color: transparent;");

    // 初始化中心widget与布局
    m_scrollWidget = new QWidget(this);
    m_scrollWidget->resize(800, 500);
    m_expandLayout = new QVBoxLayout(m_scrollWidget);
    m_expandLayout->setContentsMargins(0, 0, 0, 0);  // 左右留边距，避免贴边
    m_expandLayout->setSpacing(8);
    m_expandLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);  // 布局整体居左顶部对齐
    this->setWidget(m_scrollWidget);

    // 初始化滚动动画
    m_scrollAni = new QPropertyAnimation(this->verticalScrollBar(), "value", this);
    m_scrollAni->setDuration(350);
}

LyricsWeight::~LyricsWeight()
{
    delete m_scrollAni;
    // 手动删除滚动部件
    delete m_scrollWidget;
}

void LyricsWeight::setLyrics(const QVector<LineLyric>& lyricData,
                             const QVector<LineLyric>& transData,
                             const QVector<LineLyric>& romaData)
{
    // 保存歌词数据
    m_lyricData = lyricData;
    m_transData = transData;
    m_romaData = romaData;

    m_lastPositionMs = -1;

    // 清空原有数据与标签
    deleteItems();
    m_lyricsLabelList.clear();
    m_lyricsTimeList.clear();
    m_lyricsNowPoint = 0;

    if (m_lyricData.isEmpty()) {
        LyricLabel* blankLabel = new LyricLabel({"暂无歌词"}, size().width(), m_scrollWidget);
        blankLabel->setAlignment(Qt::AlignLeft);  // 空白标签居左
        m_expandLayout->addWidget(blankLabel);
    }

    // 遍历原歌词，生成每行标签（居左显示）
    for (int lineIdx = 0; lineIdx < m_lyricData.size(); ++lineIdx) {
        const LineLyric& line = m_lyricData[lineIdx];
        QVector<QString> lyricLines;

        // 添加罗马音
        if (!m_romaData.isEmpty() && lineIdx < m_romaData.size()) {
            lyricLines.append(m_romaData[lineIdx].lineContent);
        }

        // 添加原歌词
        lyricLines.append(line.lineContent);

        // 添加翻译
        if (!m_transData.isEmpty() && lineIdx < m_transData.size()) {
            lyricLines.append(m_transData[lineIdx].lineContent);
        }

        LyricLabel* lyricLabel = new LyricLabel(lyricLines, size().width(), m_scrollWidget);
        lyricLabel->setAlignment(Qt::AlignLeft);  // 核心：歌词标签居左对齐
        m_expandLayout->addWidget(lyricLabel);  // 不设置居中，继承布局居左属性
        m_lyricsLabelList.append(lyricLabel);
        m_lyricsTimeList.append(line.lineStartTime);
    }

    // 添加底部空白行（保持居左）
    for (int i = 0; i < 6; ++i) {
        LyricLabel* blankLabel = new LyricLabel({"    "}, size().width(), this);
        blankLabel->setAlignment(Qt::AlignLeft);  // 空白标签居左
        m_expandLayout->addWidget(blankLabel);
    }
}

void LyricsWeight::updateLyrics(qint64 positionMs, qint64 durationMs)
{
    // 如果是默认的暂无歌词状态，不执行更新逻辑
    if (m_lyricData.isEmpty() && !m_lyricsLabelList.isEmpty() &&
        m_lyricsLabelList.first()->text().contains("暂无歌词")) {
        return;
    }

    // 检测到播放位置回退
    if (positionMs < m_lastPositionMs) {
        // 重置所有歌词状态
        for (LyricLabel* label : std::as_const(m_lyricsLabelList)) {
            label->setPlay(false);
        }

        // 重新查找当前应该高亮的歌词行
        int newPoint = 0;
        for (; newPoint < m_lyricsTimeList.size(); ++newPoint) {
            if (m_lyricsTimeList[newPoint] > positionMs) {
                break;
            }
        }
        m_lyricsNowPoint = newPoint;
    }

    // 正常播放时的歌词更新逻辑
    while (m_lyricsNowPoint < m_lyricsTimeList.size() && positionMs >= m_lyricsTimeList[m_lyricsNowPoint]) {
        // 上一行歌词恢复正常状态
        if (m_lyricsNowPoint > 0 && m_lyricsNowPoint - 1 < m_lyricsLabelList.size()) {
            m_lyricsLabelList[m_lyricsNowPoint - 1]->setPlay(false);
        }

        // 当前行歌词设置为播放状态
        if (m_lyricsNowPoint < m_lyricsLabelList.size()) {
            LyricLabel* currentLabel = m_lyricsLabelList[m_lyricsNowPoint];
            currentLabel->setPlay(true);

            // 计算滚动偏移量（居左状态下仍保持当前行垂直居中）
            QScrollBar* vScrollBar = this->verticalScrollBar();
            int labelY = currentLabel->y() - vScrollBar->value();
            int dy = labelY - (this->height() / 2) + (currentLabel->height() / 2);
            int targetValue = vScrollBar->value() + dy;

            m_scrollAni->setStartValue(vScrollBar->value());
            m_scrollAni->setEndValue(targetValue);
            m_scrollAni->start();
        }

        m_lyricsNowPoint++;
    }

    // 更新上一次播放位置
    m_lastPositionMs = positionMs;
}

void LyricsWeight::deleteItems()
{
    // 先清除所有标签引用
    m_lyricsLabelList.clear();
    m_lyricsTimeList.clear();

    // 清除布局中的所有部件
    QLayoutItem* item;
    while ((item = m_expandLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->setParent(nullptr); // 解除父对象关联
            item->widget()->deleteLater();     // 延迟删除
        }
        delete item; // 删除布局项
    }
}
