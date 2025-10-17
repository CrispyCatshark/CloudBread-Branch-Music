#include "webserver.h"
#include "ElaTheme.h"
#include "LrcParser.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QMimeDatabase>
#include <QLoggingCategory>
#include <QDir>
#include <QUrl>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include "GlobalConfig.h"

Q_LOGGING_CATEGORY(webServerLog, "web.server")

WebServer::WebServer(QObject *parent)
    : QObject(parent), m_server(new QHttpServer(this)), m_isRunning(false), m_port(0)
{
    // 设置通用路由处理所有GET请求
    m_server->route("/", QHttpServerRequest::Method::Get,
                    [this](const QHttpServerRequest &request) {
                        return handleRequest(request);
                    });
    connect(LyricFetcherThread::getInstance(),&LyricFetcherThread::lyricUpdated, this, &WebServer::lyricUpdate);
    m_server->route("/getLyric", QHttpServerRequest::Method::Get,
                    [this]() {
                        return m_lyricNow;
                    });
    m_server->route("/getTheme", QHttpServerRequest::Method::Get,
                    [this]() {
                        return (eTheme->getThemeMode() == ElaThemeType::Light)?"1":"0";
                    });
    m_server->route("/getWsPort", QHttpServerRequest::Method::Get,
                    [this]() {
                        return GlobalConfig::getInstance().getValue("Server/WsPort", 5310).toString();
                    });

    // 添加配置文件读取路由
    m_server->route("/get_config/<arg>", QHttpServerRequest::Method::Post,
                    [this](const QString &id) {
                        return handleGetConfig(id);
                    });

    // 添加配置文件写入路由
    m_server->route("/set_config/<arg>", QHttpServerRequest::Method::Post,
                    [this](const QString &id, const QHttpServerRequest &request) {
                        return handleSetConfig(id, request);
                    });
}

WebServer::~WebServer()
{
    stop();
    delete m_server;
}

void WebServer::lyricUpdate(const LyricData& lyricData) {
    // 创建歌词解析器实例
    LrcParser parser;

    m_lyricNow = lyricData.yrc;
}

void WebServer::setStaticRoot(const QString &path)
{
    m_staticRoot = QDir::cleanPath(path);
    qDebug() << "静态文件根目录设置为:" << m_staticRoot;

    discoverAndRegisterStaticPath("/");
}

void WebServer::registerDynamicPath(const QString &path, DynamicHandler handler)
{
    QString normalizedPath = path.startsWith('/') ? path : '/' + path;
    m_dynamicHandlers[normalizedPath] = std::move(handler);
    m_server->route(normalizedPath, QHttpServerRequest::Method::Get,
                    [this](const QHttpServerRequest &request) {
                        return handleRequest(request);
                    });
    qCInfo(webServerLog) << "已注册动态路径:" << normalizedPath;
}

void WebServer::discoverAndRegisterStaticPath(const QString &relativePath)
{
    // 构建完整路径
    QString fullPath = m_staticRoot + relativePath;
    QDir dir(fullPath);

    // 检查目录是否存在且可访问
    if (!dir.exists() || !dir.isReadable()) {
        qCWarning(webServerLog) << "目录不存在或不可读:" << fullPath;
        return;
    }

    // 为当前目录注册路由
    QString routePath = relativePath;
    if (routePath.isEmpty()) routePath = "/";

    m_server->route(routePath + "*", QHttpServerRequest::Method::Get,
                    [this, routePath](const QString &filename) {
                        QString fullFilePath = routePath + filename;
                        return handleStaticFile(fullFilePath);
                    });
    qCInfo(webServerLog) << "已自动注册静态路由:" << routePath;

    // 获取所有子目录并递归处理
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
    QFileInfoList subDirs = dir.entryInfoList();

    for (const QFileInfo &subDir : subDirs) {
        QString subDirRelativePath = routePath + subDir.fileName() + "/";
        discoverAndRegisterStaticPath(subDirRelativePath);
    }
}

bool WebServer::start(quint16 port)
{
    if (m_isRunning) {
        stop();
    }

    // 尝试在指定端口监听
    bool success = m_server->listen(QHostAddress::Any, port);
    if (success) {
        m_isRunning = true;
        m_port = port;
        qCInfo(webServerLog) << "服务器已启动，监听端口:" << port;
    } else {
        m_isRunning = false;
        m_port = 0;
        qDebug() << "服务器启动失败，无法监听端口:" << port;
    }

    return success;
}

