#include "mainwindow.h"

#include <QVBoxLayout>
#include <qapplication.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qprocess.h>
#include "webserver.h"
#include "globalconfig.h"
#include "ElaWindow.h"
#include "ElaStatusBar.h"
#include "LyricAndCoverBatchFetcher.h"
#include "SpeechRecognitionManager.h"

MainWindow::MainWindow(QWidget *parent)
    : ElaWindow{parent}, m_process(new QProcess(this))  // 将process改为成员变量
{
    this->setIsDefaultClosed(false);
    connect(this, &MainWindow::closeButtonClicked, this, &MainWindow::closeAPP);

    InitWindow();
    InitNav();
    StartWebServer();
    StartWsServer();

    // 配置并启动后端程序
    QString backendPath = QApplication::applicationDirPath() + "/CloudBreadMusicBackThread.exe";
    qDebug() << "启动后端程序: " << backendPath;

    m_process = new QProcess(this);
    m_process->setProgram(backendPath);
    m_process->setArguments(QStringList());
    m_process->setProcessChannelMode(QProcess::MergedChannels);  // 合并输出通道

    // 连接输出信号
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        QByteArray output = m_process->readAllStandardOutput();
        qDebug() << "后端输出: " << output;
    });

    // 连接错误信号
    connect(m_process, &QProcess::readyReadStandardError, [this]() {
        QByteArray errorOutput = m_process->readAllStandardError();
        qDebug() << "后端错误: " << errorOutput;
    });

    // 后端启动成功后执行的代码
    connect(m_process, &QProcess::started, this, [this]() {
        qDebug() << "后端程序启动成功!";

        // 在这里添加后端启动成功后需要执行的代码
        // 例如：
        // initAfterBackendStart();
        // 或者发送初始化信号等

        LyricAndCoverBatchFetcher::getInstance()->startBatchFetch();
        // m_speechManager = SpeechRecognitionManager::getInstance();
        _liveSettingsPage->backThreadStart();
    });

    // 后端程序退出时的处理
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                qDebug() << "后端程序已退出，退出码: " << exitCode;
                if (exitStatus == QProcess::CrashExit) {
                    qWarning() << "后端程序异常崩溃";
                }
            });

    // 启动后端程序
    m_process->start();

    // 应用导航栏模式设置
    int navMode = GlobalConfig::getInstance().getValue("Navigation/DisplayMode", 3).toInt();
    switch(navMode)
    {
    case 0: setNavigationBarDisplayMode(ElaNavigationType::Minimal); break;
    case 1: setNavigationBarDisplayMode(ElaNavigationType::Compact); break;
    case 2: setNavigationBarDisplayMode(ElaNavigationType::Maximal); break;
    case 3: setNavigationBarDisplayMode(ElaNavigationType::Auto); break;
    default: setNavigationBarDisplayMode(ElaNavigationType::Auto);
    }
}

void MainWindow::terminateBackendProcess()
{
    SpeechRecognitionManager::getInstance()->addTask(SpeechTaskType::TASK_SHUTDOWN_SYSTEM);
    /*if (!m_speechManager) {
        qWarning() << "SpeechRecognitionManager 实例获取失败";
    }

    qDebug() << "准备发送服务器关闭请求...";

    // 发送带认证的POST请求到关闭API
    QString response = m_speechManager->sendAuthenticatedPostRequest("/system/shutdown");

    if (response.isEmpty()) {
        qWarning() << "服务器关闭请求发送失败，无响应";
    }

    // 解析响应
    bool success = false;
    QJsonObject json = m_speechManager->parseJsonResponse(response, success);

    if (!success) {
        qWarning() << "解析服务器关闭响应失败";
    }

    int code = json["code"].toInt(-1);
    QString message = json["message"].toString("未知响应");

    if (code == 200) {
        qDebug() << "服务器关闭请求已接受:" << message;
        // 关闭本地WebSocket连接
        m_speechManager->closeWebSocket();
        return;
    } else {
        qWarning() << "服务器关闭请求失败，错误码:" << code << "消息:" << message;
    }*/
}

MainWindow::~MainWindow() {
}

