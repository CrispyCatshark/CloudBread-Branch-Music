#include "settingpage.h"
#include "globalconfig.h"  // 引入全局配置类

#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ElaApplication.h"
#include "ElaComboBox.h"
#include "ElaRadioButton.h"
#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToggleSwitch.h"
#include "ElaWindow.h"

SettingPage::SettingPage(QWidget* parent)
    : ElaScrollPage(parent)
{
    // 预览窗口标题
    ElaWindow* window = dynamic_cast<ElaWindow*>(parent);
    setWindowTitle("Setting");

    ElaText* themeText = new ElaText("主题设置", this);
    themeText->setWordWrap(false);
    themeText->setTextPixelSize(18);

    _themeComboBox = new ElaComboBox(this);
    _themeComboBox->addItem("日间模式");
    _themeComboBox->addItem("夜间模式");
    ElaScrollPageArea* themeSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* themeSwitchLayout = new QHBoxLayout(themeSwitchArea);
    ElaText* themeSwitchText = new ElaText("主题切换", this);
    themeSwitchText->setWordWrap(false);
    themeSwitchText->setTextPixelSize(15);
    themeSwitchLayout->addWidget(themeSwitchText);
    themeSwitchLayout->addStretch();
    themeSwitchLayout->addWidget(_themeComboBox);
    connect(_themeComboBox, QOverload<int>::of(&ElaComboBox::currentIndexChanged), this, [=](int index) {
        if (index == 0)
        {
            eTheme->setThemeMode(ElaThemeType::Light);
        }
        else
        {
            eTheme->setThemeMode(ElaThemeType::Dark);
        }
        // 保存主题设置
        GlobalConfig::getInstance().setValue("Appearance/Theme", index);
    });
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=](ElaThemeType::ThemeMode themeMode) {
        _themeComboBox->blockSignals(true);
        if (themeMode == ElaThemeType::Light)
        {
            _themeComboBox->setCurrentIndex(0);
            GlobalConfig::getInstance().setValue("Appearance/Theme", 0);
        }
        else
        {
            _themeComboBox->setCurrentIndex(1);
            GlobalConfig::getInstance().setValue("Appearance/Theme", 1);
        }
        _themeComboBox->blockSignals(false);
    });

    ElaText* helperText = new ElaText("应用程序设置", this);
    helperText->setWordWrap(false);
    helperText->setTextPixelSize(18);

    _micaSwitchButton = new ElaToggleSwitch(this);
    ElaScrollPageArea* micaSwitchArea = new ElaScrollPageArea(this);
    QHBoxLayout* micaSwitchLayout = new QHBoxLayout(micaSwitchArea);
    ElaText* micaSwitchText = new ElaText("启用云母效果(跨平台)", this);
    micaSwitchText->setWordWrap(false);
    micaSwitchText->setTextPixelSize(15);
    micaSwitchLayout->addWidget(micaSwitchText);
    micaSwitchLayout->addStretch();
    micaSwitchLayout->addWidget(_micaSwitchButton);
    connect(_micaSwitchButton, &ElaToggleSwitch::toggled, this, [=](bool checked) {
        eApp->setIsEnableMica(checked);
        // 保存云母效果设置
        GlobalConfig::getInstance().setValue("Appearance/EnableMica", checked);
    });

    _minimumButton = new ElaRadioButton("Minimum", this);
    _compactButton = new ElaRadioButton("Compact", this);
    _maximumButton = new ElaRadioButton("Maximum", this);
    _autoButton = new ElaRadioButton("Auto", this);
    ElaScrollPageArea* displayModeArea = new ElaScrollPageArea(this);
    QHBoxLayout* displayModeLayout = new QHBoxLayout(displayModeArea);
    ElaText* displayModeText = new ElaText("导航栏模式选择", this);
    displayModeText->setWordWrap(false);
    displayModeText->setTextPixelSize(15);
    displayModeLayout->addWidget(displayModeText);
    displayModeLayout->addStretch();
    displayModeLayout->addWidget(_minimumButton);
    displayModeLayout->addWidget(_compactButton);
    displayModeLayout->addWidget(_maximumButton);
    displayModeLayout->addWidget(_autoButton);

    // 导航栏模式变更时保存设置
    auto saveNavigationMode = [=](int mode) {
        GlobalConfig::getInstance().setValue("Navigation/DisplayMode", mode);
    };

    connect(_minimumButton, &ElaRadioButton::toggled, this, [=](bool checked) {
        if (checked)
        {
            window->setNavigationBarDisplayMode(ElaNavigationType::Minimal);
            saveNavigationMode(0);
        }
    });
    connect(_compactButton, &ElaRadioButton::toggled, this, [=](bool checked) {
        if (checked)
        {
            window->setNavigationBarDisplayMode(ElaNavigationType::Compact);
            saveNavigationMode(1);
        }
    });
    connect(_maximumButton, &ElaRadioButton::toggled, this, [=](bool checked) {
        if (checked)
        {
            window->setNavigationBarDisplayMode(ElaNavigationType::Maximal);
            saveNavigationMode(2);
        }
    });
    connect(_autoButton, &ElaRadioButton::toggled, this, [=](bool checked) {
        if (checked)
        {
            window->setNavigationBarDisplayMode(ElaNavigationType::Auto);
            saveNavigationMode(3);
        }
    });

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setWindowTitle("软件设置");
    QVBoxLayout* centerLayout = new QVBoxLayout(centralWidget);
    centerLayout->addSpacing(30);
    centerLayout->addWidget(themeText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(themeSwitchArea);
    centerLayout->addSpacing(15);
    centerLayout->addWidget(helperText);
    centerLayout->addSpacing(10);
    centerLayout->addWidget(micaSwitchArea);
    centerLayout->addWidget(displayModeArea);
    centerLayout->addStretch();
    centerLayout->setContentsMargins(0, 0, 0, 0);
    addCentralWidget(centralWidget, true, true, 0);

    // 加载保存的设置
    loadSettings();
}

SettingPage::~SettingPage()
{
}

// 加载保存的设置
void SettingPage::loadSettings()
{
    // 加载主题设置，默认使用日间模式(0)
    int themeIndex = GlobalConfig::getInstance().getValue("Appearance/Theme", 0).toInt();
    _themeComboBox->setCurrentIndex(themeIndex);

    // 加载云母效果设置，默认禁用(false)
    bool enableMica = GlobalConfig::getInstance().getValue("Appearance/EnableMica", false).toBool();
    _micaSwitchButton->setIsToggled(enableMica);
    eApp->setIsEnableMica(enableMica);

    // 加载导航栏模式设置，默认自动模式(3)
    int navMode = GlobalConfig::getInstance().getValue("Navigation/DisplayMode", 3).toInt();
    _minimumButton->blockSignals(true);
    _compactButton->blockSignals(true);
    _maximumButton->blockSignals(true);
    _autoButton->blockSignals(true);

    switch(navMode)
    {
    case 0: _minimumButton->setChecked(true); break;
    case 1: _compactButton->setChecked(true); break;
    case 2: _maximumButton->setChecked(true); break;
    case 3: _autoButton->setChecked(true); break;
    default: _autoButton->setChecked(true);
    }

    _minimumButton->blockSignals(false);
    _compactButton->blockSignals(false);
    _maximumButton->blockSignals(false);
    _autoButton->blockSignals(false);
}
