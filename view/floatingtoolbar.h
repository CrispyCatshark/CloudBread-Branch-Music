#ifndef FLOATINGTOOLBAR_H
#define FLOATINGTOOLBAR_H

#include "playlisttoolbar.h"
#include "searchpage.h"
#include <QWidget>
#include <QToolButton>
#include <QHBoxLayout>
#include <QTimer>
#include <ElaText.h>
#include <ElaPushButton.h>
#include <ElaToolButton.h>
#include  <QPropertyAnimation>

class FloatingToolbar : public QWidget
{
    Q_OBJECT

enum class PlayMode {
    PlayNow,    // 立即播放
    PlayNext,   // 下一首播放
    AddToList   // 添加到列表
};

public:
    explicit FloatingToolbar(QWidget *parent = nullptr);
    ~FloatingToolbar() override;

signals:
    void showMainPage();
    void groupRandomClicked(const QString& groupId);
    void groupOrderByIdClicked(const QString& groupId, const int id);

private:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    void initUI();
    void initToolBar();
    void initAnimations();
    void startEnterAnimation();

    // 播放队列按钮点击
    void onQueueButtonClicked();
    void showOrderSelectMenu();
    void setPlayState(bool isPlaying);
    void showSongIndexDialog(const QString& groupId, const QString& groupName, PlayMode playMode);

    QPixmap getRoundRectPixmap(QPixmap srcPixMap, const QSize & size, int radius);

    ElaText *m_coverLabel;
    // QWidget *m_ToolBar;
    bool outFlag;
    QTimer *m_hideTimer;
    ElaToolButton *m_playPauseBtn;
    ElaToolButton *m_homeBtn;
    ElaToolButton *m_orderBtn;
    ElaToolButton *m_listBtn;
    ElaToolButton *m_tableLyricsBtn;
    QWidget *m_btnContainer;          // 按钮容器
    QPropertyAnimation *m_slideInAnimation;  // 滑入动画
    QPropertyAnimation *m_slideOutAnimation; // 滑出动画
    QPoint m_initialBtnPos;           // 按钮初始位置
    searchPage *m_searchPage;

    playListToolBar *m_playListToolBar;
};

#endif // FLOATINGTOOLBAR_H
