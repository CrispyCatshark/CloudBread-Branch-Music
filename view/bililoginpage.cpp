#include "biliLoginPage.h"
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QStyleOption>

BiliLoginPage::BiliLoginPage(QWidget *parent)
    : QWidget(parent), m_targetWidget(nullptr)
{
    initUI();
}

void BiliLoginPage::initUI()
{
    // 设置窗口属性：无边框、始终在最前、半透明
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    // 创建主布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setAlignment(Qt::AlignCenter);

    // 创建登录容器
    QWidget *loginContainer = new QWidget(this);
    loginContainer->setStyleSheet(R"(
        background-color: white;
        border-radius: 10px;
        padding: 20px;
    )");

    QVBoxLayout *containerLayout = new QVBoxLayout(loginContainer);
    containerLayout->setSpacing(20);
    containerLayout->setAlignment(Qt::AlignCenter);

    // 创建标题标签
    QLabel *titleLabel = new QLabel("需要登录才能访问", loginContainer);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #333;");
    titleLabel->setAlignment(Qt::AlignCenter);

    // 创建二维码标签
    m_qrCodeLabel = new QLabel(loginContainer);
    m_qrCodeLabel->setFixedSize(200, 200);
    m_qrCodeLabel->setAlignment(Qt::AlignCenter);
    m_qrCodeLabel->setStyleSheet("border: 1px solid #eee;");

    // 创建提示信息标签
    m_infoLabel = new QLabel("请使用哔哩哔哩APP扫描二维码登录", loginContainer);
    m_infoLabel->setStyleSheet("font-size: 14px; color: #666;");
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setWordWrap(true);

    // 创建关闭按钮
    m_closeButton = new QPushButton("取消", loginContainer);
    m_closeButton->setStyleSheet(R"(
        QPushButton {
            background-color: #f0f0f0;
            color: #333;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            font-size: 14px;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
    )");
    connect(m_closeButton, &QPushButton::clicked, this, &BiliLoginPage::onCloseClicked);

    // 添加组件到容器布局
    containerLayout->addWidget(titleLabel);
    containerLayout->addWidget(m_qrCodeLabel);
    containerLayout->addWidget(m_infoLabel);

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setAlignment(Qt::AlignCenter);
    buttonLayout->addWidget(m_closeButton);
    containerLayout->addLayout(buttonLayout);

    // 添加容器到主布局
    m_mainLayout->addWidget(loginContainer);

    // 创建登录检查定时器
    m_loginCheckTimer = new QTimer(this);
    m_loginCheckTimer->setInterval(3000); // 每3秒检查一次
    connect(m_loginCheckTimer, &QTimer::timeout, this, &BiliLoginPage::checkLoginStatus);
}

void BiliLoginPage::setTargetWidget(QWidget *target)
{
    if (target) {
        m_targetWidget = target;
        // 设置与目标窗口相同的大小和位置
        setGeometry(target->geometry());
        // 监听目标窗口的大小变化
        // connect(target, &QWidget::geometryChanged, this, [this](const QRect &rect) {
        //     setGeometry(rect);
        // });
    }
}

void BiliLoginPage::updateQRCode(const QPixmap &qrcode)
{
    if (!qrcode.isNull()) {
        // 缩放二维码并保持比例
        QPixmap scaledQr = qrcode.scaled(
            m_qrCodeLabel->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            );
        m_qrCodeLabel->setPixmap(scaledQr);
    }
}

void BiliLoginPage::showLoginPage()
{
    if (m_targetWidget) {
        // 显示在目标窗口上方
        show();
        m_loginCheckTimer->start();
    }
}

void BiliLoginPage::hideLoginPage()
{
    m_loginCheckTimer->stop();
    hide();
}

void BiliLoginPage::checkLoginStatus()
{
    // 这里应该是实际检查登录状态的逻辑
    // 例如通过网络请求检查用户是否已登录
    // 这里只是示例，实际使用时需要替换为真实的检查逻辑

    // 模拟登录成功 - 在实际应用中删除这部分
    // static int count = 0;
    // if (count++ > 5) { // 15秒后模拟登录成功
    //     emit loginSuccess();
    //     hideLoginPage();
    // }
}

void BiliLoginPage::onCloseClicked()
{
    emit loginCancelled();
    hideLoginPage();
}

// 重绘事件，绘制半透明遮罩
void BiliLoginPage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    // 绘制半透明黑色遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 150));

    // 确保样式表生效
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}
