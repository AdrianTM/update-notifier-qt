#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QSettings>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void save();

private:
    void buildUi();
    void load();
    void updateIconPreviews(const QString& theme);
    bool toBool(const QString& key, bool defaultValue);

public:
    QSettings* settings;

    QListWidget* iconThemeList;
    QLabel* previewUpToDate;
    QLabel* previewUpdatesAvailable;
    QCheckBox* autoHide;
    QCheckBox* notify;
    QCheckBox* startLogin;
    QComboBox* upgradeMode;
    QLineEdit* helper;
};

#endif // SETTINGS_DIALOG_H