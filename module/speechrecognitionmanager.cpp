#include "SpeechRecognitionManager.h"
#include <QDateTime>
#include <QCryptographicHash>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QJsonArray>
#include "hv/requests.h"
#include "GlobalConfig.h"

// 1. 初始化静态成员（类外初始化）
SpeechRecognitionManager* SpeechRecognitionManager::m_instance = nullptr;
QMutex SpeechRecognitionManager::m_instanceMutex;

// 2. 单例获取接口（线程安全，双重检查锁定）
SpeechRecognitionManager* SpeechRecognitionManager::getInstance()
{
    if (m_instance == nullptr) {
        QMutexLocker locker(&m_instanceMutex);
        if (m_instance == nullptr) {
            m_instance = new SpeechRecognitionManager(nullptr);
            m_instance->start(); // 单例创建时自动启动线程
        }
    }
    return m_instance;
}

// 3. 私有化构造函数（初始化线程与业务参数）
SpeechRecognitionManager::SpeechRecognitionManager(QObject *parent)
    : QThread(parent),
    m_isRunning(true)
{
    // 初始化默认配置（原逻辑保留）
    m_currentConfig.wsServer = QString("ws://127.0.0.1:%1").arg(GlobalConfig::getInstance().getValue("Server/WsPort", 5310).toString());
    m_currentConfig.modelPath = "model/vosk-model-small-cn-0.22";
    m_currentConfig.inputDeviceIndex = 0;

    // 初始化默认状态（原逻辑保留）
    m_currentStatus.isActive = false;
    m_currentStatus.wsServerUrl = m_currentConfig.wsServer;
    m_currentStatus.modelPath = m_currentConfig.modelPath;
    m_currentStatus.inputDeviceIndex = -1;
    m_currentStatus.clientCount = 0;

    // 初始化请求头前缀（默认值，外部可修改）
    m_appSecret = "T3KqiMKLdynixcXk7mfaoWPlFozDNJlV0p5vvKtgaET46uEHyO6wNzAT5GhvHDyfd49tnYyGRQwbukdJxIfX7iIT6I0zGXNwYJVs9NpiouL3DuFaKadOc4xe89V96ZJX";                  // 客户端密钥（与后端一致）
    m_headerPrefix = "x-CloudBreadMusic-";  // 认证头部前缀
    m_baseApiUrl = "http://localhost:5330";  // 后端API基础地址
}

// 4. 私有化析构函数（线程停止与资源释放）
SpeechRecognitionManager::~SpeechRecognitionManager()
{
    // 停止线程
    m_isRunning = false;
    m_taskCond.wakeOne(); // 唤醒等待的线程
    wait(); // 等待线程退出

    // 释放单例
    m_instance = nullptr;
}

// 5. 线程执行入口（任务队列消费逻辑）
void SpeechRecognitionManager::run()
{
    qDebug() << "[SpeechRecognition] Thread started, waiting for tasks...";
    while (m_isRunning) {
        SpeechTask task;
        // 加锁获取任务
        {
            QMutexLocker locker(&m_taskMutex);
            // 等待任务（队列为空且线程运行时阻塞）
            while (m_taskQueue.isEmpty() && m_isRunning) {
                m_taskCond.wait(&m_taskMutex);
            }
            // 线程停止时退出循环
            if (!m_isRunning) break;
            // 从队列头部取出任务（FIFO顺序）
            task = m_taskQueue.takeFirst();
        }

        // 处理任务（核心逻辑分发）
        processTask(task);
    }
    qDebug() << "[SpeechRecognition] Thread stopped";
}

// 6. 任务添加接口（外部调用，线程安全）
void SpeechRecognitionManager::addTask(const SpeechTaskType &taskType, const QMap<QString, QVariant> &taskParams)
{
    QMutexLocker locker(&m_taskMutex);
    // 构建任务对象
    SpeechTask task;
    task.taskType = taskType;
    task.taskParams = taskParams;
    // 添加到任务队列
    m_taskQueue.append(task);
    qDebug() << "[SpeechRecognition] Task added, type:" << static_cast<int>(taskType) << ", queue size:" << m_taskQueue.size();
    // 唤醒线程处理任务
    m_taskCond.wakeOne();
}

