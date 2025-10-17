#include "WsServer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QHostAddress>
#include "LyricFetcherThread.h"
#include <cmath>
#include "musicplayer.h"
// WsServer 实现

WsServer::WsServer(QObject *parent)
    : QObject(parent),
    m_webSocketServer(nullptr)
{
    // 初始化WebSocket服务器
    m_webSocketServer = new QWebSocketServer(
        QStringLiteral("WS Server"),
        QWebSocketServer::NonSecureMode,
        this
        );

    // 连接新连接信号
    connect(m_webSocketServer, &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);

    // 连接服务器错误信号
    connect(m_webSocketServer, &QWebSocketServer::serverError,
            this, [this]() {
                emit errorOccurred(m_webSocketServer->errorString());
            });

    connect(LyricFetcherThread::getInstance(),&LyricFetcherThread::lyricUpdated, this, &WsServer::lyricUpdate);
    connect(eTheme, &ElaTheme::themeModeChanged, this, &WsServer::onThemeChanged);
    connect(MusicPlayer::getInstance(), &MusicPlayer::songInfoUpdated, this, &WsServer::onSongInfoUpdated);
    connect(MusicPlayer::getInstance(), &MusicPlayer::progressUpdated, this, &WsServer::updatePosition);
    connect(MusicPlayer::getInstance(), &MusicPlayer::playStateChanged, this, &WsServer::playStateChanged);
}

WsServer::~WsServer()
{
    stop();
}

void WsServer::playStateChanged(bool isPlaying)
{
    QJsonObject wsData;
    QJsonObject data;
    wsData.insert("type", "signal");
    wsData.insert("msg", "StatusUpdata");
    data.insert("isplaying", isPlaying);
    wsData.insert("data", data);
    sendToAllClients(wsData);
}

void WsServer::updatePosition(qint64 positionMs, qint64 durationMs)
{
    if (abs(positionMs - lastPosition) > 1000) {
        lastPosition = positionMs;
        QJsonObject wsData;
        QJsonObject data;
        wsData.insert("type", "signal");
        wsData.insert("msg", "PositionUpdata");
        data.insert("position", positionMs);
        data.insert("duration", durationMs);
        wsData.insert("data", data);
        sendToAllClients(wsData);
    }
}

void WsServer::onSongInfoUpdated(const QString& auther, const QString& name, const QPixmap& cover, int pitch, bool hasLyrics)
{
    QJsonObject wsData;
    QJsonObject data;
    wsData.insert("type", "signal");
    wsData.insert("msg", "MusicInfoUpdata");
    data.insert("name", name);
    data.insert("auther", auther);
    data.insert("pitch", pitch);
    wsData.insert("data", data);
    sendToAllClients(wsData);
}

void WsServer::positionUpdate(const LyricData& lyricData) {
    QJsonObject data;
    data.insert("type", "signal");
    data.insert("msg", "LyricUpdata");
    sendToAllClients(data);
}

void WsServer::onThemeChanged(ElaThemeType::ThemeMode themeMode)
{
    QJsonObject data;
    data.insert("type", "signal");
    data.insert("msg", "ThemeUpdata");
    sendToAllClients(data);
}

void WsServer::lyricUpdate(const LyricData& lyricData) {
    QJsonObject data;
    data.insert("type", "signal");
    data.insert("msg", "LyricUpdata");
    sendToAllClients(data);
}

WsServer& WsServer::getInstance()
{
    static WsServer instance;
    return instance;
}

bool WsServer::start(quint16 port)
{
    if (m_webSocketServer->isListening()) {
        emit errorOccurred("WebSocket server is already running");
        return false;
    }

    // 开始监听指定端口
    if (m_webSocketServer->listen(QHostAddress::Any, port)) {
        qDebug() << "WebSocket server started on port" << port;
        emit serverStarted(port);
        return true;
    } else {
        emit errorOccurred(QString("Failed to start WebSocket server: %1")
                               .arg(m_webSocketServer->errorString()));
        return false;
    }
}

