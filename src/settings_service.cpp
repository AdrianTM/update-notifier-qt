#include "settings_service.h"
#include "common.h"
#include "settings_dialog.h"
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusReply>

SettingsService::SettingsService(SettingsDialog *dialog)
    : QObject(dialog), settings(new QSettings(APP_ORG, APP_NAME, this)) {
  // Note: This service works even if not registered on D-Bus, because Set()
  // directly calls the SystemMonitor via QDBusInterface
}

void SettingsService::initializeSystemMonitor() {
  // Read current AUR settings and sync them to the system monitor
  bool aurEnabled = settings->value(QStringLiteral("Settings/aur_enabled"), false).toBool();
  QString aurHelper = settings->value(QStringLiteral("Settings/aur_helper"), QStringLiteral("")).toString();

  // Send to system monitor via D-Bus
  QDBusInterface systemMonitor(QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
                               QStringLiteral("/org/mxlinux/UpdateNotifierSystemMonitor"),
                               QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
                               QDBusConnection::systemBus());
  if (systemMonitor.isValid()) {
    systemMonitor.call(QStringLiteral("UpdateAurSetting"),
                       QStringLiteral("Settings/aur_enabled"),
                       aurEnabled ? QStringLiteral("true") : QStringLiteral("false"));
    if (aurEnabled && !aurHelper.isEmpty()) {
      systemMonitor.call(QStringLiteral("UpdateAurSetting"),
                         QStringLiteral("Settings/aur_helper"),
                         aurHelper);
    }
  }
}

QString SettingsService::Get(const QString &key) {
  return settings->value(key, QStringLiteral("")).toString();
}

void SettingsService::Set(const QString &key, const QString &value) {
  settings->setValue(key, value);
  settings->sync();

  // For AUR settings, also notify the system monitor directly via D-Bus
  if (key == QStringLiteral("Settings/aur_enabled") || key == QStringLiteral("Settings/aur_helper")) {
    QDBusInterface systemMonitor(QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
                                QStringLiteral("/org/mxlinux/UpdaterSystemMonitor"),
                                QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
                                QDBusConnection::systemBus());
    if (systemMonitor.isValid()) {
      systemMonitor.call(QStringLiteral("UpdateAurSetting"), key, value);
    }
  }

  emit settingsChanged(key, value);
}