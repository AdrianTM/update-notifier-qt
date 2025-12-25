#include "settings_dialog.h"
#include "common.h"
#include "settings_service.h"

SettingsDialog::SettingsDialog(SettingsService *service, QWidget *parent)
    : QDialog(parent), settings(new QSettings(APP_ORG, APP_NAME, this)),
      service(service) {
  setWindowTitle(QStringLiteral("MX Arch Updater Settings"));
  QString iconPath =
      ::iconPath(QStringLiteral(""), QStringLiteral("mx-updater-settings.svg"));
  if (QFile::exists(iconPath)) {
    setWindowIcon(QIcon(iconPath));
  }
  resize(480, 300);
  buildUi();
  load();
}

void SettingsDialog::buildUi() {
  // Icon theme selection with list widget
  QLabel *themeLabel = new QLabel(QStringLiteral("Icon theme:"), this);
  iconThemeList = new QListWidget(this);
  iconThemeList->setMaximumHeight(120);
  iconThemeList->setSelectionMode(QAbstractItemView::SingleSelection);

  // Add themes to the list
  for (const QString &theme : ICON_THEMES) {
    QListWidgetItem *item = new QListWidgetItem(theme, iconThemeList);
    item->setData(Qt::UserRole, theme);
  }

  // Create combined layout for theme list and preview
  QHBoxLayout *themeAndPreviewLayout = new QHBoxLayout();
  themeAndPreviewLayout->addWidget(iconThemeList);

  // Preview icons in a vertical layout with spacing
  QVBoxLayout *previewLayout = new QVBoxLayout();
  previewLayout->setSpacing(10);
  previewLayout->addStretch();

  // No updates preview
  QHBoxLayout *upToDateRow = new QHBoxLayout();
  upToDateRow->setSpacing(8);
  previewUpToDate = new QLabel(this);
  previewUpToDate->setFixedSize(24, 24);
  previewUpToDate->setScaledContents(true);
  QLabel *upToDateLabel = new QLabel(QStringLiteral("No updates"), this);
  upToDateRow->addWidget(previewUpToDate);
  upToDateRow->addWidget(upToDateLabel);
  upToDateRow->addStretch();

  // Updates available preview
  QHBoxLayout *updatesRow = new QHBoxLayout();
  updatesRow->setSpacing(8);
  previewUpdatesAvailable = new QLabel(this);
  previewUpdatesAvailable->setFixedSize(24, 24);
  previewUpdatesAvailable->setScaledContents(true);
  QLabel *updatesLabel = new QLabel(QStringLiteral("Updates available"), this);
  updatesRow->addWidget(previewUpdatesAvailable);
  updatesRow->addWidget(updatesLabel);
  updatesRow->addStretch();

  previewLayout->addLayout(upToDateRow);
  previewLayout->addLayout(updatesRow);
  previewLayout->addStretch();

  themeAndPreviewLayout->addLayout(previewLayout);

  // Connect theme selection to preview update
  connect(iconThemeList, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *current, QListWidgetItem *previous) {
            if (current) {
              QString theme = current->data(Qt::UserRole).toString();
              updateIconPreviews(theme);
            }
          });

  autoHide =
      new QCheckBox(QStringLiteral("Hide tray icon when no updates"), this);
  notify =
      new QCheckBox(QStringLiteral("Notify when updates are available"), this);
  startLogin = new QCheckBox(QStringLiteral("Start at login"), this);

  // Check interval with value and unit
  checkIntervalValue = new QSpinBox(this);
  checkIntervalValue->setMinimum(1);
  checkIntervalValue->setMaximum(365); // Max for days
  checkIntervalValue->setToolTip(
      QStringLiteral("How often to check for updates"));

  checkIntervalUnit = new QComboBox(this);
  checkIntervalUnit->addItem(QStringLiteral("Minutes"), 60);
  checkIntervalUnit->addItem(QStringLiteral("Hours"), 3600);
  checkIntervalUnit->addItem(QStringLiteral("Days"), 86400);
  checkIntervalUnit->setToolTip(QStringLiteral("Time unit for check interval"));

  // Update spinbox range when unit changes
  connect(checkIntervalUnit,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            int multiplier = checkIntervalUnit->itemData(index).toInt();
            if (multiplier == 60) {                 // Minutes
              checkIntervalValue->setMaximum(1440); // 24 hours in minutes
            } else if (multiplier == 3600) {        // Hours
              checkIntervalValue->setMaximum(24);   // 24 hours
            } else if (multiplier == 86400) {       // Days
              checkIntervalValue->setMaximum(30);   // 30 days
            }
          });

  // Layout for interval controls
  QHBoxLayout *intervalLayout = new QHBoxLayout();
  intervalLayout->addWidget(checkIntervalValue);
  intervalLayout->addWidget(checkIntervalUnit);
  intervalLayout->addStretch();

  packageManager = new QLineEdit(this);
  packageManager->setPlaceholderText(QStringLiteral("mx-packageinstaller"));

  QFormLayout *form = new QFormLayout();
  form->addRow(themeLabel, themeAndPreviewLayout);
  form->addRow(QStringLiteral("Auto hide"), autoHide);
  form->addRow(QStringLiteral("Notifications"), notify);
  form->addRow(QStringLiteral("Start at login"), startLogin);
  form->addRow(QStringLiteral("Check interval"), intervalLayout);
  form->addRow(QStringLiteral("Package manager"), packageManager);

  QDialogButtonBox *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::save);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addStretch(1);
  layout->addWidget(buttons);
}

