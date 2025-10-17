#ifndef LIVESETTINGSPAGE_H
#define LIVESETTINGSPAGE_H

#include <QWidget>
#include <QList>
#include <QString>
#include "ElaPushButton.h"
#include "ElaLineEdit.h"
#include "ElaComboBox.h"
#include "ElaScrollPage.h"
#include "SpeechRecognitionManager.h" // 包含修改后的SpeechRecognitionManager头文件

class liveSettingsPage : public ElaScrollPage
{
    Q_OBJECT

public:
    liveSettingsPage(QWidget *parent = nullptr);
    ~liveSettingsPage();
    void backThreadStart();

private slots:
    void initPage();
    void speechServerStatusUpdata();
    void speechServerDeviceUpdata();
    void refreshModel();
    void selectModelByFolderName(const QString& folderName);
    void updataConfig();

    // 新增：SpeechRecognitionManager任务结果回调槽函数
    void onStartRecognitionResult(bool isSuccess, const QString& errorMsg);
    void onStopRecognitionResult(bool isSuccess, const QString& errorMsg);
    void onCurrentStatusResult(bool isSuccess, const SpeechRecognitionStatus& status, const QString& errorMsg);
    void onAudioDevicesResult(bool isSuccess, const QList<AudioDeviceInfo>& devices, const QString& errorMsg);
    void onCurrentConfigResult(bool isSuccess, const SpeechRecognitionConfig& config, const QString& errorMsg);
    void onUpdateConfigResult(bool isSuccess, const QString& errorMsg);

private:
    // 原有UI成员（保持不变）
    ElaLineEdit* m_lyricAdminAddress;
    ElaPushButton* m_lyricAdminAddressCopyBtn;
    ElaPushButton* m_lyricAdminOpenBtn;
    ElaLineEdit* m_lyricSourceAddress;
    ElaPushButton* m_lyricAddressCopyBtn;
    ElaPushButton* m_lyricAddressOpenBtn;
    ElaPushButton* m_liveSubtitleStatusBtn;
    ElaPushButton* m_liveSubtitleRefreshBtn;
    ElaComboBox* m_liveSubtitleDeviceComBox;
    ElaPushButton* m_liveSubtitleDeviceRefreshBtn;
    ElaComboBox* m_liveSubtitleModelComBox;
    ElaPushButton* m_liveSubtitleModelRefreshBtn;
    ElaPushButton* m_liveSubtitleModelDirRefreshBtn;
    ElaLineEdit* m_subtitleTestAddress;
    ElaPushButton* m_subtitleTestCopyBtn;
    ElaPushButton* m_subtitleTestOpenBtn;
    ElaLineEdit* m_subtitleAddress;
    ElaPushButton* m_subtitleAddressCopyBtn;
    ElaPushButton* m_subtitleOpenBtn;

    // 原有业务成员（保持不变）
    bool speechInitFlag = false;
    SpeechRecognitionStatus m_speechStatus;
    SpeechRecognitionConfig m_speechConfig;
    QList<AudioDeviceInfo> m_speechDeviceList;
};

#endif // LIVESETTINGSPAGE_H
