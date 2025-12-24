#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnectionInterface>
#include <QDebug>
#include "settings_dialog.h"
#include "settings_service.h"
#include "common.h"

const QString SETTINGS_SERVICE_NAME = QStringLiteral("org.mxlinux.UpdaterSettings");
const QString SETTINGS_OBJECT_PATH = QStringLiteral("/org/mxlinux/UpdaterSettings");
const QString SETTINGS_INTERFACE = QStringLiteral("org.mxlinux.UpdaterSettings");

int main(int argc, char *argv[]) {
    ensureNotRoot();

    // Write debug info to a file since qDebug might not show up
    QFile debugFile(QStringLiteral("/tmp/mx-updater-settings-debug.log"));
    if (debugFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&debugFile);
        out << QDateTime::currentDateTime().toString() << QStringLiteral(" - Settings app started with ") << argc << QStringLiteral(" arguments\n");
        for (int i = 0; i < argc; ++i) {
            out << QStringLiteral("  arg ") << i << QStringLiteral(": ") << argv[i] << QStringLiteral("\n");
        }
        debugFile.close();
    }

    QApplication app(argc, argv);
    SettingsService service;
    SettingsDialog dialog(&service);

    // Register D-Bus service
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected()) {
        // Check if service is already registered (we were auto-activated)
        bool alreadyRegistered = sessionBus.interface()->isServiceRegistered(SETTINGS_SERVICE_NAME);

        QFile debugFile2(QStringLiteral("/tmp/mx-updater-settings-debug.log"));
        if (debugFile2.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&debugFile2);
            out << QStringLiteral("Settings service already registered: ") << alreadyRegistered << QStringLiteral("\n");
            debugFile2.close();
        }

        if (!alreadyRegistered) {
            QFile debugFile3(QStringLiteral("/tmp/mx-updater-settings-debug.log"));
            if (debugFile3.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&debugFile3);
                out << QStringLiteral("Registering settings service and showing dialog\n");
                debugFile3.close();
            }

            // Register the service since it's not already registered
            sessionBus.registerService(SETTINGS_SERVICE_NAME);
            sessionBus.registerObject(
                SETTINGS_OBJECT_PATH,
                SETTINGS_INTERFACE,
                &service,
                QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
            );
        } else {
            QFile debugFile4(QStringLiteral("/tmp/mx-updater-settings-debug.log"));
            if (debugFile4.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&debugFile4);
                out << QStringLiteral("Settings service already exists - we were auto-activated, exiting without dialog\n");
                debugFile4.close();
            }

            // Service already exists, we were auto-activated
            // Just provide the service interface without showing dialog
            return app.exec();
        }
    } else {
        QFile debugFile5(QStringLiteral("/tmp/mx-updater-settings-debug.log"));
        if (debugFile5.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&debugFile5);
            out << QStringLiteral("D-Bus session bus not connected\n");
            debugFile5.close();
        }
    }

    QFile debugFile6(QStringLiteral("/tmp/mx-updater-settings-debug.log"));
    if (debugFile6.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&debugFile6);
        out << QStringLiteral("Showing settings dialog\n");
        debugFile6.close();
    }

    dialog.exec();

    QFile debugFile7(QStringLiteral("/tmp/mx-updater-settings-debug.log"));
    if (debugFile7.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&debugFile7);
        out << QStringLiteral("Settings dialog closed\n");
        debugFile7.close();
    }

    return 0;
}