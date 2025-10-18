#include "livesettingspage.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include <QVBoxLayout>
#include <qapplication.h>
#include <QClipboard>
#include <QDesktopServices>
#include <qdir.h>
#include <QProcess>
#include <QFileDialog>
#include "GlobalConfig.h"
#include "ElaMessageBar.h"
#include "speechrecognitionmanager.h"

liveSettingsPage::liveSettingsPage(QWidget *parent)
    : ElaScrollPage(parent)
{
    // 1. 初始化SpeechRecognitionManager并设置基础参数
    SpeechRecognitionManager* speechMgr = SpeechRecognitionManager::getInstance();

    // 2. 连接SpeechRecognitionManager任务结果信号（核心修改：替换原直接调用为信号订阅）
    connect(speechMgr, &SpeechRecognitionManager::startRecognitionResult, this, &liveSettingsPage::onStartRecognitionResult);
    connect(speechMgr, &SpeechRecognitionManager::stopRecognitionResult, this, &liveSettingsPage::onStopRecognitionResult);
    connect(speechMgr, &SpeechRecognitionManager::currentStatusResult, this, &liveSettingsPage::onCurrentStatusResult);
    connect(speechMgr, &SpeechRecognitionManager::audioDevicesResult, this, &liveSettingsPage::onAudioDevicesResult);
    connect(speechMgr, &SpeechRecognitionManager::currentConfigResult, this, &liveSettingsPage::onCurrentConfigResult);
    connect(speechMgr, &SpeechRecognitionManager::updateConfigResult, this, &liveSettingsPage::onUpdateConfigResult);

    initPage();
}

liveSettingsPage::~liveSettingsPage()
{
}

