#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSettings>
#include <ElaWindow.h>
#include <ElaContentDialog.h>
#include <ElaMenu.h>
#include <qprocess.h>

#include "aboutpage.h"
#include "floatingtoolbar.h"
#include "livesettingspage.h"
#include "musicListPage.h"
#include "musicplayerpage.h"
#include "settingpage.h"
#include "speechrecognitionmanager.h"
#include "webserver.h"
#include "wsserver.h"

#include "biligiftpage.h"
#include "biliguardpage.h"
#include "bilihistorypage.h"
#include "bililivestatus.h"

class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void InitWindow();
    void InitNav();

    void show_main_page();
    void show_hide_page();
    void show_main_page_minisize();
    void closeAPP();
    void APPclose();

    void StartWebServer();
    void StartWsServer();
    void terminateBackendProcess();

    QProcess* m_process;
    // SpeechRecognitionManager* m_speechManager;
    QSystemTrayIcon *SysIcon;
    ElaMenu* SystemTrayMenu;
    FloatingToolbar *m_toolbar;
    ElaContentDialog* _closeDialog{nullptr};
    QSettings settings;

    musicListPage *_musicListPage;
    musicPlayerPage *_musicPlayerPage;
    // biliCountPage *_biliCountPage;
    liveSettingsPage *_liveSettingsPage;

    biliGiftPage *_biliGiftPage;
    biliGuardPage *_biliGuardPage;
    biliHistoryPage *_biliHistoryPage;
    // biliLiveStatus *_biliLiveStatus;

    AboutPage *_aboutPage;
    SettingPage *_settingPage;

    ElaText* m_statusText;

    QString _aboutKey{""};
    QString _settingKey{""};
    QString _biliPageKey{""};

    WebServer m_webServer;
    WsServerThread& m_wsServerThread = WsServerThread::getInstance();
    WsServer& m_wsServer = WsServer::getInstance();
};
#endif // MAINWINDOW_H
