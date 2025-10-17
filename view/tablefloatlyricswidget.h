#ifndef TABLEFLOATLYRICSWIDGET_H
#define TABLEFLOATLYRICSWIDGET_H

#include <QWidget>
#include <QSettings>
#include <QPropertyAnimation>
#include <QRect>
#include "LyricFetcherThread.h"
#include "lyricwidget.h"
#include "ElaTheme.h"

class tableFloatLyricsWidget : public QWidget
{
    Q_OBJECT

public:
    // 单例获取接口
    static tableFloatLyricsWidget* getInstance(QWidget* parent = nullptr);
    ~tableFloatLyricsWidget();

    // 歌词显示/隐藏控制
    void showLyrics();
    void hideLyrics();
    void toggleLyrics();

private:
    // 单例私有构造
    explicit tableFloatLyricsWidget(QWidget* parent = nullptr);
    static tableFloatLyricsWidget* m_instance;  // 单例实例

    // 初始化界面
    void initPage();
    // 从GlobalConfig加载窗口状态（位置+显示状态）
    void loadStateFromConfig();
    // 保存窗口状态到GlobalConfig
    void saveStateToConfig();
    // 获取当前窗口所在屏幕的几何信息（适配多屏幕）
    QRect getScreenGeometry() const;
    // 显示/隐藏逻辑（含动画）
    void hideOrShow(bool show);
    // 启动位置动画
    void startAnimation(int x, int y);

private slots:
    // 歌词数据更新
    void lyricUpdate(const LyricData& lyricData);
    // 主题变化响应
    void onThemeChanged(ElaThemeType::ThemeMode themeMode);

protected:
    // 事件重写
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // 原有成员变量
    LyricsWeight* m_lyricsWeight;       // 歌词显示控件
    LyricData m_lyricData;              // 缓存的歌词数据
    ElaThemeType::ThemeMode m_themeMode;// 当前主题模式
    bool m_isDragging;                  // 是否正在拖动窗口
    QPoint m_dragStartGlobalPos;        // 拖动起始的全局位置
    QPoint m_windowStartPos;            // 窗口起始位置
    QPropertyAnimation* m_animation;    // 位置动画对象
    bool m_moved;                       // 窗口是否处于"部分隐藏"状态

    // 新增：状态保存相关标记
    bool m_isStateLoaded;               // 是否已从配置加载状态（避免重复加载）
    const QString m_configGroup = "FloatLyricsWidget";  // 配置分组（避免键名冲突）
    // 配置键名（统一管理，便于维护）
    const QString m_keyPosX = "posX";          // 窗口X坐标键
    const QString m_keyPosY = "posY";          // 窗口Y坐标键
    const QString m_keyIsVisible = "isVisible";// 窗口显示状态键
};

#endif // TABLEFLOATLYRICSWIDGET_H
