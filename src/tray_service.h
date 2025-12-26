#pragma once

#include <QObject>

class TrayApp;

class TrayService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mxlinux.UpdaterSystemTrayIcon")

public:
    explicit TrayService(TrayApp* trayApp);

public Q_SLOTS:
    void Quit();
    void Refresh();
    void ShowSettings();
    void ShowView();

private:
    TrayApp* trayApp;
};
