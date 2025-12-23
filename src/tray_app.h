#ifndef TRAY_APP_H
#define TRAY_APP_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QDBusInterface>
#include <QApplication>
#include <QSettings>
#include <QJsonObject>

class TrayService;

class TrayApp : public QObject {
    Q_OBJECT

public:
    explicit TrayApp(QApplication* app);
    ~TrayApp();

public Q_SLOTS:
    void openView();
    void openSettings();
    void refresh();
    void launchHelper();
    void quit();

private Q_SLOTS:
    void setupActions();
    void setupDBus();
    void registerTrayService();
    void pollState();
    void onStateChanged(const QString& payload);
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void updateUI();

private:
    QString iconPath(bool available) const;
    void launchBin(const QString& name);

    QApplication* app;
    QSettings* settings;
    QSystemTrayIcon* tray;
    QMenu* menu;
    QAction* actionView;
    QAction* actionRefresh;
    QAction* actionSettings;
    QAction* actionQuit;

    QDBusInterface* iface;
    QTimer* pollTimer;
    TrayService* trayService;

    QJsonObject state;
    bool notifiedAvailable;
};

#endif // TRAY_APP_H