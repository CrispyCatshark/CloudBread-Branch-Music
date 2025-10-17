#include "SqliteManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>

SqliteManager::SqliteManager()
{
    // 构造函数初始化
}

SqliteManager::~SqliteManager()
{
    close();
}

SqliteManager& SqliteManager::getInstance()
{
    static SqliteManager instance;
    return instance;
}

bool SqliteManager::init(const QString& dbName)
{
    QMutexLocker locker(&m_mutex);

    // 如果已经打开则先关闭
    if (m_db.isOpen()) {
        m_db.close();
    }

    // 添加SQLite数据库驱动
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbName);

    // 打开数据库
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        qWarning() << "数据库打开失败:" << m_lastError;
        return false;
    }

    m_lastError.clear();
    qDebug() << "数据库打开成功:" << dbName;
    return true;
}

void SqliteManager::close()
{
    QMutexLocker locker(&m_mutex);

    if (m_db.isOpen()) {
        m_db.close();
        qDebug() << "数据库已关闭";
    }
}

bool SqliteManager::executeSql(const QString& sql)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "执行SQL失败:" << m_lastError << "SQL:" << sql;
        return false;
    }

    // 保存最后插入的ID
    m_lastInsertId = query.lastInsertId().toLongLong();

    m_lastError.clear();
    return true;
}

QVector<QMap<QString, QVariant>> SqliteManager::querySql(const QString& sql)
{
    QMutexLocker locker(&m_mutex);
    QVector<QMap<QString, QVariant>> result;

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return result;
    }

    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        qWarning() << "查询SQL失败:" << m_lastError << "SQL:" << sql;
        return result;
    }

    // 获取查询结果
    QSqlRecord record = query.record();
    int columnCount = record.count();

    while (query.next()) {
        QMap<QString, QVariant> row;
        for (int i = 0; i < columnCount; ++i) {
            QString columnName = record.fieldName(i);
            QVariant value = query.value(i);
            row[columnName] = value;
        }
        result.append(row);
    }

    m_lastError.clear();
    return result;
}

QVector<QMap<QString, QVariant>> SqliteManager::querySql(const QString& sql, const QVector<QVariant>& params)
{
    QMutexLocker locker(&m_mutex);
    QVector<QMap<QString, QVariant>> result;

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(sql);

    // 绑定参数
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "查询SQL失败:" << m_lastError << "SQL:" << sql;
        return result;
    }

    // 获取查询结果
    QSqlRecord record = query.record();
    int columnCount = record.count();

    while (query.next()) {
        QMap<QString, QVariant> row;
        for (int i = 0; i < columnCount; ++i) {
            QString columnName = record.fieldName(i);
            QVariant value = query.value(i);
            row[columnName] = value;
        }
        result.append(row);
    }

    m_lastError.clear();
    return result;
}

bool SqliteManager::executeSql(const QString& sql, const QVector<QVariant>& params)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(sql);

    // 绑定参数
    for (int i = 0; i < params.size(); ++i) {
        query.bindValue(i, params[i]);
    }

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "执行SQL失败:" << m_lastError << "SQL:" << sql;
        return false;
    }

    // 保存最后插入的ID
    m_lastInsertId = query.lastInsertId().toLongLong();

    m_lastError.clear();
    return true;
}

QString SqliteManager::getLastError() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

bool SqliteManager::beginTransaction()
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    return m_db.transaction();
}

bool SqliteManager::commitTransaction()
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    return m_db.commit();
}

bool SqliteManager::rollbackTransaction()
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    return m_db.rollback();
}

bool SqliteManager::tableExists(const QString& tableName)
{
    QMutexLocker locker(&m_mutex);

    if (!m_db.isOpen()) {
        m_lastError = "数据库未打开";
        return false;
    }

    QString sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=?";
    QSqlQuery query(m_db);
    query.prepare(sql);
    query.bindValue(0, tableName);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    return query.next();
}

qint64 SqliteManager::getLastInsertId() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastInsertId;  // 返回保存的最后插入ID
}
