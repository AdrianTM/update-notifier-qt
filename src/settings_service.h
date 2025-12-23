#ifndef SETTINGS_SERVICE_H
#define SETTINGS_SERVICE_H

#include <QObject>
#include <QDBusConnection>

class SettingsDialog;

class SettingsService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mxlinux.UpdaterSettings")

public:
    explicit SettingsService(SettingsDialog* dialog);

public Q_SLOTS:
    QString Get(const QString& key);
    void Set(const QString& key, const QString& value);

Q_SIGNALS:
    void settingsChanged(const QString& key, const QString& value);

private:
    SettingsDialog* dialog;
};

#endif // SETTINGS_SERVICE_H