void MainWindow::InitWindow()
{
    setWindowIcon(QIcon(":/cloudbread/src/favicon_round.png"));
    setWindowTitle("云朵面包-音乐家V1");
    setNavigationBarDisplayMode(ElaNavigationType::Compact);
    resize(1200, 740);

    setUserInfoCardVisible(false);

    //初始化系统托盘
    QIcon icon(":/cloudbread/src/favicon_round.ico");
    SysIcon = new QSystemTrayIcon(this);
    SysIcon->setIcon(icon);
    SysIcon->setToolTip("云朵面包-音乐家V1");

    SystemTrayMenu = new ElaMenu(this);
    SystemTrayMenu->setMenuItemHeight(27);
    QAction* action = nullptr;
    action = SystemTrayMenu->addElaIconAction(ElaIconType::House, "显示主页");
    action->setEnabled(true);
    connect(action, &QAction::triggered, this, &MainWindow::show_main_page);
    action = SystemTrayMenu->addElaIconAction(ElaIconType::XmarkLarge, "退出");
    action->setEnabled(true);
    connect(action, &QAction::triggered, this, &MainWindow::closeAPP);

    SysIcon->setContextMenu(SystemTrayMenu);
    SysIcon->show();
    moveToCenter();

    m_toolbar = new FloatingToolbar();
    m_toolbar->show();

    // this->setWindowState(Qt::WindowMinimized);
    connect(m_toolbar, &FloatingToolbar::showMainPage, this, &MainWindow::show_hide_page);

    ElaStatusBar* statusBar = new ElaStatusBar(this);
    m_statusText = new ElaText("初始化成功！", this);
    m_statusText->setTextPixelSize(14);
    m_statusText->setWordWrap(false);
    statusBar->addWidget(m_statusText);
    this->setStatusBar(statusBar);

    LyricAndCoverBatchFetcher* fetcher = LyricAndCoverBatchFetcher::getInstance();
    // 任务开始
    connect(fetcher, &LyricAndCoverBatchFetcher::batchFetchStarted, this, [=](int total){
        this->m_statusText->setText(QString("开始补全，共%1首音乐").arg(total));
    });
    // 进度更新
    connect(fetcher, &LyricAndCoverBatchFetcher::batchFetchProgress, this, [=](int current, int total, const QString& name){
        this->m_statusText->setText(QString("处理中: %1 (%2/%3)").arg(name).arg(current).arg(total));
    });
    // 任务结束
    connect(fetcher, &LyricAndCoverBatchFetcher::batchFetchFinished, this, [=](int success, int fail){
        this->m_statusText->setText(QString("初始化成功").arg(success).arg(fail));
    });
}

void MainWindow::InitNav()
{
    _musicPlayerPage = new musicPlayerPage(this);
    _musicListPage = new musicListPage(this);
    _liveSettingsPage = new liveSettingsPage(this);
    // _biliCountPage = new biliCountPage(this);

    addPageNode("播放器", _musicPlayerPage, ElaIconType::Music);
    addPageNode("曲库", _musicListPage, ElaIconType::ListMusic);
    addPageNode("直播设置", _liveSettingsPage, ElaIconType::Camcorder);
    // addPageNode("数据统计", _biliCountPage, ElaIconType::ChartMixed);

    /*_biliGiftPage = new biliGiftPage(this);
    _biliGuardPage = new biliGuardPage(this);
    _biliHistoryPage = new biliHistoryPage(this);
    _biliLiveStatus = new biliLiveStatus(this);

    addExpanderNode("直播数据", _biliPageKey, ElaIconType::ChartMixed);
    addPageNode("实时数据", _biliLiveStatus, _biliPageKey, ElaIconType::Video);
    addPageNode("历史场次", _biliHistoryPage, _biliPageKey, ElaIconType::ClockRotateLeft);
    addPageNode("大航海记录", _biliGuardPage, _biliPageKey, ElaIconType::Ship);
    addPageNode("礼物记录", _biliGiftPage, _biliPageKey, ElaIconType::Gift);*/

    _aboutPage = new AboutPage();
    _settingPage = new SettingPage(this);

    addFooterNode("关于", nullptr, _aboutKey, 0, ElaIconType::CircleInfo);

    _aboutPage->hide();
    connect(this, &ElaWindow::navigationNodeClicked, this, [=](ElaNavigationType::NavigationNodeType nodeType, QString nodeKey) {
        if (_aboutKey == nodeKey)
        {
            _aboutPage->setFixedSize(600, 550);
            // _aboutPage->moveToCenter();
            _aboutPage->show();
        }
    });

    addFooterNode("软件设置", _settingPage, _settingKey, 0, ElaIconType::GearComplex);
}

