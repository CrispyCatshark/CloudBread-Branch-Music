#ifndef SQLITEMANAGER_H
#define SQLITEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QMutex>
#include <QVector>
#include <QMap>
#include <QVariant>

class SqliteManager
{
public:
    // 获取单例实例
    static SqliteManager& getInstance();

    // 禁止拷贝和赋值
    SqliteManager(const SqliteManager&) = delete;
    SqliteManager& operator=(const SqliteManager&) = delete;

    // 初始化数据库
    bool init(const QString& dbName);

    // 关闭数据库
    void close();

    // 执行SQL语句（无参数）
    bool executeSql(const QString& sql);

    // 执行SQL语句（带参数）
    bool executeSql(const QString& sql, const QVector<QVariant>& params);

    // 查询SQL语句（无参数）
    QVector<QMap<QString, QVariant>> querySql(const QString& sql);

    // 查询SQL语句（带参数）
    QVector<QMap<QString, QVariant>> querySql(const QString& sql, const QVector<QVariant>& params);

    // 获取最后一次错误信息
    QString getLastError() const;

    // 事务操作
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // 检查表是否存在
    bool tableExists(const QString& tableName);

    // 获取最后插入的ID
    qint64 getLastInsertId() const;

private:
    // 私有构造函数和析构函数
    SqliteManager();
    ~SqliteManager();

    QSqlDatabase m_db;          // 数据库连接
    mutable QMutex m_mutex;     // 互斥锁，保证线程安全
    QString m_lastError;        // 最后一次错误信息
    qint64 m_lastInsertId;      // 最后插入的ID
};

#endif // SQLITEMANAGER_H
