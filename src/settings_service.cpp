#include "settings_service.h"
#include "common.h"
#include "settings_dialog.h"

SettingsService::SettingsService(SettingsDialog *dialog)
    : QObject(dialog), settings(new QSettings(APP_ORG, APP_NAME, this)) {}

QString SettingsService::Get(const QString &key) {
  return settings->value(key, QStringLiteral("")).toString();
}

void SettingsService::Set(const QString &key, const QString &value) {
  settings->setValue(key, value);
  settings->sync();
  emit settingsChanged(key, value);
}