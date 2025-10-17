#ifndef BILILOGINPAGE_H
#define BILILOGINPAGE_H

#include <QWidget>
#include <QPixmap>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTimer>

class BiliLoginPage : public QWidget
{
    Q_OBJECT
public:
    explicit BiliLoginPage(QWidget *parent = nullptr);

    // 设置需要遮挡的目标窗口
    void setTargetWidget(QWidget *target);

    // 更新二维码图片
    void updateQRCode(const QPixmap &qrcode);

    // 显示登录页面
    void showLoginPage();

    // 隐藏登录页面
    void hideLoginPage();

signals:
    // 登录成功信号
    void loginSuccess();

    // 关闭登录页面信号
    void loginCancelled();

private slots:
    // 检查登录状态
    void checkLoginStatus();

    // 关闭登录页面
    void onCloseClicked();

protected:
    // 事件重写
    void paintEvent(QPaintEvent* event) override;

private:
    // 初始化UI组件
    void initUI();

    // 目标窗口
    QWidget *m_targetWidget;

    // 二维码标签
    QLabel *m_qrCodeLabel;

    // 提示信息标签
    QLabel *m_infoLabel;

    // 关闭按钮
    QPushButton *m_closeButton;

    // 布局管理器
    QVBoxLayout *m_mainLayout;

    // 检查登录状态的定时器
    QTimer *m_loginCheckTimer;
};

#endif // BILILOGINPAGE_H
