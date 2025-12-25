#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QSettings>

class SettingsService;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(SettingsService* service = nullptr, QWidget* parent = nullptr);

private slots:
    void save();

private:
    void buildUi();
    void load();
    void updateIconPreviews(const QString& theme);
    bool toBool(const QString& key, bool defaultValue);

public:
    QSettings* settings;
    SettingsService* service;

    QCheckBox* autoHide;
    QCheckBox* notify;
    QCheckBox* startLogin;
    QCheckBox* autoUpgrade;
    QLabel* previewUpdatesAvailable;
    QLabel* previewUpToDate;
    QListWidget* iconThemeList;
    QSpinBox* checkIntervalValue;
    QComboBox* checkIntervalUnit;
    QLineEdit* packageManager;
};

#endif // SETTINGS_DIALOG_H