// 7. 任务处理核心逻辑（按任务类型分发，调用对应API）
void SpeechRecognitionManager::processTask(SpeechTask &task)
{
    qDebug() << "[SpeechRecognition] Processing task, type:" << static_cast<int>(task.taskType);

    // 通用前置校验（认证密钥、API地址，除了关闭系统任务已在自身函数校验）
    if (m_appSecret.isEmpty() && task.taskType != SpeechTaskType::TASK_SHUTDOWN_SYSTEM) {
        task.isSuccess = false;
        task.errorMsg = "认证密钥未设置";
        qWarning() << "[SpeechRecognition] Task failed:" << task.errorMsg;
        goto emitResult;
    }
    if (m_baseApiUrl.isEmpty() && task.taskType != SpeechTaskType::TASK_SHUTDOWN_SYSTEM) {
        task.isSuccess = false;
        task.errorMsg = "基础API地址未配置";
        qWarning() << "[SpeechRecognition] Task failed:" << task.errorMsg;
        goto emitResult;
    }

    // 按任务类型调用对应独立处理函数
    switch (task.taskType) {
    case SpeechTaskType::TASK_START_RECOGNITION:
        task.isSuccess = handleStartRecognition(task);
        break;
    case SpeechTaskType::TASK_STOP_RECOGNITION:
        task.isSuccess = handleStopRecognition(task);
        break;
    case SpeechTaskType::TASK_GET_CURRENT_CONFIG:
        task.isSuccess = handleGetCurrentConfig(task);
        break;
    case SpeechTaskType::TASK_UPDATE_CONFIG:
        task.isSuccess = handleUpdateConfig(task);
        break;
    case SpeechTaskType::TASK_GET_CURRENT_STATUS:
        task.isSuccess = handleGetCurrentStatus(task);
        break;
    case SpeechTaskType::TASK_GET_AUDIO_DEVICES:
        task.isSuccess = handleGetAudioDevices(task);
        break;
    case SpeechTaskType::TASK_SHUTDOWN_SYSTEM:
        task.isSuccess = handleShutdownSystem(task);
        break;
    default:
        task.isSuccess = false;
        task.errorMsg = "未知任务类型";
        qWarning() << "[SpeechRecognition] Unknown task type:" << static_cast<int>(task.taskType);
        break;
    }

// 结果发射逻辑（完全保留原逻辑，确保外部信号正常）
emitResult:
    switch (task.taskType) {
    case SpeechTaskType::TASK_START_RECOGNITION:
        emit startRecognitionResult(task.isSuccess, task.errorMsg);
        break;
    case SpeechTaskType::TASK_STOP_RECOGNITION:
        emit stopRecognitionResult(task.isSuccess, task.errorMsg);
        break;
    case SpeechTaskType::TASK_GET_CURRENT_CONFIG:
        emit currentConfigResult(task.isSuccess,
                                 task.taskResult["config"].value<SpeechRecognitionConfig>(),
                                 task.errorMsg);
        break;
    case SpeechTaskType::TASK_UPDATE_CONFIG:
        emit updateConfigResult(task.isSuccess, task.errorMsg);
        break;
    case SpeechTaskType::TASK_GET_CURRENT_STATUS:
        emit currentStatusResult(task.isSuccess,
                                 task.taskResult["status"].value<SpeechRecognitionStatus>(),
                                 task.errorMsg);
        break;
    case SpeechTaskType::TASK_GET_AUDIO_DEVICES:
        emit audioDevicesResult(task.isSuccess,
                                task.taskResult["devices"].value<QList<AudioDeviceInfo>>(),
                                task.errorMsg);
        break;
    case SpeechTaskType::TASK_SHUTDOWN_SYSTEM:
        emit shutdownSystemResult(task.isSuccess, task.errorMsg);
        break;
    default:
        break;
    }

    qDebug() << "[SpeechRecognition] Task processed, type:" << static_cast<int>(task.taskType)
             << ", success:" << task.isSuccess << ", error:" << task.errorMsg;
}

