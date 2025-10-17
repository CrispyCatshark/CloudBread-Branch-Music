#include "playlisttoolbar.h"
#include <qboxlayout.h>
#include <qheaderview.h>
#include <QPalette>
#include <QPainter>
#include <QPainterPath>
#include <QContextMenuEvent>
#include <QMessageBox>
#include <qdebug.h>
#include <ElaMessageBar.h>

playListToolBar::playListToolBar(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setFixedSize(360, 500);

    // 1. 初始化数据模型（使用单例模式）
    m_playListModel = Tm_musicPlayListModel::getInstance(this);

    // 2. 创建布局和列表视图
    QVBoxLayout *contentLayout = new QVBoxLayout(this);
    contentLayout->setContentsMargins(0, 0, 0, 0);  // 去除布局边距

    m_musicPlayListView = new ElaTableView();
    m_musicPlayListView->horizontalHeader()->setVisible(false);
    m_musicPlayListView->verticalHeader()->setVisible(false);
    m_musicPlayListView->setAlternatingRowColors(false);
    m_musicPlayListView->setModel(m_playListModel);  // 绑定模型
    m_musicPlayListView->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_musicPlayListView->horizontalHeader()->setStretchLastSection(true);

    // 设置选择行为
    m_musicPlayListView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_musicPlayListView->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_musicPlayListView, &ElaTableView::tableViewShow, this, [=]() {
        m_musicPlayListView->setColumnWidth(0, 150);
        m_musicPlayListView->setColumnWidth(1, 100);
        m_musicPlayListView->setColumnWidth(2, 50);
        m_musicPlayListView->setColumnWidth(3, 50);
    });

    contentLayout->addWidget(m_musicPlayListView);

    // 3. 初始化右键菜单
    m_contextMenu = new ElaMenu(this);
    // 创建"删除"动作（快捷键Del，增强易用性）
    m_deleteAction = new QAction(tr("删除"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    // 创建"清空列表"动作
    m_clearAction = new QAction(tr("清空列表"), this);
    // 将动作添加到菜单
    m_contextMenu->addAction(m_deleteAction);
    m_contextMenu->addAction(m_clearAction);

    // 4. 绑定菜单动作到槽函数
    connect(m_deleteAction, &QAction::triggered, this, &playListToolBar::onDeleteActionTriggered);
    connect(m_clearAction, &QAction::triggered, this, &playListToolBar::onClearActionTriggered);
    connect(m_musicPlayListView, &ElaTableView::doubleClicked, this, &playListToolBar::onTableItemDoubleClicked);

    // 5. 主题相关初始化
    m_themeMode = eTheme->getThemeMode();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        m_themeMode = themeMode;
        update();
    });

    // 初始加载播放列表（确保界面有数据）
    m_playListModel->loadPlayList();
}

playListToolBar::~playListToolBar()
{
    // 菜单和动作由parent管理，无需手动delete
}

// 重写绘制事件：绘制圆角背景
void playListToolBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QRectF widgetRect = rect();
    QPainterPath path;
    path.addRoundedRect(widgetRect, 10, 10);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ElaThemeColor(m_themeMode, WindowBase));
    painter.drawPath(path);
    painter.setClipPath(path);
    QWidget::paintEvent(event);
}

// 重写上下文菜单事件：右键时显示菜单
void playListToolBar::contextMenuEvent(QContextMenuEvent *event)
{
    // 过滤非列表区域的右键（仅在列表上右键显示菜单）
    if (m_musicPlayListView->underMouse()) {
        // 根据是否有选中项，动态启用/禁用"删除"动作（优化用户体验）
        m_deleteAction->setEnabled(!m_musicPlayListView->selectionModel()->selectedRows().isEmpty());
        // 在鼠标位置显示菜单
        m_contextMenu->exec(event->globalPos());
    }
    QWidget::contextMenuEvent(event);
}

