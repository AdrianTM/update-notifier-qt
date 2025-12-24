#include "settings_dialog.h"
#include "settings_service.h"
#include "common.h"

SettingsDialog::SettingsDialog(SettingsService* service, QWidget* parent)
    : QDialog(parent)
    , settings(new QSettings(APP_ORG, APP_NAME, this))
    , service(service)
{
    setWindowTitle(QStringLiteral("MX Arch Updater Settings"));
    QString iconPath = ::iconPath(QStringLiteral(""), QStringLiteral("mx-updater-settings.svg"));
    if (QFile::exists(iconPath)) {
        setWindowIcon(QIcon(iconPath));
    }
    resize(480, 300);
    buildUi();
    load();
}

void SettingsDialog::buildUi() {
    // Icon theme selection with list widget
    QLabel* themeLabel = new QLabel(QStringLiteral("Icon theme:"), this);
    iconThemeList = new QListWidget(this);
    iconThemeList->setMaximumHeight(120);
    iconThemeList->setSelectionMode(QAbstractItemView::SingleSelection);

    // Add themes to the list
    for (const QString& theme : ICON_THEMES) {
        QListWidgetItem* item = new QListWidgetItem(theme, iconThemeList);
        item->setData(Qt::UserRole, theme);
    }

    // Preview labels
    QLabel* previewLabel = new QLabel(QStringLiteral("Preview:"), this);
    QHBoxLayout* previewLayout = new QHBoxLayout();
    previewUpToDate = new QLabel(this);
    previewUpToDate->setFixedSize(24, 24);
    previewUpToDate->setScaledContents(true);
    previewUpdatesAvailable = new QLabel(this);
    previewUpdatesAvailable->setFixedSize(24, 24);
    previewUpdatesAvailable->setScaledContents(true);

    QLabel* upToDateLabel = new QLabel(QStringLiteral("No updates"), this);
    QLabel* updatesLabel = new QLabel(QStringLiteral("Updates available"), this);

    previewLayout->addWidget(upToDateLabel);
    previewLayout->addWidget(previewUpToDate);
    previewLayout->addStretch();
    previewLayout->addWidget(updatesLabel);
    previewLayout->addWidget(previewUpdatesAvailable);

    // Connect theme selection to preview update
    connect(iconThemeList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* current, QListWidgetItem* previous) {
        if (current) {
            QString theme = current->data(Qt::UserRole).toString();
            updateIconPreviews(theme);
        }
    });

    autoHide = new QCheckBox(QStringLiteral("Hide tray icon when no updates"), this);
    notify = new QCheckBox(QStringLiteral("Notify when updates are available"), this);
    startLogin = new QCheckBox(QStringLiteral("Start at login"), this);

    checkInterval = new QSpinBox(this);
    checkInterval->setMinimum(5);
    checkInterval->setMaximum(1440);
    checkInterval->setSuffix(QStringLiteral(" minutes"));
    checkInterval->setToolTip(QStringLiteral("How often to check for updates (5-1440 minutes)"));

    upgradeMode = new QComboBox(this);
    upgradeMode->addItems(UPGRADE_MODES);
    helper = new QLineEdit(this);
    helper->setPlaceholderText(QStringLiteral("paru"));

    QFormLayout* form = new QFormLayout();
    form->addRow(themeLabel, iconThemeList);
    form->addRow(previewLabel, previewLayout);
    form->addRow(QStringLiteral("Auto hide"), autoHide);
    form->addRow(QStringLiteral("Notifications"), notify);
    form->addRow(QStringLiteral("Start at login"), startLogin);
    form->addRow(QStringLiteral("Check interval"), checkInterval);
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
    QString currentTheme = settings->value(QStringLiteral("Settings/icon_theme"), QStringLiteral("wireframe-dark")).toString();

    // Select the current theme in the list
    for (int i = 0; i < iconThemeList->count(); ++i) {
        QListWidgetItem* item = iconThemeList->item(i);
        if (item->data(Qt::UserRole).toString() == currentTheme) {
            iconThemeList->setCurrentItem(item);
            updateIconPreviews(currentTheme);
            break;
        }
    }

    autoHide->setChecked(toBool(QStringLiteral("Settings/auto_hide"), false));
    notify->setChecked(toBool(QStringLiteral("Settings/notify"), true));
    startLogin->setChecked(toBool(QStringLiteral("Settings/start_at_login"), true));
    // Load check interval in minutes (stored in seconds, default 30 minutes)
    int intervalSeconds = settings->value(QStringLiteral("Settings/check_interval"), DEFAULT_CHECK_INTERVAL).toInt();
    checkInterval->setValue(intervalSeconds / 60);
    upgradeMode->setCurrentText(settings->value(QStringLiteral("Settings/upgrade_mode"), QStringLiteral("basic")).toString());
    helper->setText(settings->value(QStringLiteral("Settings/helper"), QStringLiteral("paru")).toString());
}

void SettingsDialog::updateIconPreviews(const QString& theme) {
    QString upToDatePath = ::iconPath(theme, QStringLiteral("up-to-date.svg"));
    QString updatesPath = ::iconPath(theme, QStringLiteral("updates-available.svg"));

    if (QFile::exists(upToDatePath)) {
        QPixmap upToDatePixmap(upToDatePath);
        previewUpToDate->setPixmap(upToDatePixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        previewUpToDate->clear();
    }

    if (QFile::exists(updatesPath)) {
        QPixmap updatesPixmap(updatesPath);
        previewUpdatesAvailable->setPixmap(updatesPixmap.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        previewUpdatesAvailable->clear();
    }
}

bool SettingsDialog::toBool(const QString& key, bool defaultValue) {
    QString value = settings->value(key, defaultValue).toString().toLower();
    return value == QStringLiteral("true") || value == QStringLiteral("1") || value == QStringLiteral("yes");
}

void SettingsDialog::save() {
    QListWidgetItem* currentItem = iconThemeList->currentItem();
    if (currentItem) {
        if (service) {
            service->Set(QStringLiteral("Settings/icon_theme"), currentItem->data(Qt::UserRole).toString());
        } else {
            settings->setValue(QStringLiteral("Settings/icon_theme"), currentItem->data(Qt::UserRole).toString());
        }
    }
    if (service) {
        service->Set(QStringLiteral("Settings/auto_hide"), autoHide->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        service->Set(QStringLiteral("Settings/notify"), notify->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        service->Set(QStringLiteral("Settings/start_at_login"), startLogin->isChecked() ? QStringLiteral("true") : QStringLiteral("false"));
        // Save check interval in seconds
        service->Set(QStringLiteral("Settings/check_interval"), QString::number(checkInterval->value() * 60));
        service->Set(QStringLiteral("Settings/upgrade_mode"), upgradeMode->currentText());
        service->Set(QStringLiteral("Settings/helper"), helper->text().trimmed());
    } else {
        settings->setValue(QStringLiteral("Settings/auto_hide"), autoHide->isChecked());
        settings->setValue(QStringLiteral("Settings/notify"), notify->isChecked());
        settings->setValue(QStringLiteral("Settings/start_at_login"), startLogin->isChecked());
        // Save check interval in seconds
        settings->setValue(QStringLiteral("Settings/check_interval"), checkInterval->value() * 60);
        settings->setValue(QStringLiteral("Settings/upgrade_mode"), upgradeMode->currentText());
        settings->setValue(QStringLiteral("Settings/helper"), helper->text().trimmed());
        settings->sync();
    }
    accept();
}