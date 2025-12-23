#ifndef TRAY_SERVICE_H
#define TRAY_SERVICE_H

#include <QObject>

class TrayApp;

class TrayService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mxlinux.UpdaterSystemTrayIcon")

public:
    explicit TrayService(TrayApp* trayApp);

public Q_SLOTS:
    void ShowView();
    void ShowSettings();
    void Refresh();
    void Quit();

private:
    TrayApp* trayApp;
};

#endif // TRAY_SERVICE_H