void SettingsDialog::load() {
  QString currentTheme = readSetting(QStringLiteral("Settings/icon_theme"),
                                     QStringLiteral("wireframe-dark"))
                             .toString();

  // Select the current theme in the list
  for (int i = 0; i < iconThemeList->count(); ++i) {
    QListWidgetItem *item = iconThemeList->item(i);
    if (item->data(Qt::UserRole).toString() == currentTheme) {
      iconThemeList->setCurrentItem(item);
      updateIconPreviews(currentTheme);
      break;
    }
  }

  autoHide->setChecked(
      readSetting(QStringLiteral("Settings/auto_hide"), false).toBool());
  notify->setChecked(
      readSetting(QStringLiteral("Settings/notify"), true).toBool());
  startLogin->setChecked(
      readSetting(QStringLiteral("Settings/start_at_login"), true).toBool());
  // Load check interval (stored in seconds, default 30 minutes)
  int intervalSeconds = readSetting(QStringLiteral("Settings/check_interval"),
                                    DEFAULT_CHECK_INTERVAL)
                            .toInt();

  // Convert to appropriate unit and value
  if (intervalSeconds >= 86400) { // >= 1 day
    checkIntervalUnit->setCurrentText(QStringLiteral("Days"));
    checkIntervalValue->setValue(intervalSeconds / 86400);
  } else if (intervalSeconds >= 3600) { // >= 1 hour
    checkIntervalUnit->setCurrentText(QStringLiteral("Hours"));
    checkIntervalValue->setValue(intervalSeconds / 3600);
  } else { // minutes
    checkIntervalUnit->setCurrentText(QStringLiteral("Minutes"));
    checkIntervalValue->setValue(intervalSeconds / 60);
  }
  packageManager->setText(
      readSetting(QStringLiteral("Settings/package_manager"),
                  QStringLiteral("mx-packageinstaller"))
          .toString());
}

void SettingsDialog::updateIconPreviews(const QString &theme) {
  QString upToDatePath = ::iconPath(theme, QStringLiteral("up-to-date.svg"));
  QString updatesPath =
      ::iconPath(theme, QStringLiteral("updates-available.svg"));

  if (QFile::exists(upToDatePath)) {
    QPixmap upToDatePixmap(upToDatePath);
    previewUpToDate->setPixmap(upToDatePixmap.scaled(
        24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    previewUpToDate->clear();
  }

  if (QFile::exists(updatesPath)) {
    QPixmap updatesPixmap(updatesPath);
    previewUpdatesAvailable->setPixmap(updatesPixmap.scaled(
        24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    previewUpdatesAvailable->clear();
  }
}

bool SettingsDialog::toBool(const QString &key, bool defaultValue) {
  QVariant value = settings->value(key, defaultValue);
  // Handle both string and boolean storage formats
  if (value.metaType().id() == QMetaType::Bool) {
    return value.toBool();
  }
  QString strValue = value.toString().toLower();
  return strValue == QStringLiteral("true") ||
         strValue == QStringLiteral("1") || strValue == QStringLiteral("yes");
}

void SettingsDialog::save() {
  QListWidgetItem *currentItem = iconThemeList->currentItem();
  if (currentItem) {
    QString theme = currentItem->data(Qt::UserRole).toString();
    writeSetting(QStringLiteral("Settings/icon_theme"), theme);
    // Notify tray app via D-Bus to reload icons
    if (service) {
      service->Set(QStringLiteral("Settings/icon_theme"), theme);
    }
  }

  writeSetting(QStringLiteral("Settings/auto_hide"), autoHide->isChecked());
  writeSetting(QStringLiteral("Settings/notify"), notify->isChecked());
  writeSetting(QStringLiteral("Settings/start_at_login"),
               startLogin->isChecked());
  // Save check interval in seconds
  int multiplier =
      checkIntervalUnit->currentData()
          .toInt(); // 60 for minutes, 3600 for hours, 86400 for days
  int intervalSeconds = checkIntervalValue->value() * multiplier;
  writeSetting(QStringLiteral("Settings/check_interval"), intervalSeconds);
  writeSetting(QStringLiteral("Settings/package_manager"),
               packageManager->text().trimmed());

  // Notify other settings changes via D-Bus if needed
  if (service) {
    service->Set(QStringLiteral("Settings/auto_hide"),
                 autoHide->isChecked() ? QStringLiteral("true")
                                       : QStringLiteral("false"));
    service->Set(QStringLiteral("Settings/package_manager"),
                 packageManager->text().trimmed());
  }

  accept();
}