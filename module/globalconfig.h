#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include <QSettings>
#include <QVariant>
#include <QString>

/**
 * @brief 全局配置管理类，基于QSettings实现，采用单例模式
 */
class GlobalConfig
{
public:
    // 禁用拷贝构造和赋值操作
    GlobalConfig(const GlobalConfig&) = delete;
    GlobalConfig& operator=(const GlobalConfig&) = delete;

    /**
     * @brief 获取单例实例
     * @return 全局唯一的配置实例
     */
    static GlobalConfig& getInstance();

    /**
     * @brief 设置配置项的值
     * @param key 配置项键名，支持分组如"Database/Host"
     * @param value 配置项的值
     */
    void setValue(const QString& key, const QVariant& value);

    /**
     * @brief 获取配置项的值（带默认值）
     * @param key 配置项键名
     * @param defaultValue 当配置项不存在时返回的默认值
     * @return 配置项的值，若不存在则返回默认值
     */
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant());

    /**
     * @brief 检查配置项是否存在
     * @param key 配置项键名
     * @return 存在返回true，否则返回false
     */
    bool contains(const QString& key) const;

    /**
     * @brief 移除配置项
     * @param key 配置项键名
     */
    void remove(const QString& key);

    /**
     * @brief 立即同步配置到文件
     */
    void sync();

private:
    // 私有构造函数，确保只能通过getInstance获取实例
    GlobalConfig();
    ~GlobalConfig();

    QSettings* m_settings;  // QSettings实例
};

#endif // GLOBAL_CONFIG_H
