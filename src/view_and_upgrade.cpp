#include "view_and_upgrade.h"
#include "common.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDBusReply>
#include <QMessageBox>
#include <QApplication>
#include <QThread>

ViewAndUpgrade::ViewAndUpgrade(QWidget* parent)
    : QDialog(parent)
    , countsLabel(new QLabel(this))
    , selectAllCheckbox(new QCheckBox(QStringLiteral("Select All"), this))
    , listWidget(new QListWidget(this))
    , buttonRefresh(new QPushButton(QStringLiteral("Refresh"), this))
    , buttonUpgrade(new QPushButton(QStringLiteral("Upgrade"), this))
    , buttonClose(new QPushButton(QStringLiteral("Close"), this))
    , iface(nullptr)
    , trayIface(nullptr)
    , progressDialog(nullptr)
    , upgradeProcess(nullptr)
    , upgradeDialog(nullptr)
    , upgradeOutput(nullptr)
    , upgradeButtons(nullptr)
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

    // Don't create trayIface during initialization to avoid auto-activation
    // Will create it lazily when needed
    trayIface = nullptr;
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

    QString countsText = QString(QStringLiteral("Upgrades: %1 | Remove: %2 | Held: %3"))
        .arg(counts[QStringLiteral("upgrade")].toInt())
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

