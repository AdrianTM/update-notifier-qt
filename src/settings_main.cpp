#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnectionInterface>
#include "settings_dialog.h"
#include "settings_service.h"
#include "common.h"

const QString SETTINGS_SERVICE_NAME = QStringLiteral("org.mxlinux.UpdaterSettings");
const QString SETTINGS_OBJECT_PATH = QStringLiteral("/org/mxlinux/UpdaterSettings");
const QString SETTINGS_INTERFACE = QStringLiteral("org.mxlinux.UpdaterSettings");

int main(int argc, char *argv[]) {
    ensureNotRoot();

    QApplication app(argc, argv);
    SettingsService service;
    SettingsDialog dialog(&service);

    // Register D-Bus service
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected()) {
        // Check if service is already registered (we were auto-activated)
        bool alreadyRegistered = sessionBus.interface()->isServiceRegistered(SETTINGS_SERVICE_NAME);

        if (!alreadyRegistered) {
            // Register the service since it's not already registered
            sessionBus.registerService(SETTINGS_SERVICE_NAME);
            sessionBus.registerObject(
                SETTINGS_OBJECT_PATH,
                SETTINGS_INTERFACE,
                &service,
                QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
            );
        } else {
            // Service already exists, we were auto-activated
            // Just provide the service interface without showing dialog
            return app.exec();
        }
    }

    dialog.exec();
    return 0;
}