void WsServer::stop()
{
    if (m_webSocketServer->isListening()) {
        m_webSocketServer->close();
    }

    // 断开所有客户端连接
    QMutexLocker locker(&m_mutex);
    for (QWebSocket* client : m_clients) {
        client->close(QWebSocketProtocol::CloseCodeNormal, "WebSocket server stopped");
    }
    m_clients.clear();

    emit serverStopped();
    qDebug() << "WebSocket server stopped";
}

void WsServer::sendToAllClients(const QJsonObject& json)
{
    QJsonDocument doc(json);
    QString message = doc.toJson(QJsonDocument::Compact);

    QMutexLocker locker(&m_mutex);
    for (QWebSocket* client : m_clients) {
        sendToClient(client, json);
    }
}

void WsServer::sendToClient(QWebSocket* client, const QJsonObject& json)
{
    if (!client || client->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonDocument doc(json);
    QString message = doc.toJson(QJsonDocument::Compact);
    client->sendTextMessage(message);
}

void WsServer::onNewConnection()
{
    QWebSocket* pSocket = m_webSocketServer->nextPendingConnection();
    if (!pSocket) {
        return;
    }

    qDebug() << "New client connected:" << pSocket->peerAddress().toString();

    // 连接客户端信号
    connect(pSocket, &QWebSocket::textMessageReceived,
            this, &WsServer::onTextMessageReceived);
    connect(pSocket, &QWebSocket::disconnected,
            this, &WsServer::onDisconnected);
    connect(pSocket, &QWebSocket::errorOccurred,
            this, [this, pSocket]() {
                emit errorOccurred(QString("Client %1 error: %2")
                                       .arg(pSocket->peerAddress().toString(), pSocket->errorString()));
            });

    // 将客户端添加到列表
    QMutexLocker locker(&m_mutex);
    m_clients << pSocket;

    emit clientConnected(pSocket);
}

void WsServer::onTextMessageReceived(const QString& message)
{
    QWebSocket* pClient = qobject_cast<QWebSocket*>(sender());

    if (message.toUtf8() == "PING") {
        if (!pClient || pClient->state() != QAbstractSocket::ConnectedState) {
            return;
        }

        pClient->sendTextMessage("PONG");
        return;
    }

    // 解析JSON消息
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        emit errorOccurred("Received invalid JSON message");

        // 可以向客户端发送错误响应
        if (pClient) {
            QJsonObject errorJson;
            errorJson["type"] = "error";
            errorJson["message"] = "Invalid JSON format";
            sendToClient(pClient, errorJson);
        }
        return;
    }

    QJsonObject json = doc.object();
    sendToAllClients(json);
    emit messageReceived(pClient, json);
}

void WsServer::onDisconnected()
{
    QWebSocket* pClient = qobject_cast<QWebSocket*>(sender());
    if (pClient) {
        qDebug() << "Client disconnected:" << pClient->peerAddress().toString();

        QMutexLocker locker(&m_mutex);
        m_clients.removeAll(pClient);
        emit clientDisconnected(pClient);

        pClient->deleteLater();
    }
}

// WsServerThread 实现

WsServerThread::WsServerThread(QObject *parent)
    : QObject(parent)
{
    // 将WebSocket服务器移动到新线程
    WsServer::getInstance().moveToThread(&m_thread);

    // 连接启动和停止信号到服务器的对应方法
    connect(this, &WsServerThread::startServerSignal,
            &WsServer::getInstance(), &WsServer::start);
    connect(this, &WsServerThread::stopServerSignal,
            &WsServer::getInstance(), &WsServer::stop);

    // 启动线程
    m_thread.start();
}

WsServerThread& WsServerThread::getInstance()
{
    static WsServerThread instance;
    return instance;
}

void WsServerThread::startServer(quint16 port)
{
    emit startServerSignal(port);
}

void WsServerThread::stopServer()
{
    emit stopServerSignal();
    m_thread.quit();
    m_thread.wait();
}