void liveSettingsPage::initPage()
{
    setWindowTitle("直播设置");
    // 创建中心部件和布局
    QWidget* centralWidget = new QWidget(this);

    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);

    ElaText* liveLyricText = new ElaText("直播间逐字歌词", this);
    liveLyricText->setWordWrap(false);
    liveLyricText->setTextPixelSize(18);

    QString AdminURL = "http://localhost:" + GlobalConfig::getInstance().getValue("Server/WebPort", 5320).toString() + "/lyric/admin.html";
    m_lyricAdminAddress = new ElaLineEdit(this);
    m_lyricAdminAddressCopyBtn = new ElaPushButton(this);
    m_lyricAdminOpenBtn = new ElaPushButton(this);
    ElaScrollPageArea* lyricAdminArea = new ElaScrollPageArea(this);
    QHBoxLayout* lyricAdminLayout = new QHBoxLayout(lyricAdminArea);
    ElaText* lyricAdminText = new ElaText("直播间歌词样式配置", this);
    m_lyricAdminAddress->setText(AdminURL);
    m_lyricAdminAddress->setFixedHeight(35);
    m_lyricAdminAddressCopyBtn->setText("复制到剪切板");
    m_lyricAdminAddressCopyBtn->setFixedHeight(35);
    m_lyricAdminOpenBtn->setText("浏览器打开");
    m_lyricAdminOpenBtn->setFixedHeight(35);
    lyricAdminText->setWordWrap(false);
    lyricAdminText->setTextPixelSize(15);
    lyricAdminLayout->addWidget(lyricAdminText);
    lyricAdminLayout->addStretch();
    lyricAdminLayout->addWidget(m_lyricAdminAddress);
    lyricAdminLayout->addWidget(m_lyricAdminAddressCopyBtn);
    lyricAdminLayout->addWidget(m_lyricAdminOpenBtn);
    connect(m_lyricAdminAddress, &ElaLineEdit::textChanged, this, [this, AdminURL]() {
        m_lyricAdminAddress->setText(AdminURL);
    });
    connect(m_lyricAdminAddressCopyBtn, &ElaPushButton::clicked, this, [this, AdminURL]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(AdminURL);
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "复制到剪切板成功", 2000);
    });
    connect(m_lyricAdminOpenBtn, &ElaPushButton::clicked, this, [this, AdminURL]() {
        QDesktopServices::openUrl(QUrl(AdminURL, QUrl::TolerantMode));
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "浏览器打开成功", 2000);
    });

    QString LyricURL = "http://localhost:" + GlobalConfig::getInstance().getValue("Server/WebPort", 5320).toString() + "/lyric/index.html";
    m_lyricSourceAddress = new ElaLineEdit(this);
    m_lyricAddressCopyBtn = new ElaPushButton(this);
    m_lyricAddressOpenBtn = new ElaPushButton(this);
    ElaScrollPageArea* lyricArea = new ElaScrollPageArea(this);
    QHBoxLayout* lyricLayout = new QHBoxLayout(lyricArea);
    ElaText* lyricText = new ElaText("直播间歌词", this);
    m_lyricSourceAddress->setText(LyricURL);
    m_lyricSourceAddress->setFixedHeight(35);
    m_lyricAddressCopyBtn->setText("复制到剪切板");
    m_lyricAddressCopyBtn->setFixedHeight(35);
    m_lyricAddressOpenBtn->setText("浏览器打开");
    m_lyricAddressOpenBtn->setFixedHeight(35);
    lyricText->setWordWrap(false);
    lyricText->setTextPixelSize(15);
    lyricLayout->addWidget(lyricText);
    lyricLayout->addStretch();
    lyricLayout->addWidget(m_lyricSourceAddress);
    lyricLayout->addWidget(m_lyricAddressCopyBtn);
    lyricLayout->addWidget(m_lyricAddressOpenBtn);
    connect(m_lyricSourceAddress, &ElaLineEdit::textChanged, this, [this, LyricURL]() {
        m_lyricSourceAddress->setText(LyricURL);
    });
    connect(m_lyricAddressCopyBtn, &ElaPushButton::clicked, this, [this, LyricURL]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(LyricURL);
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "复制到剪切板成功", 2000);
    });
    connect(m_lyricAddressOpenBtn, &ElaPushButton::clicked, this, [this, LyricURL]() {
        QDesktopServices::openUrl(QUrl(LyricURL, QUrl::TolerantMode));
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "浏览器打开成功", 2000);
    });

    ElaText* liveSubtitlesText = new ElaText("直播间实时字幕", this);
    liveSubtitlesText->setWordWrap(false);
    liveSubtitlesText->setTextPixelSize(18);

    m_liveSubtitleStatusBtn = new ElaPushButton(this);
    m_liveSubtitleRefreshBtn = new ElaPushButton(this);
    ElaScrollPageArea* lyricSubtitlesArea = new ElaScrollPageArea(this);
    QHBoxLayout* lyricSubtitlesLayout = new QHBoxLayout(lyricSubtitlesArea);
    ElaText* lyricSubtitlesText = new ElaText("直播间实时字幕服务状态", this);
    m_liveSubtitleStatusBtn->setText("启动");
    m_liveSubtitleStatusBtn->setFixedHeight(35);
    m_liveSubtitleRefreshBtn->setText("刷新状态");
    m_liveSubtitleRefreshBtn->setFixedHeight(35);
    lyricSubtitlesText->setWordWrap(false);
    lyricSubtitlesText->setTextPixelSize(15);
    lyricSubtitlesLayout->addWidget(lyricSubtitlesText);
    lyricSubtitlesLayout->addStretch();
    lyricSubtitlesLayout->addWidget(m_liveSubtitleStatusBtn);
    lyricSubtitlesLayout->addWidget(m_liveSubtitleRefreshBtn);

    // 3. 修改：替换直接调用start/stopRecognition为添加任务
    connect(m_liveSubtitleStatusBtn, &ElaPushButton::clicked, this, [this]() {
        SpeechRecognitionManager* speechMgr = SpeechRecognitionManager::getInstance();
        if (m_speechStatus.isActive) {
            // 停止识别：添加TASK_STOP_RECOGNITION任务
            speechMgr->addTask(SpeechTaskType::TASK_STOP_RECOGNITION);
        } else {
            // 启动识别：添加TASK_START_RECOGNITION任务
            speechMgr->addTask(SpeechTaskType::TASK_START_RECOGNITION);
        }
        // 触发状态刷新（实际状态更新在onCurrentStatusResult中处理）
        speechServerStatusUpdata();
    });

    // 4. 修改：替换直接调用getCurrentStatus为添加任务
    connect(m_liveSubtitleRefreshBtn, &ElaPushButton::clicked, this, &liveSettingsPage::speechServerStatusUpdata);

    m_liveSubtitleDeviceComBox = new ElaComboBox(this);
    m_liveSubtitleDeviceRefreshBtn = new ElaPushButton(this);
    ElaScrollPageArea* lyricSubtitlesDeviceArea = new ElaScrollPageArea(this);
    QHBoxLayout* lyricSubtitlesDeviceLayout = new QHBoxLayout(lyricSubtitlesDeviceArea);
    ElaText* lyricSubtitlesDeviceText = new ElaText("输入设备", this);
    m_liveSubtitleDeviceComBox->setFixedHeight(35);
    m_liveSubtitleDeviceComBox->setMinimumWidth(400);
    m_liveSubtitleDeviceComBox->adjustSize();
    m_liveSubtitleDeviceRefreshBtn->setText("刷新设备列表");
    m_liveSubtitleDeviceRefreshBtn->setFixedHeight(35);
    lyricSubtitlesDeviceText->setWordWrap(false);
    lyricSubtitlesDeviceText->setTextPixelSize(15);
    lyricSubtitlesDeviceLayout->addWidget(lyricSubtitlesDeviceText);
    lyricSubtitlesDeviceLayout->addStretch();
    lyricSubtitlesDeviceLayout->addWidget(m_liveSubtitleDeviceComBox);
    lyricSubtitlesDeviceLayout->addWidget(m_liveSubtitleDeviceRefreshBtn);
    connect(m_liveSubtitleDeviceComBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (speechInitFlag) {
            m_speechConfig.inputDeviceIndex = index;
            GlobalConfig::getInstance().setValue("SpeechToTextSettings/inputDeviceIndex", index);
            updataConfig();
            ElaMessageBar::success(ElaMessageBarType::Top, "成功", "修改成功", 2000);
        }
    });

    // 5. 修改：替换直接调用getAudioDevices为添加任务
    connect(m_liveSubtitleDeviceRefreshBtn, &ElaPushButton::clicked, this, &liveSettingsPage::speechServerDeviceUpdata);

    m_liveSubtitleModelComBox = new ElaComboBox(this);
    m_liveSubtitleModelRefreshBtn = new ElaPushButton(this);
    m_liveSubtitleModelDirRefreshBtn = new ElaPushButton(this);
    ElaScrollPageArea* lyricSubtitlesModelArea = new ElaScrollPageArea(this);
    QHBoxLayout* lyricSubtitlesModelLayout = new QHBoxLayout(lyricSubtitlesModelArea);
    ElaText* lyricSubtitlesModelText = new ElaText("识别模型", this);
    m_liveSubtitleModelComBox->setFixedHeight(35);
    m_liveSubtitleModelComBox->setMinimumWidth(300);
    m_liveSubtitleModelComBox->adjustSize();
    m_liveSubtitleModelRefreshBtn->setText("刷新模型列表");
    m_liveSubtitleModelRefreshBtn->setFixedHeight(35);
    m_liveSubtitleModelDirRefreshBtn->setText("打开模型文件夹");
    m_liveSubtitleModelDirRefreshBtn->setFixedHeight(35);
    lyricSubtitlesModelText->setWordWrap(false);
    lyricSubtitlesModelText->setTextPixelSize(15);
    lyricSubtitlesModelLayout->addWidget(lyricSubtitlesModelText);
    lyricSubtitlesModelLayout->addStretch();
    lyricSubtitlesModelLayout->addWidget(m_liveSubtitleModelComBox);
    lyricSubtitlesModelLayout->addWidget(m_liveSubtitleModelRefreshBtn);
    lyricSubtitlesModelLayout->addWidget(m_liveSubtitleModelDirRefreshBtn);
    connect(m_liveSubtitleModelComBox, &QComboBox::currentTextChanged, this, [this](const QString& modelPath) {
        if (speechInitFlag) {
            m_speechConfig.modelPath = "model/" + modelPath;
            GlobalConfig::getInstance().setValue("SpeechToTextSettings/modelPath", m_speechConfig.modelPath);
            updataConfig();
            ElaMessageBar::success(ElaMessageBarType::Top, "成功", "修改成功", 2000);
        }
    });
    connect(m_liveSubtitleModelRefreshBtn, &ElaPushButton::clicked, this, &liveSettingsPage::refreshModel);
    connect(m_liveSubtitleModelDirRefreshBtn, &ElaPushButton::clicked, this, [this]() {
        const QString explorer = "explorer";
        QString path = QCoreApplication::applicationDirPath() + "/model";
        QStringList param;
        if (!QFileInfo(path).isDir()) {
            param << QLatin1String("/select,");
        }
        param << QDir::toNativeSeparators(path);
        QProcess::startDetached(explorer, param);
    });

    QString SubtitleTestURL = "http://localhost:" + GlobalConfig::getInstance().getValue("Server/WebPort", 5320).toString() + "/subtitles/test.html";
    m_subtitleTestAddress = new ElaLineEdit(this);
    m_subtitleTestCopyBtn = new ElaPushButton(this);
    m_subtitleTestOpenBtn = new ElaPushButton(this);
    ElaScrollPageArea* SubtitleTestArea = new ElaScrollPageArea(this);
    QHBoxLayout* SubtitleTestLayout = new QHBoxLayout(SubtitleTestArea);
    ElaText* SubtitleTestText = new ElaText("直播间实时字幕测试", this);
    m_subtitleTestAddress->setText(SubtitleTestURL);
    m_subtitleTestAddress->setFixedHeight(35);
    m_subtitleTestCopyBtn->setText("复制到剪切板");
    m_subtitleTestCopyBtn->setFixedHeight(35);
    m_subtitleTestOpenBtn->setText("浏览器打开");
    m_subtitleTestOpenBtn->setFixedHeight(35);
    SubtitleTestText->setWordWrap(false);
    SubtitleTestText->setTextPixelSize(15);
    SubtitleTestLayout->addWidget(SubtitleTestText);
    SubtitleTestLayout->addStretch();
    SubtitleTestLayout->addWidget(m_subtitleTestAddress);
    SubtitleTestLayout->addWidget(m_subtitleTestCopyBtn);
    SubtitleTestLayout->addWidget(m_subtitleTestOpenBtn);
    connect(m_subtitleTestAddress, &ElaLineEdit::textChanged, this, [this, SubtitleTestURL]() {
        m_subtitleTestAddress->setText(SubtitleTestURL);
    });
    connect(m_subtitleTestCopyBtn, &ElaPushButton::clicked, this, [this, SubtitleTestURL]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(SubtitleTestURL);
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "复制到剪切板成功", 2000);
    });
    connect(m_subtitleTestOpenBtn, &ElaPushButton::clicked, this, [this, SubtitleTestURL]() {
        QDesktopServices::openUrl(QUrl(SubtitleTestURL, QUrl::TolerantMode));
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "浏览器打开成功", 2000);
    });

    QString SubtitleURL = "http://localhost:" + GlobalConfig::getInstance().getValue("Server/WebPort", 5320).toString() + "/subtitles/index.html";
    m_subtitleAddress = new ElaLineEdit(this);
    m_subtitleAddressCopyBtn = new ElaPushButton(this);
    m_subtitleOpenBtn = new ElaPushButton(this);
    ElaScrollPageArea* SubtitleArea = new ElaScrollPageArea(this);
    QHBoxLayout* SubtitleLayout = new QHBoxLayout(SubtitleArea);
    ElaText* SubtitleText = new ElaText("直播间实时字幕", this);
    m_subtitleAddress->setText(SubtitleURL);
    m_subtitleAddress->setFixedHeight(35);
    m_subtitleAddressCopyBtn->setText("复制到剪切板");
    m_subtitleAddressCopyBtn->setFixedHeight(35);
    m_subtitleOpenBtn->setText("浏览器打开");
    m_subtitleOpenBtn->setFixedHeight(35);
    SubtitleText->setWordWrap(false);
    SubtitleText->setTextPixelSize(15);
    SubtitleLayout->addWidget(SubtitleText);
    SubtitleLayout->addStretch();
    SubtitleLayout->addWidget(m_subtitleAddress);
    SubtitleLayout->addWidget(m_subtitleAddressCopyBtn);
    SubtitleLayout->addWidget(m_subtitleOpenBtn);
    connect(m_subtitleAddress, &ElaLineEdit::textChanged, this, [this, SubtitleURL]() {
        m_subtitleAddress->setText(SubtitleURL);
    });
    connect(m_subtitleAddressCopyBtn, &ElaPushButton::clicked, this, [this, SubtitleURL]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(SubtitleURL);
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "复制到剪切板成功", 2000);
    });
    connect(m_subtitleOpenBtn, &ElaPushButton::clicked, this, [this, SubtitleURL]() {
        QDesktopServices::openUrl(QUrl(SubtitleURL, QUrl::TolerantMode));
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "浏览器打开成功", 2000);
    });

    centralWidget->setWindowTitle("直播设置");
    centerLayout->addWidget(liveLyricText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(lyricAdminArea);
    centerLayout->addWidget(lyricArea);
    centerLayout->addSpacing(20);
    centerLayout->addWidget(liveSubtitlesText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(SubtitleTestArea);
    centerLayout->addWidget(SubtitleArea);
    centerLayout->addWidget(lyricSubtitlesArea);
    centerLayout->addWidget(lyricSubtitlesDeviceArea);
    centerLayout->addWidget(lyricSubtitlesModelArea);
    centerLayout->addStretch();
    centerLayout->setContentsMargins(0, 0, 0, 0);

    addCentralWidget(centralWidget, true, true, 0);
}

void liveSettingsPage::backThreadStart()
{
    m_speechStatus.modelPath = "model/vosk-model-small-cn-0.22";
    // 7. 修改：替换直接调用为添加任务（初始化时获取设备、状态、配置）
    SpeechRecognitionManager* speechMgr = SpeechRecognitionManager::getInstance();
    speechMgr->addTask(SpeechTaskType::TASK_GET_AUDIO_DEVICES);    // 获取设备列表
    refreshModel();

    // 初始化配置（从全局配置读取）
    m_speechConfig.modelPath = GlobalConfig::getInstance().getValue("SpeechToTextSettings/modelPath", "model/vosk-model-small-cn-0.22").toString();
    m_speechConfig.inputDeviceIndex = GlobalConfig::getInstance().getValue("SpeechToTextSettings/inputDeviceIndex", 0).toInt();
    m_speechConfig.wsServer = QString("ws://127.0.0.1:%1").arg(GlobalConfig::getInstance().getValue("Server/WsPort", 5310).toString());
    updataConfig(); // 同步配置到后端
    speechMgr->addTask(SpeechTaskType::TASK_GET_CURRENT_STATUS);   // 获取当前状态

    if (GlobalConfig::getInstance().getValue("SpeechToTextSettings/active", false).toBool()) {
        speechMgr->addTask(SpeechTaskType::TASK_START_RECOGNITION);
    }
}

void liveSettingsPage::speechServerStatusUpdata()
{
    // 8. 修改：替换直接调用getCurrentStatus为添加TASK_GET_CURRENT_STATUS任务
    SpeechRecognitionManager::getInstance()->addTask(SpeechTaskType::TASK_GET_CURRENT_STATUS);
}

void liveSettingsPage::speechServerDeviceUpdata()
{
    // 9. 修改：替换直接调用getAudioDevices为添加TASK_GET_AUDIO_DEVICES任务
    SpeechRecognitionManager::getInstance()->addTask(SpeechTaskType::TASK_GET_AUDIO_DEVICES);
}

void liveSettingsPage::refreshModel()
{
    // 原有模型列表刷新逻辑（无SpeechRecognitionManager调用，保持不变）
    m_liveSubtitleModelComBox->clear();
    QString modelDirPath = QCoreApplication::applicationDirPath() + "/model";
    QDir modelDir(modelDirPath);
    QDir::Filters dirFilters = QDir::Dirs | QDir::NoDotAndDotDot;
    QFileInfoList dirList = modelDir.entryInfoList(dirFilters);

    for (const QFileInfo& dirInfo : std::as_const(dirList))
    {
        QString dirName = dirInfo.fileName();
        QString dirFullPath = dirInfo.absoluteFilePath();
        m_liveSubtitleModelComBox->addItem(dirName, dirFullPath);
    }

    // 选中当前模型（从m_speechStatus获取）
    if (!m_speechStatus.modelPath.isEmpty() && m_liveSubtitleModelComBox->count())
    {
        selectModelByFolderName(m_speechStatus.modelPath.mid(6)); // mid(6)：截取"model/"后的路径
    }
    SpeechRecognitionManager::getInstance()->addTask(SpeechTaskType::TASK_GET_CURRENT_STATUS);
}

void liveSettingsPage::selectModelByFolderName(const QString& folderName)
{
    // 原有模型选中逻辑（保持不变）
    for (int i = 0; i < m_liveSubtitleModelComBox->count(); ++i)
    {
        QString currentItemName = m_liveSubtitleModelComBox->itemText(i);
        if (currentItemName == folderName)
        {
            m_liveSubtitleModelComBox->setCurrentIndex(i);
            return;
        }
    }

    qDebug() << "[Model Not Found] Folder Name:" << folderName;
    ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "未找到名为" + folderName + "的模型目录", 2000);
}

