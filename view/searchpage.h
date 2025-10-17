#ifndef SEARCHPAGE_H
#define SEARCHPAGE_H

#include "ElaLineEdit.h"
#include "ElaWidget.h"
#include "ElaScrollPageArea.h"
#include "ElaTableView.h"
#include "ElaMenu.h"
#include "MusicSearchTableModel.h"
#include "TM_musicPlayListModel.h"
#include "LyricSearchTableModel.h"
#include "lyricssearchpreviewpage.h"

class searchPage : public ElaWidget
{
    Q_OBJECT
public:
    explicit searchPage(int type, QWidget *parent = nullptr);
    ~searchPage() override;

    void setMusicID(QString musicId);

signals:
    // 透传播放列表的播放信号（供外部使用）
    void playMusic(const QString& musicId);
    // 歌词操作信号（预留：预览/设置歌词）
    void previewLyric(const QString& lyricId);  // lyricId：歌词唯一标识
    void setLyricConfig(const QString& lyricId);
    void fetchLyricByIndex(const QModelIndex& index);

private slots:
    // ---------- 歌曲搜索菜单槽函数（与播放列表交互） ----------
    void playImmediately();       // 立即播放
    void playNext();              // 下一首播放
    void addToPlaylist();         // 添加到播放列表末尾
    // ---------- 歌词搜索菜单槽函数（预留实现） ----------
    void previewLyricSlot();      // 预览歌词
    void setLyricSlot();          // 设置歌词

private:
    void intPage();
    // 初始化差异化模式
    void initMusicSearchMode();   // 初始化歌曲搜索模式（m_type=0）
    void initLyricSearchMode();   // 初始化歌词搜索模式（m_type=1）
    // 创建上下文菜单（根据类型区分菜单项）
    void createContextMenu();
    // 菜单显示逻辑
    void showContextMenu(const QPoint& pos);
    void showContextMenuForIndex(const QModelIndex& index);

private:
    ElaLineEdit* m_searchInput;
    int m_type;                       // 0=歌曲搜索，1=歌词搜索
    ElaScrollPageArea* m_searchArea{nullptr};  // 搜索区域
    ElaTableView* m_tableView{nullptr};        // 表格视图
    ElaMenu* m_contextMenu{nullptr};           // 上下文菜单
    QModelIndex m_currentIndex;                // 当前选中的表格项索引

    QString m_music_id;
    lyricsSearchPreviewPage* lyricWindow;

    // 双模型适配
    MusicSearchTableModel* m_musicModel{nullptr};  // 歌曲搜索模型
    LyricSearchTableModel* m_lyricModel = LyricSearchTableModel::getInstance();  // 歌词搜索模型

    // 播放列表单例引用（核心交互对象）
    Tm_musicPlayListModel* m_playListModel;
};

#endif // SEARCHPAGE_H
