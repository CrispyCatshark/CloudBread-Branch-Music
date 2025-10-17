#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <functional>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include "LyricFetcherThread.h"

class WebServer : public QObject
{
    Q_OBJECT
public:
    using DynamicHandler = std::function<QHttpServerResponse(const QHttpServerRequest&)>;

    explicit WebServer(QObject *parent = nullptr);
    ~WebServer() override;

    // 设置静态文件根目录
    void setStaticRoot(const QString &path);

    // 注册动态路径处理器
    void registerDynamicPath(const QString &path, DynamicHandler handler);

    // 启动服务器，返回是否成功
    bool start(quint16 port);

    // 停止服务器
    void stop();

    // 检查服务器是否在运行
    bool isRunning() const;

private:
    void lyricUpdate(const LyricData& lyricData);
    void discoverAndRegisterStaticPath(const QString &relativePath);
    QHttpServerResponse handleSetConfig(const QString &id, const QHttpServerRequest &request);
    QHttpServerResponse handleGetConfig(const QString &id);

    // 处理所有请求
    QHttpServerResponse handleRequest(const QHttpServerRequest &request);

    // 处理静态文件请求
    QHttpServerResponse handleStaticFile(const QString &path);

    // 获取文件的MIME类型
    QString getMimeType(const QString &fileName);

    // 检查路径安全性，防止路径遍历攻击
    bool isPathSafe(const QString &path);

private:
    QString m_lyricNow;
    QHttpServer *m_server;          // HTTP服务器实例
    QString m_staticRoot;           // 静态文件根目录
    QMap<QString, DynamicHandler> m_dynamicHandlers;  // 动态路径处理器映射
    quint16 m_port;                 // 监听端口
    bool m_isRunning;               // 服务器运行状态
};

#endif // WEBSERVER_H