// ------------------------------
// 辅助函数（复用原逻辑，无修改）
// ------------------------------
QString SpeechRecognitionManager::sendAuthenticatedPostRequest(const QString &apiPath, const QJsonObject &params)
{
    QString fullUrl = m_baseApiUrl + apiPath;
    qDebug() << "[SpeechRecognition] Sending POST request to:" << fullUrl;

    QString timestamp = generateTimestamp();
    QMap<QString, QString> emptyParams;
    QString sign = generateSign(emptyParams);

    http_headers headers;
    headers["Content-Type"] = "application/json;charset=utf-8";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36";
    headers["Accept"] = "application/json, text/plain, */*";
    headers[m_headerPrefix.toStdString() + "Sign"] = sign.toStdString();
    headers[m_headerPrefix.toStdString() + "Timestamp"] = timestamp.toStdString();

    QJsonDocument doc(params);
    std::string body = doc.toJson(QJsonDocument::Compact).toStdString();

    auto resp = requests::post(fullUrl.toStdString().c_str(), body, headers);
    if (resp == nullptr) {
        qWarning() << "[SpeechRecognition] POST request failed: null response";
        return "";
    }

    QString response = QString::fromStdString(resp->body);
    qDebug() << "[SpeechRecognition] POST response:" << response;
    return response;
}

QString SpeechRecognitionManager::sendAuthenticatedGetRequest(const QString &apiPath, const QMap<QString, QString> &params)
{
    QString timestamp = generateTimestamp();
    QMap<QString, QString> allParams = params;
    allParams["timestamp"] = timestamp;
    QString sign = generateSign(allParams);

    QStringList urlParams;
    for (auto it = allParams.begin(); it != allParams.end(); ++it) {
        QString encodedKey = QUrl::toPercentEncoding(it.key());
        QString encodedVal = QUrl::toPercentEncoding(it.value());
        urlParams.append(QString("%1=%2").arg(encodedKey, encodedVal));
    }
    urlParams.append(QString("sign=%1").arg(QUrl::toPercentEncoding(sign)));

    QString fullUrl = m_baseApiUrl + apiPath + "?" + urlParams.join("&");
    qDebug() << "[SpeechRecognition] Sending GET request to:" << fullUrl;

    http_headers headers;
    headers["Content-Type"] = "application/json;charset=utf-8";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36";
    headers["Accept"] = "application/json, text/plain, */*";
    headers[m_headerPrefix.toStdString() + "Sign"] = sign.toStdString();
    headers[m_headerPrefix.toStdString() + "Timestamp"] = timestamp.toStdString();

    auto resp = requests::get(fullUrl.toStdString().c_str(), headers);
    if (resp == nullptr) {
        qWarning() << "[SpeechRecognition] GET request failed: null response";
        return "";
    }

    QString response = QString::fromStdString(resp->body);
    qDebug() << "[SpeechRecognition] GET response:" << response;
    return response;
}

QString SpeechRecognitionManager::generateSign(const QMap<QString, QString> &params)
{
    if (m_appSecret.isEmpty()) {
        qWarning() << "[SpeechRecognition] Generate sign failed: AppSecret is empty";
        return "";
    }

    QStringList sortedKeys = params.keys();
    sortedKeys.sort(Qt::CaseInsensitive);

    QStringList paramList;
    for (const QString &key : sortedKeys) {
        if (key.toLower() == "sign") continue;
        QString encodedKey = QUrl::toPercentEncoding(key, ".-_~");
        QString encodedVal = QUrl::toPercentEncoding(params[key], ".-_~");
        paramList.append(QString("%1=%2").arg(encodedKey, encodedVal));
    }
    QString paramStr = paramList.join("&");

    QString signStr = paramStr + m_appSecret;
    QByteArray md5Bytes = QCryptographicHash::hash(signStr.toUtf8(), QCryptographicHash::Md5);
    QString sign = md5Bytes.toHex().toLower();

    qDebug() << "[SpeechRecognition] Generated sign: " << sign << "(source: " << signStr << ")";
    return sign;
}

QString SpeechRecognitionManager::generateTimestamp()
{
    return QString::number(QDateTime::currentSecsSinceEpoch());
}

QJsonObject SpeechRecognitionManager::parseJsonResponse(const QString &response, bool &success)
{
    success = false;
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "[SpeechRecognition] Parse JSON failed:" << jsonError.errorString() << "(response: " << response << ")";
        return QJsonObject();
    }

    if (!doc.isObject()) {
        qWarning() << "[SpeechRecognition] JSON is not an object (response: " << response << ")";
        return QJsonObject();
    }

    success = true;
    return doc.object();
}

