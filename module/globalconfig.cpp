#include "GlobalConfig.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>

// 单例实例的静态成员变量初始化
GlobalConfig& GlobalConfig::getInstance()
{
    static GlobalConfig instance;
    return instance;
}

GlobalConfig::GlobalConfig()
{
    // 获取应用程序名称和组织名称
    QString organizationName = QCoreApplication::organizationName();
    QString applicationName = QCoreApplication::applicationName();

    // 如果未设置组织名称和应用名称，则使用默认值
    if (organizationName.isEmpty()) {
        organizationName = "CloudBread";
    }
    if (applicationName.isEmpty()) {
        applicationName = "CloudBreadMusic";
    }

#ifdef Q_OS_WIN
    // 在Windows上使用INI格式
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config.ini";
    m_settings = new QSettings(configPath, QSettings::IniFormat);
#else
    // 在其他系统上使用默认格式
    m_settings = new QSettings(organizationName, applicationName);
#endif

    // 确保配置文件所在目录存在
    QDir().mkpath(QFileInfo(m_settings->fileName()).absolutePath());

    qDebug() << "配置文件路径:" << m_settings->fileName();
}

GlobalConfig::~GlobalConfig()
{
    delete m_settings;
    m_settings = nullptr;
}

void GlobalConfig::setValue(const QString& key, const QVariant& value)
{
    if (m_settings) {
        m_settings->setValue(key, value);
    }
}

QVariant GlobalConfig::getValue(const QString& key, const QVariant& defaultValue)
{
    if (m_settings && m_settings->contains(key)) {
        return m_settings->value(key);
    }
    return defaultValue;
}

bool GlobalConfig::contains(const QString& key) const
{
    if (m_settings) {
        return m_settings->contains(key);
    }
    return false;
}

void GlobalConfig::remove(const QString& key)
{
    if (m_settings) {
        m_settings->remove(key);
    }
}

void GlobalConfig::sync()
{
    if (m_settings) {
        m_settings->sync();
    }
}
