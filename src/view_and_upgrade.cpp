#include "view_and_upgrade.h"
#include "common.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDBusReply>
#include <QMessageBox>
#include <QApplication>

ViewAndUpgrade::ViewAndUpgrade(QWidget* parent)
    : QDialog(parent)
    , countsLabel(new QLabel(this))
    , listWidget(new QListWidget(this))
    , buttonRefresh(new QPushButton(QStringLiteral("Refresh"), this))
    , buttonUpgrade(new QPushButton(QStringLiteral("Upgrade"), this))
    , buttonClose(new QPushButton(QStringLiteral("Close"), this))
    , iface(nullptr)
    , progressDialog(nullptr)
    , upgradeProcess(nullptr)
{
    setWindowTitle(QStringLiteral("MX Arch Updater"));
    resize(680, 420);

    ensureNotRoot();
    buildUi();
    setupDBus();
    refresh();
}

ViewAndUpgrade::~ViewAndUpgrade() {
    if (upgradeProcess) {
        upgradeProcess->kill();
        upgradeProcess->waitForFinished(3000);
    }
}

void ViewAndUpgrade::buildUi() {
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(buttonRefresh, &QPushButton::clicked, this, &ViewAndUpgrade::refresh);
    connect(buttonUpgrade, &QPushButton::clicked, this, &ViewAndUpgrade::upgrade);
    connect(buttonClose, &QPushButton::clicked, this, &ViewAndUpgrade::close);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(buttonRefresh);
    buttonLayout->addWidget(buttonUpgrade);
    buttonLayout->addWidget(buttonClose);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(countsLabel);
    mainLayout->addWidget(listWidget);
    mainLayout->addLayout(buttonLayout);
}

void ViewAndUpgrade::setupDBus() {
    iface = new QDBusInterface(
        QStringLiteral("org.mxlinux.UpdaterSystemMonitor"),
        QStringLiteral("/org/mxlinux/UpdaterSystemMonitor"),
        QStringLiteral("org.mxlinux.UpdaterSystemMonitor"),
        QDBusConnection::systemBus(),
        this
    );
}

void ViewAndUpgrade::refresh() {
    if (!iface || !iface->isValid()) {
        countsLabel->setText(QStringLiteral("System monitor is not available."));
        return;
    }

    QDBusReply<QString> reply = iface->call(QStringLiteral("GetState"));
    if (!reply.isValid()) {
        countsLabel->setText(QStringLiteral("Unable to query system monitor."));
        return;
    }

    QString payload = reply.value();
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        countsLabel->setText(QStringLiteral("Received invalid state from monitor."));
        return;
    }

    QJsonObject state = doc.object();
    QJsonObject counts = state[QStringLiteral("counts")].toObject();

    QString countsText = QString(QStringLiteral("Upgrades: %1 | New: %2 | Remove: %3 | Held: %4"))
        .arg(counts[QStringLiteral("upgrade")].toInt())
        .arg(counts[QStringLiteral("new")].toInt())
        .arg(counts[QStringLiteral("remove")].toInt())
        .arg(counts[QStringLiteral("held")].toInt());
    countsLabel->setText(countsText);

    listWidget->clear();
    QJsonArray packages = state[QStringLiteral("packages")].toArray();
    for (const QJsonValue& value : packages) {
        listWidget->addItem(value.toString());
    }
}

void ViewAndUpgrade::upgrade() {
    QString upgradeMode = readSetting(QStringLiteral("Settings/upgrade_mode"), QStringLiteral("basic")).toString();
    QString scriptPath = helperPath(QStringLiteral("updater_upgrade"));

    if (!QFile::exists(scriptPath)) {
        QMessageBox::warning(this, QStringLiteral("Upgrade"), QStringLiteral("Upgrade helper script not found."));
        return;
    }

    QStringList command;
    command << QStringLiteral("pkexec") << scriptPath << QStringLiteral("--mode") << upgradeMode;

    progressDialog = new QProgressDialog(QStringLiteral("Upgrading packages..."), QString(), 0, 0, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setCancelButton(nullptr);
    progressDialog->show();

    upgradeProcess = new QProcess(this);
    connect(upgradeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ViewAndUpgrade::onUpgradeFinished);

    upgradeProcess->start(command.first(), command.mid(1));
}

void ViewAndUpgrade::onUpgradeFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    if (progressDialog) {
        progressDialog->close();
        delete progressDialog;
        progressDialog = nullptr;
    }

    if (upgradeProcess) {
        delete upgradeProcess;
        upgradeProcess = nullptr;
    }

    refresh();
}