void liveSettingsPage::updataConfig()
{
    // 10. 修改：替换直接调用updateConfig为添加TASK_UPDATE_CONFIG任务（带参数）
    SpeechRecognitionManager* speechMgr = SpeechRecognitionManager::getInstance();
    QMap<QString, QVariant> taskParams;
    taskParams["newConfig"] = QVariant::fromValue(m_speechConfig); // 传入新配置参数
    speechMgr->addTask(SpeechTaskType::TASK_UPDATE_CONFIG, taskParams);

    qDebug() << "[Update Config] ModelPath:" << m_speechConfig.modelPath
             << "| DeviceIndex:" << m_speechConfig.inputDeviceIndex
             << "| WsServer:" << m_speechConfig.wsServer;
}

// ------------------------------
// 新增：SpeechRecognitionManager任务结果回调实现
// ------------------------------
void liveSettingsPage::onStartRecognitionResult(bool isSuccess, const QString& errorMsg)
{
    if (isSuccess) {
        m_speechStatus.isActive = true;
        m_liveSubtitleStatusBtn->setText("停止");
        m_liveSubtitleDeviceComBox->setEnabled(false);
        m_liveSubtitleModelComBox->setEnabled(false);
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "语音识别启动成功", 2000);
    } else {
        m_speechStatus.isActive = false;
        m_liveSubtitleStatusBtn->setText("启动");
        ElaMessageBar::error(ElaMessageBarType::Top, "错误", "语音识别启动失败：" + errorMsg, 2000);
    }
    // 同步状态到全局配置
    GlobalConfig::getInstance().setValue("SpeechToTextSettings/active", m_speechStatus.isActive);
}

