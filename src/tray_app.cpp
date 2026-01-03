#include "tray_app.h"
#include "common.h"
#include "history_dialog.h"
#include "settings_dialog.h"
#include "settings_service.h"
#include "tray_service.h"
#include <QDBusConnection>
#include <QDBusReply>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>

TrayApp::TrayApp(QApplication *app)
    : QObject(app), app(app), settings(new QSettings(APP_ORG, APP_NAME, this)),
      tray(new QSystemTrayIcon(this)), menu(new QMenu()), actionView(nullptr),
      actionPackageInstaller(nullptr), actionRefresh(nullptr),
      actionHistory(nullptr), actionPreferences(nullptr), actionAbout(nullptr),
      actionQuit(nullptr), iface(nullptr), settingsIface(nullptr),
      trayIface(nullptr), pollTimer(new QTimer(this)), trayService(nullptr),
      settingsService(nullptr), progressDialog(nullptr), upgradeProcess(nullptr),
      upgradeDialog(nullptr), upgradeOutput(nullptr), upgradeButtons(nullptr),
      updateWindow(nullptr), settingsDialog(nullptr), historyDialog(nullptr),
      upgradesCount(0), repoCount(0), aurCount(0), removeCount(0), heldCount(0),
      notifiedAvailable(false), initializationComplete(false) {
  // Auto-enable the tray service if not already enabled
  autoEnableTrayService();

  setupActions();
  setupDBus();
  registerTrayService();
  registerSettingsService();
  updateUI();
  tray->show();

  qDebug() << "TrayApp initialization complete";

  // Now connect the activated signal after all initialization is complete
  connect(tray, &QSystemTrayIcon::activated, this, &TrayApp::onActivated);
  initializationComplete = true;
  qDebug() << "Initialization complete, activation signal connected";
}

TrayApp::~TrayApp() {
  delete menu;
  if (trayService) {
    delete trayService;
  }
  if (settingsService) {
    delete settingsService;
  }
}

void TrayApp::setupActions() {
  actionView = new QAction(QStringLiteral("&View and Upgrade"), menu);
  connect(actionView, &QAction::triggered, this, &TrayApp::openView);

  // Create Package Manager action if configured executable exists
  QString packageManager =
      readSetting(QStringLiteral("Settings/package_manager"),
                  QStringLiteral("mx-packageinstaller"))
          .toString();
  if (!packageManager.isEmpty() && isPackageInstalled(packageManager)) {
    QString displayName = getDesktopFileName(packageManager);
    actionPackageInstaller = new QAction(displayName, menu);
    connect(actionPackageInstaller, &QAction::triggered, this,
            &TrayApp::launchPackageInstaller);
  } else {
    actionPackageInstaller = nullptr;
  }

  actionRefresh = new QAction(QStringLiteral("&Check for Updates"), menu);
  connect(actionRefresh, &QAction::triggered, this, &TrayApp::refresh);

  actionHistory = new QAction(QStringLiteral("&History"), menu);
  connect(actionHistory, &QAction::triggered, this, &TrayApp::openHistory);

  actionPreferences = new QAction(QStringLiteral("&Preferences"), menu);
  connect(actionPreferences, &QAction::triggered, this, &TrayApp::openSettings);

  actionAbout = new QAction(QStringLiteral("&About"), menu);
  connect(actionAbout, &QAction::triggered, this, &TrayApp::openAbout);

  actionQuit = new QAction(QStringLiteral("&Quit"), menu);
  connect(actionQuit, &QAction::triggered, app, &QApplication::quit);

  menu->addAction(actionView);
  if (actionPackageInstaller) {
    menu->addAction(actionPackageInstaller);
  }
  menu->addAction(actionRefresh);
  menu->addAction(actionHistory);
  menu->addAction(actionPreferences);
  menu->addAction(actionAbout);
  menu->addSeparator();
  menu->addAction(actionQuit);

  tray->setContextMenu(menu);

  // Update UI and package manager action when menu is shown
  connect(menu, &QMenu::aboutToShow, this, [this]() {
    updateUI();
    updatePackageManagerAction();
  });
}