// 槽函数：处理"删除"动作
void playListToolBar::onDeleteActionTriggered()
{
    // 获取选中行的索引
    QModelIndexList selectedRows = m_musicPlayListView->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        // 使用ElaMessageBar替换QMessageBox::warning
        ElaMessageBar::warning(
            ElaMessageBarType::Top,  // 顶部显示
            tr("提示"),
            tr("请先选中要删除的歌曲！"),
            3000
            );
        return;
    }

    // 从模型中获取选中行的sort_id（删除接口依赖sort_id）
    QModelIndex selectedIndex = selectedRows.first();
    const PlayListItem& item = m_playListModel->getPlayItem(selectedIndex.row());
    int targetSortId = item.sortId;

    // 调用模型的删除接口
    bool deleteOk = m_playListModel->deleteMusicBySortId(targetSortId);
    if (!deleteOk) {
        // 使用ElaMessageBar替换QMessageBox::critical
        ElaMessageBar::error(
            ElaMessageBarType::Top,
            tr("删除失败"),
            tr("删除歌曲出错：%1").arg(m_playListModel->getLastError()),
            3000
            );
        qWarning() << "Delete failed:" << m_playListModel->getLastError();
    } else {
        // 使用ElaMessageBar添加删除成功提示
        ElaMessageBar::success(
            ElaMessageBarType::Top,
            tr("删除成功"),
            tr("歌曲已从播放列表中移除"),
            2000
            );
        qDebug() << "Delete success, sort_id:" << targetSortId;
    }
}

// 槽函数：处理"清空列表"动作
void playListToolBar::onClearActionTriggered()
{
    // 保留QMessageBox确认框（重要操作需要用户明确确认）
    QMessageBox::StandardButton ret = QMessageBox::question(
        this,
        tr("确认清空"),
        tr("是否要清空整个播放列表？此操作不可撤销！"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No  // 默认选中"取消"
        );

    if (ret != QMessageBox::Yes) {
        return;  // 用户取消，不执行清空
    }

    // 调用模型的清空接口
    bool clearOk = m_playListModel->clearPlayList();
    if (!clearOk) {
        // 使用ElaMessageBar替换QMessageBox::critical
        ElaMessageBar::error(
            ElaMessageBarType::Top,
            tr("清空失败"),
            tr("清空列表出错：%1").arg(m_playListModel->getLastError()),
            3000
            );
        qWarning() << "Clear failed:" << m_playListModel->getLastError();
    } else {
        // 使用ElaMessageBar替换QMessageBox::information
        ElaMessageBar::success(
            ElaMessageBarType::Top,
            tr("清空成功"),
            tr("播放列表已清空！"),
            2000
            );
        qDebug() << "Clear play list success";
    }
}

// 双击列表行触发播放
void playListToolBar::onTableItemDoubleClicked(const QModelIndex &index)
{
    // 校验索引有效性（排除无效行/列）
    if (!index.isValid() || index.row() >= m_playListModel->rowCount()) {
        return;
    }

    // 1. 从模型获取双击行的播放项数据（需模型提供getPlayItem接口）
    const PlayListItem& item = m_playListModel->getPlayItem(index.row());
    int targetSortId = item.sortId;

    // 2. 调用模型的切换播放接口（原模型已实现switchToMusic，直接复用）
    bool switchOk = m_playListModel->switchToMusic(targetSortId);
    if (!switchOk) {
        // 播放失败：显示错误提示
        ElaMessageBar::error(
            ElaMessageBarType::Top,
            tr("播放失败"),
            tr("切换歌曲出错：%1").arg(m_playListModel->getLastError()),
            3000
            );
        qWarning() << "Play failed, sort_id:" << targetSortId << ", error:" << m_playListModel->getLastError();
    } else {
        // 播放成功：显示成功提示（可选，提升用户体验）
        const QString musicName = m_playListModel->data(index, Qt::DisplayRole).toString();
        ElaMessageBar::success(
            ElaMessageBarType::Top,
            tr("播放中"),
            tr("正在播放：%1").arg(musicName),
            2000
            );
        qDebug() << "Play success, sort_id:" << targetSortId << ", music:" << musicName;
    }
}
