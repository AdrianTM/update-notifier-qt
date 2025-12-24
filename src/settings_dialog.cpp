#include "settings_dialog.h"
#include "common.h"

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , settings(new QSettings(APP_ORG, APP_NAME, this))
{
    setWindowTitle(QStringLiteral("MX Arch Updater Settings"));
    resize(480, 300);
    buildUi();
    load();
}

void SettingsDialog::buildUi() {
    iconTheme = new QComboBox(this);
    iconTheme->addItems(ICON_THEMES);
    autoHide = new QCheckBox(QStringLiteral("Hide tray icon when no updates"), this);
    notify = new QCheckBox(QStringLiteral("Notify when updates are available"), this);
    startLogin = new QCheckBox(QStringLiteral("Start at login"), this);
    upgradeMode = new QComboBox(this);
    upgradeMode->addItems(UPGRADE_MODES);
    helper = new QLineEdit(this);
    helper->setPlaceholderText(QStringLiteral("paru"));

    QFormLayout* form = new QFormLayout();
    form->addRow(QStringLiteral("Icon theme"), iconTheme);
    form->addRow(QStringLiteral("Auto hide"), autoHide);
    form->addRow(QStringLiteral("Notifications"), notify);
    form->addRow(QStringLiteral("Start at login"), startLogin);
    form->addRow(QStringLiteral("Upgrade mode"), upgradeMode);
    form->addRow(QStringLiteral("Helper"), helper);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        this
    );
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addStretch(1);
    layout->addWidget(buttons);
}

void SettingsDialog::load() {
    iconTheme->setCurrentText(settings->value(QStringLiteral("Settings/icon_theme"), QStringLiteral("wireframe-dark")).toString());
    autoHide->setChecked(toBool(QStringLiteral("Settings/auto_hide"), false));
    notify->setChecked(toBool(QStringLiteral("Settings/notify"), true));
    startLogin->setChecked(toBool(QStringLiteral("Settings/start_at_login"), true));
    upgradeMode->setCurrentText(settings->value(QStringLiteral("Settings/upgrade_mode"), QStringLiteral("basic")).toString());
    helper->setText(settings->value(QStringLiteral("Settings/helper"), QStringLiteral("paru")).toString());
}

bool SettingsDialog::toBool(const QString& key, bool defaultValue) {
    QString value = settings->value(key, defaultValue).toString().toLower();
    return value == QStringLiteral("true") || value == QStringLiteral("1") || value == QStringLiteral("yes");
}

void SettingsDialog::save() {
    settings->setValue(QStringLiteral("Settings/icon_theme"), iconTheme->currentText());
    settings->setValue(QStringLiteral("Settings/auto_hide"), autoHide->isChecked());
    settings->setValue(QStringLiteral("Settings/notify"), notify->isChecked());
    settings->setValue(QStringLiteral("Settings/start_at_login"), startLogin->isChecked());
    settings->setValue(QStringLiteral("Settings/upgrade_mode"), upgradeMode->currentText());
    settings->setValue(QStringLiteral("Settings/helper"), helper->text().trimmed());
    settings->sync();
    accept();
}