// 1. 处理“启动识别”任务
bool SpeechRecognitionManager::handleStartRecognition(SpeechTask &task)
{
    bool success = false;
    QString errorMsg = "";
    QString response = sendAuthenticatedGetRequest("/speech/control/start");
    QJsonObject json = parseJsonResponse(response, success);

    if (success && json["code"].toInt() == 200) {
        m_currentStatus.isActive = true;
        m_currentStatus.lastError = "";
        emit statusUpdated(m_currentStatus);
    } else {
        errorMsg = json["message"].toString("启动识别失败");
        m_currentStatus.lastError = errorMsg;
        emit statusUpdated(m_currentStatus);
    }

    // 更新任务结果
    task.isSuccess = success;
    task.errorMsg = errorMsg;
    return success;
}

// 2. 处理“停止识别”任务
bool SpeechRecognitionManager::handleStopRecognition(SpeechTask &task)
{
    bool success = false;
    QString errorMsg = "";
    QString response = sendAuthenticatedGetRequest("/speech/control/stop");
    QJsonObject json = parseJsonResponse(response, success);

    if (success && json["code"].toInt() == 200) {
        m_currentStatus.isActive = false;
        m_currentStatus.lastError = "";
        emit statusUpdated(m_currentStatus);
    } else {
        errorMsg = json["message"].toString("停止识别失败");
        m_currentStatus.lastError = errorMsg;
        emit statusUpdated(m_currentStatus);
    }

    task.isSuccess = success;
    task.errorMsg = errorMsg;
    return success;
}

// 3. 处理“获取当前配置”任务
bool SpeechRecognitionManager::handleGetCurrentConfig(SpeechTask &task)
{
    bool success = false;
    QString errorMsg = "";
    QString response = sendAuthenticatedGetRequest("/speech/settings");
    QJsonObject json = parseJsonResponse(response, success);
    SpeechRecognitionConfig config = m_currentConfig; // 默认用本地缓存

    if (success && json["code"].toInt() == 200) {
        QJsonObject configJson = json["settings"].toObject();
        config.wsServer = configJson["ws_server_url"].toString();
        config.modelPath = configJson["model_path"].toString("model/vosk-model-small-cn-0.22");
        config.inputDeviceIndex = configJson["input_device_index"].toInt(-1);
        m_currentConfig = config; // 更新本地缓存
    } else {
        errorMsg = json["message"].toString("获取配置失败，使用本地缓存");
    }

    task.isSuccess = success;
    task.errorMsg = errorMsg;
    task.taskResult["config"] = QVariant::fromValue(config); // 存入结果
    return success;
}

// 4. 处理“更新配置”任务
bool SpeechRecognitionManager::handleUpdateConfig(SpeechTask &task)
{
    bool success = false;
    QString errorMsg = "";

    // 先检查参数是否存在
    if (!task.taskParams.contains("newConfig")) {
        errorMsg = "更新配置失败：缺少新配置参数";
        task.isSuccess = success;
        task.errorMsg = errorMsg;
        return success;
    }

    SpeechRecognitionConfig newConfig = task.taskParams["newConfig"].value<SpeechRecognitionConfig>();
    QJsonObject params;
    params["ws_server_url"] = newConfig.wsServer;
    params["model_path"] = newConfig.modelPath;
    params["input_device_index"] = newConfig.inputDeviceIndex;
    QString response = sendAuthenticatedPostRequest("/speech/settings", params);
    QJsonObject json = parseJsonResponse(response, success);

    if (success && json["code"].toInt() == 200) {
        m_currentConfig = newConfig; // 更新本地缓存
        m_currentStatus.lastError = "";
        emit statusUpdated(m_currentStatus);
    } else {
        errorMsg = json["message"].toString("更新配置失败");
        m_currentStatus.lastError = errorMsg;
        emit statusUpdated(m_currentStatus);
    }

    task.isSuccess = success;
    task.errorMsg = errorMsg;
    return success;
}

