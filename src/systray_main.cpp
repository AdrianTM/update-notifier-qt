#include <QApplication>
#include <QCommandLineParser>
#include "tray_app.h"
#include "common.h"

int main(int argc, char *argv[]) {
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("MX Arch Updater tray"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("autostart"), QStringLiteral("Start automatically at login")});
    QApplication app(argc, argv);
    parser.process(app);

    ensureNotRoot();

    if (parser.isSet(QStringLiteral("autostart"))) {
        bool enabled = readSetting(QStringLiteral("Settings/start_at_login"), true).toBool();
        if (!enabled) {
            return 0;
        }
    }
    app.setQuitOnLastWindowClosed(false);

    TrayApp trayApp(&app);
    return app.exec();
}
