#include "settings_service.h"
#include "settings_dialog.h"

SettingsService::SettingsService(SettingsDialog* dialog)
    : QObject(dialog)
    , dialog(dialog)
{
}

QString SettingsService::Get(const QString& key) {
    return dialog->settings->value(key, QStringLiteral("")).toString();
}

void SettingsService::Set(const QString& key, const QString& value) {
    dialog->settings->setValue(key, value);
    dialog->settings->sync();
    emit settingsChanged(key, value);
}