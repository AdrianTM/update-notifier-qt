#ifndef TRAY_APP_H
#define TRAY_APP_H

#include <QAction>
#include <QApplication>
#include <QDBusInterface>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QMenu>
#include <QObject>
#include <QProcess>
#include <QProgressDialog>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>

class TrayService;
class ViewAndUpgrade;
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
  void pollState();
  void onStateChanged(const QString &payload);
  void onActivated(QSystemTrayIcon::ActivationReason reason);
  void onSettingsChanged(const QString &key, const QString &value);
  void updateUI();

private:
  QString iconPath(bool available) const;
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
  QDBusInterface *trayIface;
  QTimer *pollTimer;
  TrayService *trayService;
  QProgressDialog *progressDialog;
  QProcess *upgradeProcess;

  // Upgrade dialog components
  QDialog *upgradeDialog;
  QTextEdit *upgradeOutput;
  QDialogButtonBox *upgradeButtons;

  // Update window singleton
  ViewAndUpgrade *updateWindow;

  // Embedded dialogs
  SettingsDialog *settingsDialog;
  HistoryDialog *historyDialog;

  QJsonObject state;
  bool notifiedAvailable;
  bool initializationComplete;

  // Icon cache
  QIcon iconAvailable;
  QIcon iconUpToDate;
  QString cachedTheme;
};

#endif // TRAY_APP_H