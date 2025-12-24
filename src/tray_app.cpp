#include "tray_app.h"
#include "common.h"
#include <QDebug>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDBusReply>

TrayApp::TrayApp(QApplication* app)
    : QObject(app)
    , app(app)
    , settings(new QSettings(APP_ORG, APP_NAME, this))
    , tray(new QSystemTrayIcon(this))
    , menu(new QMenu())
    , iface(nullptr)
    , pollTimer(new QTimer(this))
    , uiUpdateTimer(new QTimer(this))
    , trayService(nullptr)
    , notifiedAvailable(false)
{
    setupActions();
    setupDBus();
    registerTrayService();
    updateUI();
    tray->show();

    // Update UI periodically to pick up settings changes
    connect(uiUpdateTimer, &QTimer::timeout, this, &TrayApp::updateUI);
    uiUpdateTimer->start(30 * 1000); // Update every 30 seconds
}

TrayApp::~TrayApp() {
    delete menu;
    if (trayService) {
        delete trayService;
    }
}

void TrayApp::setupActions() {
    actionView = new QAction(QStringLiteral("View && Upgrade"), menu);
    connect(actionView, &QAction::triggered, this, &TrayApp::openView);

    actionRefresh = new QAction(QStringLiteral("Refresh"), menu);
    connect(actionRefresh, &QAction::triggered, this, &TrayApp::refresh);

    actionSettings = new QAction(QStringLiteral("Settings"), menu);
    connect(actionSettings, &QAction::triggered, this, &TrayApp::openSettings);

    actionQuit = new QAction(QStringLiteral("Quit"), menu);
    connect(actionQuit, &QAction::triggered, app, &QApplication::quit);

    menu->addAction(actionView);
    menu->addAction(actionRefresh);
    menu->addAction(actionSettings);
    menu->addSeparator();
    menu->addAction(actionQuit);

    tray->setContextMenu(menu);
    connect(tray, &QSystemTrayIcon::activated, this, &TrayApp::onActivated);
}

void TrayApp::setupDBus() {
    iface = new QDBusInterface(
        QStringLiteral("org.mxlinux.UpdaterSystemMonitor"),
        QStringLiteral("/org/mxlinux/UpdaterSystemMonitor"),
        QStringLiteral("org.mxlinux.UpdaterSystemMonitor"),
        QDBusConnection::systemBus(),
        this
    );

    connect(pollTimer, &QTimer::timeout, this, &TrayApp::pollState);
    pollTimer->start(60 * 1000); // Poll every minute

    refresh();
}

void TrayApp::registerTrayService() {
    // TODO: Implement tray service
}

void TrayApp::refresh() {
    if (iface && iface->isValid()) {
        iface->call(QStringLiteral("Refresh"));
    } else {
        updateUI();
    }
}

void TrayApp::pollState() {
    if (!iface || !iface->isValid()) {
        return;
    }

    QDBusReply<QString> reply = iface->call(QStringLiteral("GetState"));
    if (reply.isValid()) {
        onStateChanged(reply.value());
    }
}

void TrayApp::onStateChanged(const QString& payload) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        return;
    }

    state = doc.object();
    updateUI();
}

void TrayApp::updateUI() {
    settings->sync();

    QJsonObject counts = state[QStringLiteral("counts")].toObject();
    int upgrades = counts[QStringLiteral("upgrade")].toInt();
    bool available = upgrades > 0;

    QString iconPath = this->iconPath(available);
    if (QFile::exists(iconPath)) {
        tray->setIcon(QIcon(iconPath));
    }

    QString tooltip = QString(QStringLiteral("Upgrades: %1\nNew: %2\nRemove: %3\nHeld: %4"))
        .arg(counts[QStringLiteral("upgrade")].toInt())
        .arg(counts[QStringLiteral("new")].toInt())
        .arg(counts[QStringLiteral("remove")].toInt())
        .arg(counts[QStringLiteral("held")].toInt());
    tray->setToolTip(tooltip);

    bool autohide = readSetting(QStringLiteral("Settings/auto_hide"), false).toBool();
    if (autohide && !available) {
        tray->hide();
    } else {
        tray->show();
    }

    bool notify = readSetting(QStringLiteral("Settings/notify"), true).toBool();
    if (notify && available && !notifiedAvailable) {
        tray->showMessage(QStringLiteral("Updates Available"), tooltip, tray->icon());
        notifiedAvailable = true;
    }
    if (!available) {
        notifiedAvailable = false;
    }
}

QString TrayApp::iconPath(bool available) const {
    QString theme = readSetting(QStringLiteral("Settings/icon_theme"), QStringLiteral("wireframe-dark")).toString();
    if (!ICON_THEMES.contains(theme)) {
        theme = QStringLiteral("wireframe-dark");
    }
    QString name = available ? QStringLiteral("updates-available.svg") : QStringLiteral("up-to-date.svg");
    return ::iconPath(theme, name);
}

void TrayApp::onActivated(QSystemTrayIcon::ActivationReason reason) {
    qDebug() << "Tray activated with reason:" << reason;
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::Unknown) {
        qDebug() << "Treating activation as Trigger.";
        openView();
    } else if (reason == QSystemTrayIcon::MiddleClick) {
        launchHelper();
    }
}

void TrayApp::openView() {
    launchBin(QStringLiteral("updater-view-and-upgrade"));
}

void TrayApp::openSettings() {
    launchBin(QStringLiteral("updater-settings"));
}

void TrayApp::launchHelper() {
    QString helper = readSetting(QStringLiteral("Settings/helper"), QStringLiteral("mx-packageinstaller")).toString();
    if (helper.isEmpty()) {
        helper = QStringLiteral("mx-packageinstaller");
    }

    QProcess::startDetached(helper, QStringList());
}

void TrayApp::launchBin(const QString& name) {
    QString appPath = QCoreApplication::applicationDirPath();
    QString path = appPath + QStringLiteral("/") + name;
    qDebug() << "Attempting to launch:" << path;
    if (QFile::exists(path)) {
        QProcess::startDetached(path, QStringList());
    } else {
        qWarning() << "Binary not found at:" << path;
        // Fallback to just the name, in case it's in the system PATH
        qDebug() << "Attempting to launch from PATH:" << name;
        QProcess::startDetached(name, QStringList());
    }
}

void TrayApp::quit() {
    app->quit();
}