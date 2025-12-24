#include <QApplication>
#include <QCommandLineParser>
#include "settings_dialog.h"
#include "settings_service.h"
#include "common.h"

const QString SETTINGS_SERVICE_NAME = QStringLiteral("org.mxlinux.UpdaterSettings");
const QString SETTINGS_OBJECT_PATH = QStringLiteral("/org/mxlinux/UpdaterSettings");
const QString SETTINGS_INTERFACE = QStringLiteral("org.mxlinux.UpdaterSettings");

int main(int argc, char *argv[]) {
    ensureNotRoot();

    QApplication app(argc, argv);
    SettingsService service(nullptr);
    SettingsDialog dialog(&service);

    // Register D-Bus service
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected()) {
        sessionBus.registerService(SETTINGS_SERVICE_NAME);
        sessionBus.registerObject(
            SETTINGS_OBJECT_PATH,
            SETTINGS_INTERFACE,
            &service,
            QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
        );
    }

    dialog.exec();
    return 0;
}