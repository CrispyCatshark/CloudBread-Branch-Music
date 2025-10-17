#include "aboutpage.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>

#include "ElaImageCard.h"
#include "ElaText.h"

AboutPage::AboutPage(QWidget* parent)
    : ElaWidget(parent)
{
    setWindowTitle("关于 云朵面包-音乐家V1");
    setWindowIcon(QIcon(":/cloudbread/src/favicon_round.png"));
    setWindowModality(Qt::ApplicationModal);
    setWindowButtonFlags(ElaAppBarType::CloseButtonHint);
    //setWindowFlags(windowFlags() & ~Qt::WindowMinimizeButtonHint);
    setIsFixedSize(true);
    setWindowModality(Qt::ApplicationModal);
    //setWindowButtonFlags(ElaAppBarType::CloseButtonHint);

    ElaImageCard* pixCard = new ElaImageCard(this);
    pixCard->setFixedSize(70, 70);
    pixCard->setIsPreserveAspectCrop(false);
    pixCard->setCardImage(QImage(":/cloudbread/src/favicon_round.png"));

    QVBoxLayout* pixCardLayout = new QVBoxLayout();
    pixCardLayout->addWidget(pixCard);
    pixCardLayout->addStretch();

    ElaText* appNameText = new ElaText(QString("云朵面包-音乐家V1"), this);
    QFont appNameFont = appNameText->font();
    appNameFont.setWeight(QFont::Bold);
    appNameText->setFont(appNameFont);
    appNameText->setWordWrap(false);
    appNameText->setTextPixelSize(18);

    ElaText* versionText = new ElaText("核心版本：" + QString("0.1.3"), this);
    versionText->setWordWrap(false);
    versionText->setTextPixelSize(14);
    ElaText* supportText = new ElaText("开发者：脆脆的猫猫鲨\n开发者邮箱：zxmmh@foxmail.com\n"
                                       "此版本是用于验证功能提供的抢先版，非正式版本。\n"
                                       "此版本不具备自动更新和相关软件分支更新及补丁推送功能", this);
    supportText->setWordWrap(false);
    supportText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    supportText->setTextPixelSize(14);
    ElaText* otherSupportText = new ElaText("捉虫：一行ikinn\n哔哩主页:https://space.bilibili.com/441311847\n部分美术资源：萌起MENGQI\n哔哩主页：https://space.bilibili.com/2130239542", this);
    otherSupportText->setWordWrap(false);
    otherSupportText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    otherSupportText->setTextPixelSize(14);
    ElaText* libraryText = new ElaText("使用开源项目声明：\n此软件UI依赖ElaWidgetTools\n版权所有 © 2024 Liniyous\n"
                                       "开源库位于https://github.com/Liniyous/ElaWidgetTools"
                                       "\n\n此软件部分网络组件依赖libhv\n开源库位于https://github.com/ithewei/libhv", this);
    libraryText->setWordWrap(false);
    libraryText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    libraryText->setTextPixelSize(14);
    ElaText* helperText = new ElaText("获取支持请前往官方论坛或官方QQ群", this);
    helperText->setWordWrap(false);
    helperText->setTextPixelSize(14);
    ElaText* copyrightText = new ElaText("版权所有 © 2025 脆脆的猫猫鲨", this);
    copyrightText->setWordWrap(false);
    copyrightText->setTextPixelSize(14);

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(15);
    textLayout->addWidget(appNameText);
    textLayout->addWidget(versionText);
    textLayout->addWidget(supportText);
    textLayout->addWidget(otherSupportText);
    textLayout->addWidget(libraryText);
    textLayout->addWidget(helperText);
    textLayout->addWidget(copyrightText);
    textLayout->addStretch();

    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->addSpacing(30);
    contentLayout->addLayout(pixCardLayout);
    contentLayout->addSpacing(30);
    contentLayout->addLayout(textLayout);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 25, 0, 0);
    mainLayout->addLayout(contentLayout);
}

AboutPage::~AboutPage()
{
}