void liveSettingsPage::onStopRecognitionResult(bool isSuccess, const QString& errorMsg)
{
    if (isSuccess) {
        m_speechStatus.isActive = false;
        m_liveSubtitleStatusBtn->setText("启动");
        m_liveSubtitleDeviceComBox->setEnabled(true);
        m_liveSubtitleModelComBox->setEnabled(true);
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "语音识别停止成功", 2000);
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "错误", "语音识别停止失败：" + errorMsg, 2000);
    }
    // 同步状态到全局配置
    GlobalConfig::getInstance().setValue("SpeechToTextSettings/active", m_speechStatus.isActive);
}

void liveSettingsPage::onCurrentStatusResult(bool isSuccess, const SpeechRecognitionStatus& status, const QString& errorMsg)
{
    if (isSuccess) {
        // 更新本地状态缓存
        m_speechStatus = status;
        // 更新UI显示
        if (m_speechStatus.isActive) {
            m_liveSubtitleStatusBtn->setText("停止");
            m_liveSubtitleDeviceComBox->setEnabled(false);
            m_liveSubtitleModelComBox->setEnabled(false);
        } else {
            m_liveSubtitleStatusBtn->setText("启动");
            m_liveSubtitleDeviceComBox->setEnabled(true);
            m_liveSubtitleModelComBox->setEnabled(true);
        }
        // 同步设备索引选择
        if (m_speechStatus.inputDeviceIndex >= 0 && m_speechStatus.inputDeviceIndex < m_liveSubtitleDeviceComBox->count()) {
            m_liveSubtitleDeviceComBox->setCurrentIndex(m_speechStatus.inputDeviceIndex);
        }
        // 同步模型路径
        if (!m_speechStatus.modelPath.isEmpty() && m_liveSubtitleModelComBox->count()) {
            selectModelByFolderName(m_speechStatus.modelPath.mid(6));
        }
    } else {
        ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "获取状态失败：" + errorMsg + "（使用本地缓存）", 2000);
    }
}