void TrayApp::setupDBus() {
  iface =
      new QDBusInterface(QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
                         QStringLiteral("/org/mxlinux/UpdaterSystemMonitor"),
                         QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
                         QDBusConnection::systemBus(), this);

  settingsIface =
      new QDBusInterface(QStringLiteral("org.mxlinux.UpdateNotifierSettings"),
                         QStringLiteral("/org/mxlinux/UpdaterSettings"),
                         QStringLiteral("org.mxlinux.UpdateNotifierSettings"),
                         QDBusConnection::sessionBus(), this);

  if (settingsIface->isValid()) {
    connect(settingsIface, SIGNAL(settingsChanged(QString, QString)), this,
            SLOT(onSettingsChanged(QString, QString)));
  }

  // Connect to D-Bus signal for state changes (primary method)
  if (iface && iface->isValid()) {
    connect(iface, SIGNAL(summaryChanged(QString)), this,
            SLOT(onSummaryChanged(QString)));
  }

  // Keep polling as fallback (every 15 minutes) in case signals are missed
  connect(pollTimer, &QTimer::timeout, this, &TrayApp::pollState);
  pollTimer->start(15 * 60 * 1000); // Poll every 15 minutes as fallback

  refresh();
}

void TrayApp::registerTrayService() {
  const QString TRAY_SERVICE_NAME =
      QStringLiteral("org.mxlinux.UpdateNotifierTrayIcon");
  const QString TRAY_OBJECT_PATH =
      QStringLiteral("/org/mxlinux/UpdaterSystemTrayIcon");
  const QString TRAY_INTERFACE =
      QStringLiteral("org.mxlinux.UpdateNotifierTrayIcon");

  QDBusConnection sessionBus = QDBusConnection::sessionBus();
  if (!sessionBus.isConnected()) {
    qWarning()
        << "Could not connect to session bus for tray service registration";
    return;
  }

  if (!sessionBus.registerService(TRAY_SERVICE_NAME)) {
    qWarning() << "Could not register tray service name:"
               << sessionBus.lastError().message();
    return;
  }

  trayService = new TrayService(this);
  if (!sessionBus.registerObject(TRAY_OBJECT_PATH, TRAY_INTERFACE, trayService,
                                 QDBusConnection::ExportAllSlots |
                                     QDBusConnection::ExportAllSignals)) {
    qWarning() << "Could not register tray service object:"
               << sessionBus.lastError().message();
    delete trayService;
    trayService = nullptr;
  }
}

void TrayApp::registerSettingsService() {
  const QString SETTINGS_SERVICE_NAME =
      QStringLiteral("org.mxlinux.UpdateNotifierSettings");
  const QString SETTINGS_OBJECT_PATH =
      QStringLiteral("/org/mxlinux/UpdaterSettings");
  const QString SETTINGS_INTERFACE =
      QStringLiteral("org.mxlinux.UpdateNotifierSettings");

  // Always create the service instance - it can propagate settings to SystemMonitor
  // via direct D-Bus calls even if we can't register it on the bus
  settingsService = new SettingsService();

  QDBusConnection sessionBus = QDBusConnection::sessionBus();
  if (!sessionBus.isConnected()) {
    qWarning()
        << "Could not connect to session bus for settings service registration";
    qWarning() << "Settings will still work but won't be accessible via D-Bus";
    return;
  }

  if (!sessionBus.registerService(SETTINGS_SERVICE_NAME)) {
    qWarning() << "Could not register settings service name:"
               << sessionBus.lastError().message();
    qWarning() << "Settings will still work but won't be accessible via D-Bus";
    return;
  }

  if (!sessionBus.registerObject(SETTINGS_OBJECT_PATH, SETTINGS_INTERFACE,
                                 settingsService,
                                 QDBusConnection::ExportAllSlots |
                                     QDBusConnection::ExportAllSignals)) {
    qWarning() << "Could not register settings service object:"
               << sessionBus.lastError().message();
    qWarning() << "Settings will still work but won't be accessible via D-Bus";
  }
}

void TrayApp::refresh() {
  if (iface && iface->isValid()) {
    iface->call(QStringLiteral("Refresh"));
    // Immediately poll the state after triggering refresh
    pollState();
  } else {
    updateUI();
  }
}

void TrayApp::pollState() {
  if (!iface || !iface->isValid()) {
    return;
  }

  QDBusReply<QString> reply = iface->call(QStringLiteral("GetStateSummary"));
  if (reply.isValid()) {
    onSummaryChanged(reply.value());
  }
}

