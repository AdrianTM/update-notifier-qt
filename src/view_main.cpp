#include <QApplication>
#include <QSystemSemaphore>
#include <QMessageBox>
#include "view_and_upgrade.h"
#include "common.h"

int main(int argc, char *argv[]) {
    ensureNotRoot();

    // Use semaphore to ensure only one instance runs
    QSystemSemaphore semaphore(QStringLiteral("mx-arch-updater-view"), 1, QSystemSemaphore::Open);
    if (!semaphore.acquire()) {
        // Another instance is already running
        QMessageBox::information(nullptr, QStringLiteral("MX Arch Updater"),
                                QStringLiteral("The update window is already open."));
        return 0;
    }

    QApplication app(argc, argv);
    ViewAndUpgrade dialog;

    // Release semaphore when app exits
    QObject::connect(&app, &QApplication::aboutToQuit, [&semaphore]() {
        semaphore.release();
    });

    // Exit app when dialog is closed
    QObject::connect(&dialog, &QDialog::finished, &app, &QApplication::quit);

    dialog.show();
    return app.exec();
}