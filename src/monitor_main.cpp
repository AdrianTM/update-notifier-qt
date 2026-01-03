#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDebug>
#include <unistd.h>

#include "system_monitor.h"

const QString SYSTEM_SERVICE_NAME = QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor");
const QString SYSTEM_OBJECT_PATH = QStringLiteral("/org/mxlinux/UpdateNotifierSystemMonitor");
const QString SYSTEM_INTERFACE = QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor");

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Update Notifier Qt system monitor"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("debug"), QStringLiteral("Enable debug output")});
    parser.addOption({QStringLiteral("no-checksum"), QStringLiteral("Disable checksum verification for state file")});
    parser.process(app);

    if (geteuid() != 0) {
        qCritical() << QStringLiteral("ERROR: mx-updater-system-monitor must run as root.");
        return 1;
    }

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        qCritical() << QStringLiteral("ERROR: Could not connect to the system bus.");
        return 1;
    }

    if (!bus.registerService(SYSTEM_SERVICE_NAME)) {
        qCritical() << QStringLiteral("ERROR: Could not register system monitor service.");
        return 1;
    }

    SystemMonitor monitor(!parser.isSet(QStringLiteral("no-checksum")));
    bus.registerObject(
        SYSTEM_OBJECT_PATH,
        SYSTEM_INTERFACE,
        &monitor,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );

    return app.exec();
}