bool ViewAndUpgrade::launchInTerminal(const QString& command, const QStringList& args) {
    // List of common terminal emulators to try, in order of preference
    const QStringList terminals = {
        QStringLiteral("konsole"),      // KDE
        QStringLiteral("gnome-terminal"), // GNOME
        QStringLiteral("alacritty"),    // Popular lightweight terminal
        QStringLiteral("xfce4-terminal"), // XFCE
        QStringLiteral("mate-terminal"), // MATE
        QStringLiteral("lxterminal"),   // LXDE
        QStringLiteral("xterm"),        // Fallback, usually available
        QStringLiteral("urxvt"),        // rxvt-unicode
        QStringLiteral("st")            // Simple Terminal
    };

    QString fullCommand = command;
    for (const QString& arg : args) {
        fullCommand += QStringLiteral(" \"") + arg + QStringLiteral("\"");
    }

    for (const QString& terminal : terminals) {
        // Check if terminal is available
        QProcess checkProcess;
        checkProcess.start(QStringLiteral("which"), QStringList() << terminal);
        if (checkProcess.waitForFinished(1000) && checkProcess.exitCode() == 0) {
            // Terminal is available, try to launch it
            QStringList terminalArgs;

            if (terminal == QStringLiteral("konsole")) {
                terminalArgs << QStringLiteral("--hold") << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("gnome-terminal")) {
                terminalArgs << QStringLiteral("--") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("alacritty")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("xfce4-terminal")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash -c '") + fullCommand + QStringLiteral("; read -p \"Press Enter to close\"'") << QStringLiteral("--hold");
            } else if (terminal == QStringLiteral("mate-terminal")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash -c '") + fullCommand + QStringLiteral("; read -p \"Press Enter to close\"'");
            } else if (terminal == QStringLiteral("lxterminal")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash -c '") + fullCommand + QStringLiteral("; read -p \"Press Enter to close\"'");
            } else if (terminal == QStringLiteral("xterm")) {
                terminalArgs << QStringLiteral("-hold") << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("urxvt")) {
                terminalArgs << QStringLiteral("-hold") << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("st")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else {
                // Generic fallback
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            }

            bool success = QProcess::startDetached(terminal, terminalArgs);
            if (success) {
                return true;
            }
        }
    }

    return false; // No suitable terminal found
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

    QString upgradeMode = readSetting(QStringLiteral("Settings/upgrade_mode"), QStringLiteral("standard")).toString();

    if (upgradeMode == QStringLiteral("include AUR updates")) {
        // Launch AUR helper in terminal for AUR updates
        QString aurHelper = readSetting(QStringLiteral("Settings/aur_helper"), QStringLiteral("paru")).toString();
        if (aurHelper.isEmpty()) {
            aurHelper = QStringLiteral("paru");
        }

        // Try to launch AUR helper in a terminal
        bool terminalLaunched = launchInTerminal(aurHelper, selectedPackages);
        if (!terminalLaunched) {
            QMessageBox::warning(this, QStringLiteral("Terminal Not Found"),
                                QStringLiteral("Could not find a suitable terminal emulator to run the AUR update.\n\n"
                                              "Please install a terminal emulator like konsole, gnome-terminal, alacritty, or xterm."));
            return;
        }
        QMessageBox::information(this, QStringLiteral("AUR Update Started"),
                                QStringLiteral("AUR package update has been started in a terminal window.\n\nPlease monitor the terminal for any prompts or errors."));
        return;
    }

    // Standard mode: use pacman with the upgrade dialog
    QStringList command;
    command << QStringLiteral("pkexec") << QStringLiteral("pacman") << QStringLiteral("-S") << QStringLiteral("--noconfirm");
    command.append(selectedPackages);

    // Create upgrade dialog with output display
    upgradeDialog = new QDialog(this);
    upgradeDialog->setWindowTitle(QStringLiteral("Upgrading Packages"));
    upgradeDialog->setModal(true);
    upgradeDialog->resize(600, 400);

    upgradeOutput = new QTextEdit(upgradeDialog);
    upgradeOutput->setReadOnly(true);
    upgradeOutput->setFont(QFont(QStringLiteral("Monospace"), 9));

    upgradeButtons = new QDialogButtonBox(QDialogButtonBox::Cancel, upgradeDialog);
    connect(upgradeButtons, &QDialogButtonBox::rejected, this, &ViewAndUpgrade::onUpgradeCancel);

    // Create upgrade dialog with output display (but don't show yet)
    upgradeDialog = new QDialog(this);
    upgradeDialog->setWindowTitle(QStringLiteral("Upgrading Packages"));
    upgradeDialog->setModal(true);
    upgradeDialog->resize(600, 400);

    upgradeOutput = new QTextEdit(upgradeDialog);
    upgradeOutput->setReadOnly(true);
    upgradeOutput->setFont(QFont(QStringLiteral("Monospace"), 9));

    upgradeButtons = new QDialogButtonBox(QDialogButtonBox::Cancel, upgradeDialog);
    connect(upgradeButtons, &QDialogButtonBox::rejected, this, &ViewAndUpgrade::onUpgradeCancel);

    QVBoxLayout* upgradeLayout = new QVBoxLayout(upgradeDialog);
    upgradeLayout->addWidget(new QLabel(QStringLiteral("Package upgrade in progress..."), upgradeDialog));
    upgradeLayout->addWidget(upgradeOutput);
    upgradeLayout->addWidget(upgradeButtons);

    upgradeProcess = new QProcess(this);
    connect(upgradeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ViewAndUpgrade::onUpgradeFinished);
    connect(upgradeProcess, &QProcess::errorOccurred,
            this, &ViewAndUpgrade::onUpgradeError);

    // Connect output signals to both show dialog and display output
    connect(upgradeProcess, &QProcess::readyReadStandardOutput,
            this, &ViewAndUpgrade::onUpgradeOutput);
    connect(upgradeProcess, &QProcess::readyReadStandardError,
            this, &ViewAndUpgrade::onUpgradeOutput);

    // Show dialog only when we first receive output (authentication complete)
    connect(upgradeProcess, &QProcess::readyReadStandardOutput,
            this, &ViewAndUpgrade::onFirstOutput);
    connect(upgradeProcess, &QProcess::readyReadStandardError,
            this, &ViewAndUpgrade::onFirstOutput);

    upgradeProcess->start(command.first(), command.mid(1));

    if (!upgradeProcess->waitForStarted(5000)) {
        // Clean up dialog components without showing them
        if (upgradeDialog) {
            upgradeDialog->deleteLater();
            upgradeDialog = nullptr;
        }

        QMessageBox::critical(this, QStringLiteral("Upgrade Error"),
                            QStringLiteral("Failed to start upgrade process."));

        upgradeProcess->deleteLater();
        upgradeProcess = nullptr;
        upgradeOutput = nullptr;
        upgradeButtons = nullptr;
    }
}

void ViewAndUpgrade::onUpgradeFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    // Refresh system monitor data first
    if (iface && iface->isValid()) {
        iface->call(QStringLiteral("Refresh"));
        // Give a moment for the refresh to complete
        QThread::msleep(1000);
    }

    // Refresh the UI to show updated package list
    refresh();

    // Also refresh the tray icon immediately
    if (!trayIface) {
        trayIface = new QDBusInterface(
            QStringLiteral("org.mxlinux.UpdaterSystemTrayIcon"),
            QStringLiteral("/org/mxlinux/UpdaterSystemTrayIcon"),
            QStringLiteral("org.mxlinux.UpdaterSystemTrayIcon"),
            QDBusConnection::sessionBus(),
            this
        );
    }
    if (trayIface && trayIface->isValid()) {
        trayIface->call(QStringLiteral("Refresh"));
    }

    // Now close the upgrade dialog
    if (upgradeDialog) {
        upgradeDialog->close();
        upgradeDialog->deleteLater();
        upgradeDialog = nullptr;
        upgradeOutput = nullptr;
        upgradeButtons = nullptr;
    }

    if (upgradeProcess) {
        upgradeProcess->deleteLater();
        upgradeProcess = nullptr;
    }
}