void WebServer::stop()
{
    if (m_isRunning) {
        // 停止服务器的方法：重新创建服务器实例
        delete m_server;
        m_server = new QHttpServer(this);

        // 重新设置路由
        m_server->route("*", QHttpServerRequest::Method::Get,
                        [this](const QHttpServerRequest &request) {
                            return handleRequest(request);
                        });

        m_isRunning = false;
        m_port = 0;
        qCInfo(webServerLog) << "服务器已停止";
    }
}

bool WebServer::isRunning() const
{
    return m_isRunning;
}

QHttpServerResponse WebServer::handleRequest(const QHttpServerRequest &request)
{
    const QString path = request.url().path();
    qDebug() << "收到请求:" << path;

    // 检查是否是动态路径
    if (m_dynamicHandlers.contains(path)) {
        try {
            return m_dynamicHandlers[path](request);
        } catch (const std::exception &e) {
            qDebug() << "处理动态路径时出错" << path << ":" << e.what();
            return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
        }
    }

    // 处理静态文件
    return handleStaticFile(path);
}

QHttpServerResponse WebServer::handleStaticFile(const QString &path)
{
    // 处理根路径请求，默认返回index.html
    QString filePath = path.isEmpty() || path == "/" ? "/index.html" : path;

    // 构建完整文件路径
    QString fullPath = m_staticRoot + filePath;

    // 检查路径安全性，防止路径遍历攻击
    if (!isPathSafe(fullPath)) {
        qDebug() << "路径遍历攻击尝试:" << fullPath;
        return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
    }

    // 检查文件是否存在且是文件
    QFileInfo fileInfo(fullPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "文件不存在:" << fullPath;
        return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
    }

    // 尝试打开文件
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件:" << fullPath << "错误:" << file.errorString();
        return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
    }

    // 读取文件内容
    QByteArray content = file.readAll();
    file.close();

    // 获取MIME类型
    QByteArray mimeType = getMimeType(fileInfo.fileName()).toUtf8();

    // 构建并返回响应
    return QHttpServerResponse(mimeType, content, QHttpServerResponse::StatusCode::Ok);
}

// 处理配置文件读取请求
QHttpServerResponse WebServer::handleGetConfig(const QString &id)
{
    // 构建配置文件路径
    QString configFilePath = QString("%1/%2.json").arg(GlobalConfig::getInstance().getValue("App/DBpath","").toString()).arg(id);

    // 检查文件是否存在
    QFileInfo fileInfo(configFilePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qWarning() << "配置文件不存在:" << configFilePath;
        return QHttpServerResponse("application/json", "{\"msg\":\"configEmpty\",\"data\":\"\"}", QHttpServerResponse::StatusCode::Ok);
    }

    // 读取文件内容
    QFile file(configFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开配置文件:" << configFilePath << "错误:" << file.errorString();
        return QHttpServerResponse("application/json", "{\"msg\":\"configEmpty\",\"data\":\"\"}", QHttpServerResponse::StatusCode::Ok);
    }

    QByteArray content = file.readAll();
    file.close();

    // 返回JSON数据
    return QHttpServerResponse("application/json", content, QHttpServerResponse::StatusCode::Ok);
}

// 处理配置文件写入请求
QHttpServerResponse WebServer::handleSetConfig(const QString &id, const QHttpServerRequest &request)
{
    // 构建配置文件路径
    QString configFilePath = QString("%1/%2.json").arg(GlobalConfig::getInstance().getValue("App/DBpath","").toString()).arg(id);

    // 验证请求数据是否为JSON
    QByteArray requestData = request.body();
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(requestData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "无效的JSON数据:" << parseError.errorString();
        return QHttpServerResponse("application/json",
                                   QJsonDocument(QJsonObject{{"error", "无效的JSON数据"}}).toJson(),
                                   QHttpServerResponse::StatusCode::BadRequest);
    }

    // 写入文件
    QFile file(configFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "无法写入配置文件:" << configFilePath << "错误:" << file.errorString();
        return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
    }

    file.write(jsonDoc.toJson(QJsonDocument::Indented));
    file.close();

    // 返回成功响应
    QJsonObject response;
    response["status"] = "success";
    response["message"] = "配置已更新";
    response["id"] = id;

    return QHttpServerResponse("application/json",
                               QJsonDocument(response).toJson(),
                               QHttpServerResponse::StatusCode::Ok);
}

QString WebServer::getMimeType(const QString &fileName)
{
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(fileName);
    return mime.name();
}

bool WebServer::isPathSafe(const QString &path)
{
    // 检查路径是否在静态根目录下
    qDebug() << path;
    QFileInfo fileInfo(path);
    QString canonicalPath = fileInfo.absoluteFilePath();
    qDebug() << canonicalPath;
    return canonicalPath.startsWith(m_staticRoot);
}