void liveSettingsPage::onAudioDevicesResult(bool isSuccess, const QList<AudioDeviceInfo>& devices, const QString& errorMsg)
{
    if (isSuccess) {
        // 更新本地设备列表缓存
        m_speechDeviceList = devices;
        // 清空下拉框并重新添加设备
        m_liveSubtitleDeviceComBox->clear();
        for (const auto& device : std::as_const(m_speechDeviceList))
        {
            QString deviceInfo = QString("%1 (通道数: %2)").arg(device.name).arg(device.channels);
            m_liveSubtitleDeviceComBox->addItem(deviceInfo, device.index);
        }
        // 选中当前设备（从m_speechStatus获取）
        if (m_speechStatus.inputDeviceIndex >= 0 && m_speechStatus.inputDeviceIndex < m_liveSubtitleDeviceComBox->count()) {
            m_liveSubtitleDeviceComBox->setCurrentIndex(m_speechStatus.inputDeviceIndex);
        } else if (!m_speechDeviceList.isEmpty()) {
            m_liveSubtitleDeviceComBox->setCurrentIndex(0);
        }
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "错误", "获取音频设备失败：" + errorMsg, 2000);
    }
}

void liveSettingsPage::onCurrentConfigResult(bool isSuccess, const SpeechRecognitionConfig& config, const QString& errorMsg)
{
    if (isSuccess) {
        // 更新本地配置缓存
        m_speechConfig = config;
        // 同步UI显示（如模型路径、WS地址）
        selectModelByFolderName(m_speechConfig.modelPath.mid(6));
    } else {
        ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "获取配置失败：" + errorMsg + "（使用本地缓存）", 2000);
    }
}

void liveSettingsPage::onUpdateConfigResult(bool isSuccess, const QString& errorMsg)
{
    speechInitFlag = true;
    if (isSuccess) {
        ElaMessageBar::success(ElaMessageBarType::Top, "成功", "配置更新成功", 2000);
    } else {
        ElaMessageBar::error(ElaMessageBarType::Top, "错误", "配置更新失败：" + errorMsg, 2000);
    }
}