// 5. 处理“获取当前状态”任务
bool SpeechRecognitionManager::handleGetCurrentStatus(SpeechTask &task)
{
    bool success = false;
    QString errorMsg = "";
    QString response = sendAuthenticatedGetRequest("/speech/status");
    QJsonObject json = parseJsonResponse(response, success);
    SpeechRecognitionStatus status = m_currentStatus; // 默认用本地缓存

    if (success && json["code"].toInt() == 200) {
        status.isActive = json["active"].toBool(false);
        status.wsServerUrl = json["ws_server_url"].toString();
        status.modelPath = json["model_path"].toString("model/vosk-model-small-cn-0.22");
        status.inputDeviceIndex = json["input_device"].toInt(-1);
        status.clientCount = json["client_count"].toInt(0);
        status.lastError = "";
        m_currentStatus = status; // 更新本地缓存
    } else {
        errorMsg = json["message"].toString("获取状态失败，使用本地缓存");
    }

    task.isSuccess = success;
    task.errorMsg = errorMsg;
    task.taskResult["status"] = QVariant::fromValue(status); // 存入结果
    return success;
}

// 6. 处理“获取音频设备”任务
bool SpeechRecognitionManager::handleGetAudioDevices(SpeechTask &task)
{
    bool success = false;
    QString errorMsg = "";
    m_audioDevices.clear();
    QString response = sendAuthenticatedGetRequest("/speech/devices");
    QJsonObject json = parseJsonResponse(response, success);
    QList<AudioDeviceInfo> devices;

    if (success && json["code"].toInt() == 200) {
        QJsonArray devicesArray = json["devices"].toArray();
        for (const QJsonValue &val : devicesArray) {
            QJsonObject devObj = val.toObject();
            AudioDeviceInfo device;
            device.index = devObj["index"].toInt();
            device.name = devObj["name"].toString();
            device.channels = devObj["channels"].toInt();
            devices.append(device);
        }
        m_audioDevices = devices; // 更新本地缓存
        qDebug() << "[SpeechRecognition] Found" << devices.size() << "audio devices";
    } else {
        errorMsg = json["message"].toString("获取音频设备失败");
        m_currentStatus.lastError = errorMsg;
        emit statusUpdated(m_currentStatus);
    }

    task.isSuccess = success;
    task.errorMsg = errorMsg;
    task.taskResult["devices"] = QVariant::fromValue(devices); // 存入结果
    return success;
}

// 7. 处理“关闭系统”任务
bool SpeechRecognitionManager::handleShutdownSystem(SpeechTask &task)
{
    bool success = false;
    QString errorMsg = "";

    // 先检查必要参数
    if (m_appSecret.isEmpty()) {
        errorMsg = "认证密钥未设置，无法发送关闭指令";
        m_currentStatus.lastError = errorMsg;
        emit statusUpdated(m_currentStatus);
        task.isSuccess = success;
        task.errorMsg = errorMsg;
        return success;
    }
    if (m_baseApiUrl.isEmpty()) {
        errorMsg = "基础API地址未配置，无法发送关闭指令";
        m_currentStatus.lastError = errorMsg;
        emit statusUpdated(m_currentStatus);
        task.isSuccess = success;
        task.errorMsg = errorMsg;
        return success;
    }

    // 发送关闭请求
    QString apiPath = "/system/shutdown";
    QString fullUrl = m_baseApiUrl + apiPath;
    QString timestamp = generateTimestamp();
    QMap<QString, QString> emptyParams;
    QString sign = generateSign(emptyParams);

    http_headers headers;
    headers["Content-Type"] = "application/json;charset=utf-8";
    headers["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36";
    headers["Accept"] = "application/json, text/plain, */*";
    headers[m_headerPrefix.toStdString() + "Sign"] = sign.toStdString();
    headers[m_headerPrefix.toStdString() + "Timestamp"] = timestamp.toStdString();

    std::string emptyBody = "{}";
    auto resp = requests::post(fullUrl.toStdString().c_str(), emptyBody, headers);
    if (resp == nullptr) {
        errorMsg = "关闭系统请求失败：无服务器响应";
        qWarning() << "[SpeechRecognition] Shutdown request failed:" << errorMsg;
    } else {
        success = true;
        errorMsg = "关闭系统请求已发送";
        qDebug() << "[SpeechRecognition] Shutdown request sent, response:" << QString::fromStdString(resp->body);
    }

    task.isSuccess = success;
    task.errorMsg = errorMsg;
    return success;
}
