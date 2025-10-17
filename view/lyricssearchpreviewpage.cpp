#include "lyricssearchpreviewpage.h"
#include "ElaScrollArea.h"
#include <qboxlayout.h>
#include <QScrollArea>
#include <QWidget>

lyricsSearchPreviewPage::lyricsSearchPreviewPage(QWidget *parent)
    : ElaWidget(parent)
{
    initPage();  // 修正原代码中的拼写错误 intPage -> initPage
}

void lyricsSearchPreviewPage::initPage()  // 修正原代码中的拼写错误 intPage -> initPage
{
    resize(800, 500);
    moveToCenter();
    QString titleText = "歌词预览";
    setWindowTitle(titleText);
    setWindowIcon(QIcon(":/cloudbread/src/favicon_round.png"));
    setWindowFlags(Qt::WindowStaysOnTopHint);
    setVisible(false);

    // 主布局，设置边距为0，确保内容填满整个窗口
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建滚动区域，作为歌词的容器
    QScrollArea* scrollArea = new QScrollArea(this);
    // 设置滚动区域属性
    scrollArea->setWidgetResizable(true);  // 使内部部件可以随滚动区域大小变化
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);  // 禁用水平滚动条
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);     // 按需显示垂直滚动条
    scrollArea->setContentsMargins(10, 10, 10, 10);  // 内部边距，让文本与边框有间距

    // 创建滚动区域的内部部件，用于承载歌词文本
    QWidget* scrollContent = new QWidget(scrollArea);
    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    // 歌词文本框
    m_layricText = new ElaText();
    m_layricText->setWordWrap(true);  // 自动换行
    m_layricText->setMinimumWidth(0); // 允许文本框宽度自适应
    m_layricText->setTextPixelSize(14);
    // m_layricText->setStyleSheet("font-size: 14px; line-height: 1.8;");  // 可选：调整字体和行高

    // 将文本框添加到内容布局
    contentLayout->addWidget(m_layricText);
    // 添加伸缩项，确保文本框上方填满空间
    contentLayout->addStretch();

    // 设置滚动区域的内部部件
    scrollArea->setWidget(scrollContent);

    // 将滚动区域添加到主布局
    mainLayout->addWidget(scrollArea);
}

void lyricsSearchPreviewPage::setLayrics(QString lyricText)
{
    m_layricText->setText(lyricText);
}