void TrayApp::onSummaryChanged(const QString &payload) {
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);

  if (error.error != QJsonParseError::NoError) {
    return;
  }

  QJsonObject counts = doc.object()[QStringLiteral("counts")].toObject();
  upgradesCount =
      counts[QStringLiteral("total_upgrade")].toInt(); // Use total including AUR
  repoCount = counts[QStringLiteral("upgrade")].toInt();
  aurCount = counts[QStringLiteral("aur_upgrade")].toInt();
  removeCount = counts[QStringLiteral("remove")].toInt();
  heldCount = counts[QStringLiteral("held")].toInt();
  updateUI();
}

void TrayApp::updateUI() {
  settings->sync();

  bool available = upgradesCount > 0;

  // Use cached icons for better performance
  loadIconsIfNeeded();
  tray->setIcon(available ? iconAvailable : iconUpToDate);

  QString tooltip =
      QString(QStringLiteral("Upgrades: %1 total (%2 repo + %3 AUR)\nRemove: %4\nHeld: %5"))
          .arg(upgradesCount)
          .arg(repoCount)
          .arg(aurCount)
          .arg(removeCount)
          .arg(heldCount);
  tray->setToolTip(tooltip);

  bool autohide =
      readBoolSetting(QStringLiteral("Settings/auto_hide"), false);
  tray->setVisible(!(autohide && !available));

  bool notify = readBoolSetting(QStringLiteral("Settings/notify"), true);
  if (notify && available && !notifiedAvailable) {
    tray->showMessage(QStringLiteral("Updates Available"), tooltip,
                      tray->icon());
    notifiedAvailable = true;
  }
  if (!available) {
    notifiedAvailable = false;
  }
}

void TrayApp::loadIconsIfNeeded() {
  QString theme = readSetting(QStringLiteral("Settings/icon_theme"),
                              QStringLiteral("modern-light"))
                      .toString();
  if (!isKnownIconTheme(theme)) {
    theme = QStringLiteral("modern-light");
  }

  // Only reload icons if theme changed
  if (theme != cachedTheme) {
    QString availablePath = ::iconPath(theme, QStringLiteral("updates-available.svg"));
    QString upToDatePath = ::iconPath(theme, QStringLiteral("up-to-date.svg"));

    iconAvailable = QIcon(availablePath);
    iconUpToDate = QIcon(upToDatePath);
    cachedTheme = theme;
  }
}

void TrayApp::onSettingsChanged(const QString &key, const QString &value) {
  // Settings are now updated when menu is shown, so this method is kept for
  // compatibility but the actual updates happen in the menu aboutToShow handler
}



void TrayApp::onActivated(QSystemTrayIcon::ActivationReason reason) {
  qDebug() << "Tray activated with reason:" << reason << "at"
           << QDateTime::currentDateTime().toString();

  // Ignore activations during initialization
  if (!initializationComplete) {
    qDebug() << "Ignoring activation - initialization not complete";
    return;
  }

  // Temporarily disconnect the signal to prevent duplicate rapid-fire activations
  // (some desktop environments send multiple signals for a single click)
  disconnect(tray, &QSystemTrayIcon::activated, this, &TrayApp::onActivated);

  if (reason == QSystemTrayIcon::Trigger ||
      reason == QSystemTrayIcon::Unknown) {
    qDebug() << "Treating activation as Trigger - calling openView()";
    openView();
  } else if (reason == QSystemTrayIcon::MiddleClick) {
    qDebug() << "Middle click detected - launching package installer";
    launchPackageInstaller();
  } else {
    qDebug() << "Unhandled activation reason:" << reason;
  }

  // Reconnect the signal after a short delay to allow duplicate signals to be ignored
  QTimer::singleShot(100, this, [this]() {
    connect(tray, &QSystemTrayIcon::activated, this, &TrayApp::onActivated);
  });
}

void TrayApp::openView() {
  static int callCount = 0;
  callCount++;
  qDebug() << "TrayApp::openView() called #" << callCount << " at"
           << QDateTime::currentDateTime().toString();
  qDebug() << "Launching view-and-upgrade application";
  launchBin(QStringLiteral("update-notifier-view-and-upgrade"));
}

void TrayApp::openSettings() {
  if (!settingsDialog) {
    settingsDialog = new SettingsDialog(settingsService, nullptr);
    settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(settingsDialog, &QDialog::finished, this, [this]() {
      updateUI(); // Refresh UI when settings are changed
      settingsDialog = nullptr;
    });
  }
  settingsDialog->show();
  settingsDialog->raise();
  settingsDialog->activateWindow();
}

