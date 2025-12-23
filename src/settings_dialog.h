#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
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
    bool toBool(const QString& key, bool defaultValue);

public:
    QSettings* settings;

    QComboBox* iconTheme;
    QCheckBox* autoHide;
    QCheckBox* notify;
    QCheckBox* startLogin;
    QComboBox* upgradeMode;
    QLineEdit* helper;
};

#endif // SETTINGS_DIALOG_H