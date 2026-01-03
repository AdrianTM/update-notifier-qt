#pragma once

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
    void onAurEnabledToggled(bool enabled);

private:
    void buildUi();
    void load();
    void updateIconPreviews(const QString& theme);
    void updateAurHelperOptions();
    bool toBool(const QString& key, bool defaultValue);

public:
    QSettings* settings;
    SettingsService* service;

    QCheckBox* autoHide;
    QCheckBox* notify;
    QCheckBox* startLogin;
    QLabel* previewUpdatesAvailable;
    QLabel* previewUpToDate;
    QListWidget* iconThemeList;
    QSpinBox* checkIntervalValue;
    QComboBox* checkIntervalUnit;
    QLineEdit* packageManager;
    QCheckBox* aurEnabled;
    QComboBox* aurHelper;
    QLabel* aurStatus;
};
