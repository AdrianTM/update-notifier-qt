#pragma once

#include <QObject>
#include <QDBusConnection>
#include <QSettings>

class SettingsDialog;

class SettingsService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mxlinux.UpdaterSettings")

public:
    explicit SettingsService(SettingsDialog* dialog = nullptr);

public Q_SLOTS:
    QString Get(const QString& key);
    void Set(const QString& key, const QString& value);

Q_SIGNALS:
    void settingsChanged(const QString& key, const QString& value);

private:
    QSettings* settings;
};
