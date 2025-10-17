#ifndef FIRSTRUNSETUP_H
#define FIRSTRUNSETUP_H

#include "GlobalConfig.h"
#include <QWidget>
#include <QSettings>
#include <QDir>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QMessageBox;
QT_END_NAMESPACE

class FirstRunSetup : public QWidget
{
    Q_OBJECT

public:
    FirstRunSetup(QWidget *parent = nullptr);
    ~FirstRunSetup() override;
    bool initDatabaseTables(const QString& dbPath);

private slots:
    void onBrowseClicked();
    void onContinueClicked();
    void onUseExistingClicked();
    void onClearExistingClicked();

private:
    void setupUI();
    bool checkDirectoryStructure(const QString &path);
    bool createDirectoryStructure(const QString &path);
    bool clearDirectoryContents(const QString &path);
    void showMainWindow();

    QLabel *m_titleLabel;
    QLabel *m_descriptionLabel;
    QLabel *m_pathLabel;
    QLineEdit *m_pathEdit;
    QPushButton *m_browseButton;
    QPushButton *m_continueButton;

    // 用于显示现有数据提示的控件
    QWidget *m_existingDataWidget;
    QLabel *m_existingDataLabel;
    QPushButton *m_useExistingButton;
    QPushButton *m_clearExistingButton;

    QString m_selectedPath;
    GlobalConfig& m_settings = GlobalConfig::getInstance();
};

#endif // FIRSTRUNSETUP_H
