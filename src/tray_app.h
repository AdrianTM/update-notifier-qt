#pragma once

#include <QAction>
#include <QApplication>
#include <QDBusInterface>
#include <QElapsedTimer>
#include <QMenu>
#include <QObject>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWidget>

class TrayService;
class SettingsService;
class SettingsDialog;
class HistoryDialog;

class TrayApp : public QObject {
  Q_OBJECT

public:
  explicit TrayApp(QApplication *app);
  ~TrayApp();

public Q_SLOTS:
  void openView();
  void openHistory();
  void openAbout();
  void openSettings();
  void refresh();
  void launchPackageInstaller();
  void quit();

private Q_SLOTS:
  void setupActions();
  void setupDBus();
  void registerTrayService();
  void registerSettingsService();
  void pollState();
  void onSummaryChanged(const QString &payload);
  void onActivated(QSystemTrayIcon::ActivationReason reason);
  void onSettingsChanged(const QString &key, const QString &value);
  void onSystemMonitorServiceChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
  void updateUI();

private:
  void launchBin(const QString &name);
  void updatePackageManagerAction();
  bool isPackageInstalled(const QString &packageName) const;
  void autoEnableTrayService();
  void loadIconsIfNeeded();

  QApplication *app;
  QSettings *settings;
  QSystemTrayIcon *tray;
  QMenu *menu;
  QAction *actionView;
  QAction *actionPackageInstaller;
  QAction *actionRefresh;
  QAction *actionHistory;
  QAction *actionPreferences;
  QAction *actionAbout;
  QAction *actionQuit;

  QDBusInterface *iface;
  QDBusInterface *settingsIface;
  QTimer *pollTimer;
  TrayService *trayService;
  SettingsService *settingsService;

  // Embedded dialogs
  SettingsDialog *settingsDialog;
  HistoryDialog *historyDialog;

  int upgradesCount;
  int repoCount;
  int aurCount;
  int removeCount;
  int heldCount;
  bool notifiedAvailable;
  bool initializationComplete;

  // Icon cache
  QIcon iconAvailable;
  QIcon iconUpToDate;
  QString cachedTheme;

  // Hidden widget to keep app alive when tray is hidden (autohide feature)
  QWidget *keepAliveWidget;
};
