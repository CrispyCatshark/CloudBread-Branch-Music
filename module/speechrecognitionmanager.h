#ifndef SPEECHRECOGNITIONMANAGER_H
#define SPEECHRECOGNITIONMANAGER_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QString>

// 音频设备信息结构体
struct AudioDeviceInfo {
    int index;
    QString name;
    int channels;
};

// 语音识别配置结构体
struct SpeechRecognitionConfig {
    QString wsServer;
    QString modelPath;
    int inputDeviceIndex;
};

// 语音识别状态结构体
struct SpeechRecognitionStatus {
    bool isActive;
    QString wsServerUrl;
    QString modelPath;
    int inputDeviceIndex;
    int clientCount;
    QString lastError;
};

// 任务类型枚举（对应已有API）
enum class SpeechTaskType {
    TASK_START_RECOGNITION,    // 启动语音识别
    TASK_STOP_RECOGNITION,     // 停止语音识别
    TASK_GET_CURRENT_CONFIG,   // 获取当前配置
    TASK_UPDATE_CONFIG,        // 更新配置
    TASK_GET_CURRENT_STATUS,   // 获取当前状态
    TASK_GET_AUDIO_DEVICES,    // 获取音频设备列表
    TASK_SHUTDOWN_SYSTEM       // 关闭系统
};

// 任务数据结构体（承载任务参数与结果）
struct SpeechTask {
    SpeechTaskType taskType;                  // 任务类型
    QMap<QString, QVariant> taskParams;       // 任务参数（如更新配置时的新配置）
    QMap<QString, QVariant> taskResult;       // 任务结果（任务执行后填充）
    bool isSuccess = false;                   // 任务执行成功标识
    QString errorMsg = "";                    // 任务错误信息
};

class SpeechRecognitionManager : public QThread
{
    Q_OBJECT

private:
    // 私有化构造/析构/拷贝构造/赋值运算符（单例基础）
    explicit SpeechRecognitionManager(QObject *parent = nullptr);
    ~SpeechRecognitionManager();
    SpeechRecognitionManager(const SpeechRecognitionManager&) = delete;
    SpeechRecognitionManager& operator=(const SpeechRecognitionManager&) = delete;

    // 线程执行入口
    void run() override;
    // 处理单个任务（核心逻辑分发）
    void processTask(SpeechTask& task);

    // 辅助函数（复用原逻辑，增加任务参数适配）
    QString sendAuthenticatedPostRequest(const QString &apiPath, const QJsonObject &params);
    QString sendAuthenticatedGetRequest(const QString &apiPath, const QMap<QString, QString> &params = QMap<QString, QString>());
    QString generateSign(const QMap<QString, QString> &params);
    QString generateTimestamp();
    QJsonObject parseJsonResponse(const QString &response, bool &success);

signals:
    // 任务结果信号（按任务类型区分，向外返回数据）
    void startRecognitionResult(bool isSuccess, const QString& errorMsg);
    void stopRecognitionResult(bool isSuccess, const QString& errorMsg);
    void currentConfigResult(bool isSuccess, const SpeechRecognitionConfig& config, const QString& errorMsg);
    void updateConfigResult(bool isSuccess, const QString& errorMsg);
    void currentStatusResult(bool isSuccess, const SpeechRecognitionStatus& status, const QString& errorMsg);
    void audioDevicesResult(bool isSuccess, const QList<AudioDeviceInfo>& devices, const QString& errorMsg);
    void shutdownSystemResult(bool isSuccess, const QString& errorMsg);
    void initSysSuccess(bool isSuccess);

    // 原状态更新信号（保留兼容性）
    void statusUpdated(const SpeechRecognitionStatus& status);

public:
    // 单例获取接口（线程安全）
    static SpeechRecognitionManager* getInstance();
    // 任务添加接口（外部调用，灵活添加API请求任务）
    void addTask(const SpeechTaskType& taskType, const QMap<QString, QVariant>& taskParams = QMap<QString, QVariant>());

private:
    bool handleStartRecognition(SpeechTask &task);
    bool handleStopRecognition(SpeechTask &task);
    bool handleGetCurrentConfig(SpeechTask &task);
    bool handleUpdateConfig(SpeechTask &task);
    bool handleGetCurrentStatus(SpeechTask &task);
    bool handleGetAudioDevices(SpeechTask &task);
    bool handleShutdownSystem(SpeechTask &task);

    // 设置认证密钥（外部调用，原逻辑保留）
    void setAppSecret(const QString& appSecret) { m_appSecret = appSecret; }
    // 设置基础API地址（外部调用，原逻辑保留）
    void setBaseApiUrl(const QString& baseApiUrl) { m_baseApiUrl = baseApiUrl; }
    // 设置请求头前缀（外部调用，原逻辑保留）
    void setHeaderPrefix(const QString& headerPrefix) { m_headerPrefix = headerPrefix; }

    // 单例静态成员
    static SpeechRecognitionManager* m_instance;
    static QMutex m_instanceMutex;

    // 线程控制成员
    bool m_isRunning;              // 线程运行标识
    QMutex m_taskMutex;            // 任务队列互斥锁
    QWaitCondition m_taskCond;     // 任务等待条件变量
    QList<SpeechTask> m_taskQueue; // 任务队列

    // 原业务成员（保留原逻辑）
    QString m_appSecret;
    QString m_baseApiUrl;
    QString m_headerPrefix;
    SpeechRecognitionConfig m_currentConfig;
    SpeechRecognitionStatus m_currentStatus;
    QList<AudioDeviceInfo> m_audioDevices;
};

#endif // SPEECHRECOGNITIONMANAGER_H
