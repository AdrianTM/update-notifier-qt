#include <QApplication>
#include <QSystemSemaphore>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include "view_and_upgrade.h"
#include "common.h"

int main(int argc, char *argv[]) {
    ensureNotRoot();

    // Use a lock file to ensure only one instance
    QString lockFilePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
                          QStringLiteral("/mx-arch-updater-view.lock");

    QFile lockFile(lockFilePath);
    bool alreadyRunning = false;
    if (lockFile.exists()) {
        // Check if the process is actually running
        if (lockFile.open(QIODevice::ReadOnly)) {
            QByteArray pidData = lockFile.readAll().trimmed();
            QString pidStr = QString::fromUtf8(pidData);
            bool ok;
            qint64 pid = pidStr.toLongLong(&ok);
            if (ok && pid > 0) {
                // Check if process is still running
                QProcess checkProcess;
                checkProcess.start(QStringLiteral("kill"), QStringList() << QStringLiteral("-0") << QString::number(pid));
                if (checkProcess.waitForFinished(1000) && checkProcess.exitCode() == 0) {
                    // Process is still running
                    alreadyRunning = true;
                }
            }
            lockFile.close();
        }
    }

    QApplication app(argc, argv);

    if (alreadyRunning) {
        QMessageBox::information(nullptr, QStringLiteral("MX Arch Updater"),
                                QStringLiteral("The update window is already open."));
        return 0;
    }

    // Create lock file with our PID
    if (lockFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        lockFile.write(QByteArray::number(QApplication::applicationPid()));
        lockFile.close();
    }
    ViewAndUpgrade dialog;

    // Clean up lock file when app exits
    QObject::connect(&app, &QApplication::aboutToQuit, [lockFilePath]() {
        QFile::remove(lockFilePath);
    });

    dialog.show();
    int result = app.exec();

    // Clean up lock file
    QFile::remove(lockFilePath);

    return result;
}