void TrayApp::openHistory() {
  if (!historyDialog) {
    historyDialog = new HistoryDialog(nullptr);
    historyDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(historyDialog, &QDialog::finished, this,
            [this]() { historyDialog = nullptr; });
  }
  historyDialog->show();
  historyDialog->raise();
  historyDialog->activateWindow();
}

void TrayApp::openAbout() {
  QMessageBox messageBox(nullptr);
  messageBox.setWindowTitle(QStringLiteral("About Update Notifier Qt"));
  messageBox.setIcon(QMessageBox::Information);
  messageBox.setTextFormat(Qt::PlainText);
  messageBox.setText(QStringLiteral(
                         "Update Notifier Qt\n"
                         "Version %1\n"
                         "A system tray application for managing Arch Linux updates.\n"
                         "Copyright © 2026 MX Linux\n"
                         "Licensed under GPL")
                         .arg(APP_VERSION));
  messageBox.exec();
}

void TrayApp::updatePackageManagerAction() {
  // Remove existing package installer action from menu if it exists
  if (actionPackageInstaller) {
    menu->removeAction(actionPackageInstaller);
    delete actionPackageInstaller;
    actionPackageInstaller = nullptr;
  }

  // Create new Package Manager action if configured executable exists
  QString packageManager =
      readSetting(QStringLiteral("Settings/package_manager"),
                  QStringLiteral("mx-packageinstaller"))
          .toString();
  if (!packageManager.isEmpty() && isPackageInstalled(packageManager)) {
    QString displayName = getDesktopFileName(packageManager);
    actionPackageInstaller = new QAction(displayName, menu);
    connect(actionPackageInstaller, &QAction::triggered, this,
            &TrayApp::launchPackageInstaller);

    // Insert the action after View and Upgrade, before Check for Updates
    QList<QAction *> actions = menu->actions();
    int insertIndex = actions.indexOf(actionView) + 1;
    if (insertIndex < actions.size()) {
      menu->insertAction(actions[insertIndex], actionPackageInstaller);
    } else {
      menu->addAction(actionPackageInstaller);
    }
  }
}

void TrayApp::launchPackageInstaller() {
  QString packageManager =
      readSetting(QStringLiteral("Settings/package_manager"),
                  QStringLiteral("mx-packageinstaller"))
          .toString();
  if (!packageManager.isEmpty()) {
    QProcess::startDetached(packageManager, QStringList());
  }
}

void TrayApp::launchBin(const QString &name) {
  QString appPath = QCoreApplication::applicationDirPath();
  QString path = appPath + QStringLiteral("/") + name;
  if (QFile::exists(path)) {
    QProcess::startDetached(path, QStringList());
  } else {
    qWarning() << "Binary not found at:" << path;
    // Fallback to just the name, in case it's in the system PATH
    QProcess::startDetached(name, QStringList());
  }
}

void TrayApp::quit() { app->quit(); }

bool TrayApp::isPackageInstalled(const QString &packageName) const {
  QProcess process;
  process.start(QStringLiteral("pacman"),
                QStringList() << QStringLiteral("-Q") << packageName);
  process.waitForFinished(5000);
  return process.exitCode() == 0;
}

void TrayApp::autoEnableTrayService() {
  // Check if the tray service is already enabled
  QProcess checkProcess;
  checkProcess.start(
      QStringLiteral("systemctl"),
      QStringList() << QStringLiteral("--user") << QStringLiteral("is-enabled")
                    << QStringLiteral("update-notifier-tray.service"));
  checkProcess.waitForFinished(2000);

  if (checkProcess.exitCode() != 0) {
    // Service is not enabled, try to enable it
    qDebug() << "Tray service not enabled, attempting to enable it";
    QProcess enableProcess;
    enableProcess.start(
        QStringLiteral("systemctl"),
        QStringList() << QStringLiteral("--user") << QStringLiteral("enable")
                      << QStringLiteral("update-notifier-tray.service"));
    if (enableProcess.waitForFinished(5000)) {
      if (enableProcess.exitCode() == 0) {
        qDebug() << "Tray service enabled successfully";
      } else {
        qWarning() << "Failed to enable tray service:"
                   << enableProcess.readAllStandardError();
      }
    } else {
      qWarning() << "Timeout enabling tray service";
    }
  } else {
    qDebug() << "Tray service already enabled";
  }
}