void ViewAndUpgrade::onUpgradeError(QProcess::ProcessError error) {
    if (upgradeDialog) {
        upgradeDialog->close();
        upgradeDialog->deleteLater();
        upgradeDialog = nullptr;
        upgradeOutput = nullptr;
        upgradeButtons = nullptr;
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

void ViewAndUpgrade::onFirstOutput() {
    // Show dialog when we first receive output (authentication complete)
    if (upgradeDialog && !upgradeDialog->isVisible()) {
        upgradeDialog->show();
    }

    // Disconnect this slot so it only runs once
    disconnect(upgradeProcess, &QProcess::readyReadStandardOutput,
               this, &ViewAndUpgrade::onFirstOutput);
    disconnect(upgradeProcess, &QProcess::readyReadStandardError,
               this, &ViewAndUpgrade::onFirstOutput);
}

void ViewAndUpgrade::onUpgradeOutput() {
    if (upgradeProcess && upgradeOutput) {
        QByteArray stdoutData = upgradeProcess->readAllStandardOutput();
        QByteArray stderrData = upgradeProcess->readAllStandardError();

        if (!stdoutData.isEmpty()) {
            upgradeOutput->append(QString::fromUtf8(stdoutData));
        }
        if (!stderrData.isEmpty()) {
            upgradeOutput->append(QStringLiteral("<font color=\"red\">") +
                                 QString::fromUtf8(stderrData) +
                                 QStringLiteral("</font>"));
        }

        // Auto-scroll to bottom
        QTextCursor cursor = upgradeOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        upgradeOutput->setTextCursor(cursor);
    }
}

void ViewAndUpgrade::onUpgradeCancel() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        upgradeDialog,
        QStringLiteral("Cancel Upgrade"),
        QStringLiteral("Are you sure you want to cancel the package upgrade?\n\n"
                      "This may result in incomplete installation and system instability."),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (upgradeProcess) {
            upgradeProcess->terminate();
            // Give it 5 seconds to terminate gracefully
            if (!upgradeProcess->waitForFinished(5000)) {
                upgradeProcess->kill();
            }
        }
        if (upgradeDialog) {
            upgradeDialog->close();
        }
    }
}