void MainWindow::show_hide_page()
{
    if (isMinimized()) {
        this->show();
        this->showNormal();
        this->activateWindow();
    } else {
        this->setWindowState(Qt::WindowMinimized);
    }
}

void MainWindow::show_main_page_minisize()
{
    _closeDialog->close();
    this->setWindowState(Qt::WindowMinimized);
}

void MainWindow::show_main_page()
{
    this->show();
    this->showNormal();
    this->activateWindow();
}

void MainWindow::closeAPP()
{
    GlobalConfig::getInstance().sync();
    _closeDialog = new ElaContentDialog(this);
    _closeDialog->setLeftButtonText("取消");
    _closeDialog->setMiddleButtonText("最小化");
    _closeDialog->setRightButtonText("退出");
    connect(_closeDialog, &ElaContentDialog::rightButtonClicked, this, &MainWindow::APPclose);
    connect(_closeDialog, &ElaContentDialog::middleButtonClicked, this, &MainWindow::show_main_page_minisize);
    _closeDialog->exec();
}

void MainWindow::APPclose()
{
    terminateBackendProcess();
    WsServerThread& serverThread = WsServerThread::getInstance();
    serverThread.stopServer();
    QApplication* app = nullptr;
    app->quit();
}

void MainWindow::StartWebServer()
{
    // 获取配置的端口号
    quint16 webPort = GlobalConfig::getInstance().getValue("Server/WebPort", 5320).toInt();

    // 设置静态文件根目录（根据实际情况修改）
    m_webServer.setStaticRoot(QCoreApplication::applicationDirPath() + "/www");

    // 注册动态路径：返回当前时间
    m_webServer.registerDynamicPath("/time", [](const QHttpServerRequest &request) {
        Q_UNUSED(request);
        QJsonObject json;
        json["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        json["message"] = "当前服务器时间";
        return QHttpServerResponse("application/json",
                                   QJsonDocument(json).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    });

    // 注册动态路径：返回请求信息
    m_webServer.registerDynamicPath("/info", [](const QHttpServerRequest &request) {
        QJsonObject json;
        json["path"] = request.url().path();
        json["remoteAddress"] = request.remoteAddress().toString();

        return QHttpServerResponse("application/json",
                                   QJsonDocument(json).toJson(),
                                   QHttpServerResponse::StatusCode::Ok);
    });

    // 启动服务器
    if (!m_webServer.start(webPort)) {
        qCritical() << "无法启动Web服务器，端口" << webPort;
    } else {
        qInfo() << "Web服务器启动成功，访问 http://localhost:" << webPort;
    }
}

void MainWindow::StartWsServer()
{
    // 获取配置的端口号
    quint16 wsPort = GlobalConfig::getInstance().getValue("Server/WsPort", 5310).toInt();

    // 收到客户端消息
    // QObject::connect(&m_wsServer, &WsServer::messageReceived, this,
    //                  [this](QWebSocket* client, const QJsonObject& json) {
    //                      // qDebug() << "Received message from client:" << json;
    //                 });

    // 客户端连接
    QObject::connect(&m_wsServer, &WsServer::clientConnected, this,
                     [](QWebSocket* client) {
                         qDebug() << "Client connected:" << client->peerAddress().toString();
                     });

    // 客户端断开连接
    QObject::connect(&m_wsServer, &WsServer::clientDisconnected, this,
                     [](QWebSocket* client) {
                         qDebug() << "Client disconnected:" << client->peerAddress().toString();
                     });

    // 错误发生
    QObject::connect(&m_wsServer, &WsServer::errorOccurred, this,
                     [](const QString& errorString) {
                         qWarning() << "Server error:" << errorString;
                     });

    m_wsServerThread.startServer(wsPort);
    qInfo() << "WebSocket服务器启动成功，端口:" << wsPort;
}
