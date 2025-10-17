#ifndef PLAYLISTTOOLBAR_H
#define PLAYLISTTOOLBAR_H

#include <QWidget>
#include <ElaTableView.h>  // 假设ElaTableView继承自QTableView
#include "Tm_musicPlayListModel.h"  // 引入数据模型
#include "ElaTheme.h"      // 引入主题相关
#include <ElaMenu.h>           // 右键菜单头文件

class playListToolBar : public QWidget
{
    Q_OBJECT
public:
    explicit playListToolBar(QWidget *parent = nullptr);
    ~playListToolBar() override;

protected:
    void paintEvent(QPaintEvent* event) override;
    // 重写上下文菜单事件（右键触发）
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    // 菜单动作触发的槽函数
    void onDeleteActionTriggered();  // 删除选中歌曲
    void onClearActionTriggered();   // 清空列表
    void onTableItemDoubleClicked(const QModelIndex &index);

private:
    ElaTableView* m_musicPlayListView;
    Tm_musicPlayListModel* m_playListModel = Tm_musicPlayListModel::getInstance(this); // 播放列表模型（新增）
    ElaThemeType::ThemeMode m_themeMode;
    ElaMenu* m_contextMenu;  // 右键菜单对象
    QAction* m_deleteAction;  // "删除"动作
    QAction* m_clearAction;   // "清空列表"动作
};

#endif // PLAYLISTTOOLBAR_H
