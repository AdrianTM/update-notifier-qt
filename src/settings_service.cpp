#include "settings_service.h"
#include "common.h"
#include "settings_dialog.h"
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusReply>

SettingsService::SettingsService(SettingsDialog *dialog)
    : QObject(dialog), settings(new QSettings(APP_ORG, APP_NAME, this)) {}

QString SettingsService::Get(const QString &key) {
  return settings->value(key, QStringLiteral("")).toString();
}

void SettingsService::Set(const QString &key, const QString &value) {
  settings->setValue(key, value);
  settings->sync();

  // For AUR settings, also notify the system monitor directly via D-Bus
  if (key == QStringLiteral("Settings/aur_enabled") || key == QStringLiteral("Settings/aur_helper")) {
    QDBusInterface systemMonitor(QStringLiteral("org.mxlinux.UpdaterSystemMonitor"),
                                QStringLiteral("/org/mxlinux/UpdaterSystemMonitor"),
                                QStringLiteral("org.mxlinux.UpdaterSystemMonitor"),
                                QDBusConnection::systemBus());
    if (systemMonitor.isValid()) {
      systemMonitor.call(QStringLiteral("UpdateAurSetting"), key, value);
    }
  }

  emit settingsChanged(key, value);
}