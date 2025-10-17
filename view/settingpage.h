#ifndef T_SETTINGPAGE_H
#define T_SETTINGPAGE_H

#include <QObject>

#include "ElaScrollPage.h"

class ElaRadioButton;
class ElaToggleSwitch;
class ElaComboBox;
class SettingPage : public ElaScrollPage
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit SettingPage(QWidget* parent = nullptr);
    ~SettingPage();

private:
    ElaComboBox* _themeComboBox{nullptr};
    ElaToggleSwitch* _micaSwitchButton{nullptr};
    ElaToggleSwitch* _logSwitchButton{nullptr};
    ElaRadioButton* _minimumButton{nullptr};
    ElaRadioButton* _compactButton{nullptr};
    ElaRadioButton* _maximumButton{nullptr};
    ElaRadioButton* _autoButton{nullptr};

    void loadSettings();
};

#endif // T_SETTINGPAGE_H
