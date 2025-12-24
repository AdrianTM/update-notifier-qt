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
    , selectAllCheckbox(new QCheckBox(QStringLiteral("Select All"), this))
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
    if (upgradeProcess && upgradeProcess->state() == QProcess::Running) {
        // Upgrade in progress - disconnect signals to prevent calling slots on destroyed object
        upgradeProcess->disconnect(this);
        // Let the upgrade complete - don't kill it as that could corrupt the system
        // The process will be cleaned up when it finishes due to deleteLater() calls
    }
}

void ViewAndUpgrade::closeEvent(QCloseEvent* event) {
    if (upgradeProcess && upgradeProcess->state() == QProcess::Running) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            QStringLiteral("Upgrade In Progress"),
            QStringLiteral("A system upgrade is currently running. Closing this window will not stop the upgrade process.\n\nAre you sure you want to close?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }

    event->accept();
}

void ViewAndUpgrade::buildUi() {
    listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    selectAllCheckbox->setChecked(true);
    connect(selectAllCheckbox, &QCheckBox::toggled, this, &ViewAndUpgrade::onSelectAllToggled);
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
    mainLayout->addWidget(selectAllCheckbox);
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
        QListWidgetItem* item = new QListWidgetItem(value.toString(), listWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        listWidget->addItem(item);
    }

    // Update Select All checkbox state
    selectAllCheckbox->setChecked(true);
}

void ViewAndUpgrade::onSelectAllToggled(bool checked) {
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem* item = listWidget->item(i);
        if (item) {
            item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void ViewAndUpgrade::upgrade() {
    // Collect selected packages
    QStringList selectedPackages;
    for (int i = 0; i < listWidget->count(); ++i) {
        QListWidgetItem* item = listWidget->item(i);
        if (item && item->checkState() == Qt::Checked) {
            // Extract package name (first word before space)
            QString packageInfo = item->text();
            QString packageName = packageInfo.split(QStringLiteral(" ")).first();
            selectedPackages.append(packageName);
        }
    }

    if (selectedPackages.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Packages Selected"),
                                QStringLiteral("Please select at least one package to upgrade."));
        return;
    }

    // Note: upgrade_mode setting is read but currently both basic/full modes use same command
    // This is kept for potential future enhancement where modes might differ
    QString upgradeMode = readSetting(QStringLiteral("Settings/upgrade_mode"), QStringLiteral("basic")).toString();
    Q_UNUSED(upgradeMode)

    QStringList command;
    command << QStringLiteral("pkexec") << QStringLiteral("pacman") << QStringLiteral("-S");
    command.append(selectedPackages);

    progressDialog = new QProgressDialog(QStringLiteral("Upgrading packages..."), QString(), 0, 0, this);
    progressDialog->setWindowModality(Qt::WindowModal);
    progressDialog->setCancelButton(nullptr);
    progressDialog->show();

    upgradeProcess = new QProcess(this);
    connect(upgradeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ViewAndUpgrade::onUpgradeFinished);
    connect(upgradeProcess, &QProcess::errorOccurred,
            this, &ViewAndUpgrade::onUpgradeError);

    upgradeProcess->start(command.first(), command.mid(1));

    if (!upgradeProcess->waitForStarted(5000)) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
            progressDialog = nullptr;
        }

        QMessageBox::critical(this, QStringLiteral("Upgrade Error"),
                            QStringLiteral("Failed to start upgrade process."));

        upgradeProcess->deleteLater();
        upgradeProcess = nullptr;
    }
}

void ViewAndUpgrade::onUpgradeFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
        progressDialog = nullptr;
    }

    if (upgradeProcess) {
        upgradeProcess->deleteLater();
        upgradeProcess = nullptr;
    }

    refresh();
}

void ViewAndUpgrade::onUpgradeError(QProcess::ProcessError error) {
    if (progressDialog) {
        progressDialog->close();
        progressDialog->deleteLater();
        progressDialog = nullptr;
    }

    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = QStringLiteral("Failed to start upgrade process. Check if pkexec is installed.");
            break;
        case QProcess::Crashed:
            errorMsg = QStringLiteral("Upgrade process crashed.");
            break;
        case QProcess::Timedout:
            errorMsg = QStringLiteral("Upgrade process timed out.");
            break;
        case QProcess::WriteError:
            errorMsg = QStringLiteral("Write error occurred during upgrade.");
            break;
        case QProcess::ReadError:
            errorMsg = QStringLiteral("Read error occurred during upgrade.");
            break;
        default:
            errorMsg = QStringLiteral("Unknown error occurred during upgrade.");
            break;
    }

    QMessageBox::critical(this, QStringLiteral("Upgrade Error"), errorMsg);

    if (upgradeProcess) {
        upgradeProcess->deleteLater();
        upgradeProcess = nullptr;
    }
}