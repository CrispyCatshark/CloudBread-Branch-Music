#ifndef WSSERVER_H
#define WSSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QThread>
#include <QJsonObject>
#include <QMutex>
#include <memory>
#include "LyricFetcherThread.h"
#include "ElaTheme.h"

/**
 * @brief WebSocket服务器类，运行在独立线程中，采用单例模式
 * 提供WebSocket服务，使用JSON格式通信，通过信号传递消息
 */
class WsServer : public QObject
{
    Q_OBJECT
public:
    // 单例模式，获取唯一实例
    static WsServer& getInstance();

    // 禁止拷贝和移动
    WsServer(const WsServer&) = delete;
    WsServer& operator=(const WsServer&) = delete;
    WsServer(WsServer&&) = delete;
    WsServer& operator=(WsServer&&) = delete;

    /**
     * @brief 启动WebSocket服务器
     * @param port 监听端口
     * @return 是否启动成功
     */
    bool start(quint16 port);

    /**
     * @brief 停止WebSocket服务器
     */
    void stop();

    /**
     * @brief 向所有连接的客户端发送JSON消息
     * @param json 要发送的JSON对象
     */
    void sendToAllClients(const QJsonObject& json);

    /**
     * @brief 向指定客户端发送JSON消息
     * @param client 目标客户端
     * @param json 要发送的JSON对象
     */
    void sendToClient(QWebSocket* client, const QJsonObject& json);

signals:
    /**
     * @brief 接收到客户端消息时触发
     * @param client 发送消息的客户端
     * @param json 接收到的JSON消息
     */
    void messageReceived(QWebSocket* client, const QJsonObject& json);

    /**
     * @brief 客户端连接时触发
     * @param client 新连接的客户端
     */
    void clientConnected(QWebSocket* client);

    /**
     * @brief 客户端断开连接时触发
     * @param client 断开连接的客户端
     */
    void clientDisconnected(QWebSocket* client);

    /**
     * @brief 服务器启动成功时触发
     * @param port 监听端口
     */
    void serverStarted(quint16 port);

    /**
     * @brief 服务器停止时触发
     */
    void serverStopped();

    /**
     * @brief 发生错误时触发
     * @param errorString 错误信息
     */
    void errorOccurred(const QString& errorString);

private slots:
    // 处理新的客户端连接
    void onNewConnection();

    // 处理客户端发送的消息
    void onTextMessageReceived(const QString& message);

    // 处理客户端断开连接
    void onDisconnected();

private:
    // 私有构造函数，确保单例
    explicit WsServer(QObject *parent = nullptr);

    void lyricUpdate(const LyricData& lyricData);
    void onThemeChanged(ElaThemeType::ThemeMode themeMode);
    void positionUpdate(const LyricData& lyricData);
    void onSongInfoUpdated(const QString& auther, const QString& name, const QPixmap& cover, int pitch, bool hasLyrics);
    void updatePosition(qint64 positionMs, qint64 durationMs);
    void playStateChanged(bool isPlaying);

    // 析构函数
    ~WsServer() override;

    QWebSocketServer* m_webSocketServer;  // WebSocket服务器实例
    QList<QWebSocket*> m_clients;         // 连接的客户端列表
    QMutex m_mutex;                       // 用于线程安全的互斥锁
    int lastPosition;
};

/**
 * @brief WebSocket服务器线程管理类
 * 负责在独立线程中运行WebSocket服务器
 */
class WsServerThread : public QObject
{
    Q_OBJECT
public:
    // 单例模式，获取唯一实例
    static WsServerThread& getInstance();

    // 禁止拷贝和移动
    WsServerThread(const WsServerThread&) = delete;
    WsServerThread& operator=(const WsServerThread&) = delete;
    WsServerThread(WsServerThread&&) = delete;
    WsServerThread& operator=(WsServerThread&&) = delete;

    /**
     * @brief 启动WebSocket服务器线程
     * @param port 监听端口
     */
    void startServer(quint16 port = 12345);

    /**
     * @brief 停止WebSocket服务器线程
     */
    void stopServer();

signals:
    // 内部信号，用于跨线程启动服务器
    void startServerSignal(quint16 port);
    // 内部信号，用于跨线程停止服务器
    void stopServerSignal();

private:
    // 私有构造函数
    explicit WsServerThread(QObject *parent = nullptr);

    QThread m_thread;  // 服务器运行的线程
};

#endif // WSSERVER_H
