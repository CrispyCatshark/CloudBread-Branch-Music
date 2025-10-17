#ifndef TM_LISTVIEWMODEL_H
#define TM_LISTVIEWMODEL_H

#include <QAbstractListModel>
#include "MusicDataStruct.h"
#include "SqliteManager.h"

class Tm_listViewModel : public QAbstractListModel
{
    Q_OBJECT
public:
    // 自定义角色（用于获取歌单ID、封面等数据）
    enum GroupRole {
        IdRole = Qt::UserRole + 1,    // 歌单ID
        NameRole,                     // 歌单名称
        ImgRole                       // 歌单封面
    };

    explicit Tm_listViewModel(QObject *parent = nullptr);

    // 重写QAbstractListModel纯虚函数
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 歌单增删查改接口
    bool loadAllGroups();                  // 加载所有歌单
    bool addGroup(const GroupItem &group); // 添加歌单（需传入完整GroupItem）
    bool addGroup(const QString &name);    // 简化接口：仅传名称（封面用默认）
    bool deleteGroup(const QString &groupId); // 删除歌单（含关联歌曲）
    bool updateGroup(const GroupItem &group); // 更新歌单（名称/封面）
    GroupItem getGroupById(const QString &groupId) const; // 按ID获取歌单
    int getGroupMusicCount(const QString &groupId) const; // 获取歌单歌曲数量
    QString getRandomMusicCoverByGroupId(const QString& groupId);

private:
    QVector<GroupItem> m_groupItems;       // 存储所有歌单数据
    SqliteManager &m_db = SqliteManager::getInstance(); // 数据库单例
};

#endif // TM_LISTVIEWMODEL_H
