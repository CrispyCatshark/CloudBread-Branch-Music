#ifndef MUSICLISTPAGE_H
#define MUSICLISTPAGE_H

#include <ElaScrollPage.h>
#include <ElaListView.h>
#include <ElaTableView.h>
#include <ElaToolButton.h>
#include <ElaText.h>
#include <ElaScrollPageArea.h>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include "musicimporter.h"
#include "musicplayerbase.h"
#include "searchpage.h"
#include "tm_listviewmodel.h"
#include "tm_musiclistviewmodel.h"  // 引入歌曲模型
#include "tm_musicplaylistmodel.h"

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/wavfile.h>
#include <taglib/mp4file.h>

class musicListPage : public ElaScrollPage
{
    Q_OBJECT

public:
    musicListPage(QWidget* parent = nullptr);
    ~musicListPage();

signals:
    void startImport(const QStringList& filePaths, const QString& playlistId);

private:
    musicplayerbase* _musicplayerbase;
    ElaToolButton* m_importBtn;
    ElaToolButton* m_addListBtn;
    ElaListView* m_listView;       // 歌单列表
    ElaScrollPageArea* m_listOperationArea;
    ElaText* m_musicListCoverLabel;
    ElaText* m_listNameTitle;
    ElaText* m_listInfo;
    ElaToolButton* m_listImportBtn;
    ElaToolButton* m_listPlayBtn;
    ElaToolButton* m_disruptSortBtn;
    ElaTableView* m_musicListView; // 歌曲列表
    Tm_listViewModel* m_listModel; // 歌单模型

    // 新增：歌曲数据模型指针（核心）
    Tm_musicListViewModel* m_musicModel;

    // 辅助函数
    QPixmap getRoundRectPixmap(QPixmap srcPixMap, const QSize& size, int radius);
    void initPage();
    void updateSelectedGroupInfo(const QString& groupId);

    // 新增：辅助函数 - 生成歌曲ID（UUID）
    QString generateMusicId() const;
    // 新增：辅助函数 - 解析音频文件获取时长（预留，实际需结合音频库）
    int getAudioDuration(const QString& filePath) const;

    QThread* m_importThread = nullptr;       // 导入子线程
    MusicImporter* m_musicImporter = nullptr;// 音乐导入器实例
    bool m_isImporting = false;              // 导入状态标记
    int m_clickedSongColumn = -1;
    Tm_musicPlayListModel* m_playListModel; // 播放列表模型（新增）
    searchPage *m_searchPage;
    ElaLineEdit* _searchEdit;
    ElaThemeType::ThemeMode _themeMode;
    QAction* _searchEdit_lightSearchAction{nullptr};
    QAction* _searchEdit_darkSearchAction{nullptr};

private slots:
    void onMusicListContextMenu(const QPoint& pos);
    void onSongListContextMenu(const QPoint& pos);
    void onMusicListModify();
    void onMusicListDelete();
    void onMusicListAddNew();
    void onSongImport();       // 歌曲导入（核心）
    void onSongNameModify();       // 歌曲修改
    void onSongAutherModify();       // 歌曲修改
    void onSongDelete();       // 歌曲删除
    void onDisruptSort();
    void onMusicListPlay();
    void onSongPlayNext();
    void onSongAddToQueue();
    void onSearchMusicLyric();
    void onMusicListItemClicked(const QModelIndex& index);
    void onThemeModeChanged(ElaThemeType::ThemeMode themeMode);
    void onSearchTextChanged(const QString &keyword);

    // 新增：歌曲播放次数更新（示例：点击歌曲时增加播放次数）
    /**
     * @brief 歌曲双击播放 - 直接播放选中歌曲（替换当前播放列表）
     */
    void onSongItemClicked(const QModelIndex& index);

    void onImportProgress(int importedCount, int totalCount);  // 导入进度更新
    void onImportFinished(int successCount, int totalCount);   // 导入完成
    void onImportError(const QString& errorMsg);               // 导入错误
};

#endif // MUSICLISTPAGE_H
