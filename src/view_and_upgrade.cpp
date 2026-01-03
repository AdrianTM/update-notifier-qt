#include "view_and_upgrade.h"
#include "common.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDBusReply>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QIcon>
#include <QMessageBox>
#include <QApplication>
#include <QTimer>
#include <QThread>
#include <QStringView>

namespace {
QString shellQuoteArgument(const QString &arg) {
    if (arg.isEmpty()) {
        return QStringLiteral("''");
    }
    if (!arg.contains(QLatin1Char(' ')) && !arg.contains(QLatin1Char('\t')) &&
        !arg.contains(QLatin1Char('\n')) && !arg.contains(QLatin1Char('\''))) {
        return arg;
    }
    QString quoted = QStringLiteral("'");
    quoted.reserve(arg.size() + 2);
    for (QChar ch : arg) {
        if (ch == QLatin1Char('\'')) {
            quoted += QStringLiteral("'\\''");
        } else {
            quoted += ch;
        }
    }
    quoted += QLatin1Char('\'');
    return quoted;
}
} // namespace

ViewAndUpgrade::ViewAndUpgrade(QWidget* parent)
    : QDialog(parent)
    , countsLabel(new QLabel(this))
    , refreshProgress(new QProgressBar(this))
    , statusLayout(nullptr)
    , selectAllCheckbox(new QCheckBox(QStringLiteral("Select All"), this))
    , treeWidget(new QTreeWidget(this))
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
    QString theme = readSetting(QStringLiteral("Settings/icon_theme"),
                                QStringLiteral("modern-light"))
                        .toString();
    if (!isKnownIconTheme(theme)) {
        theme = QStringLiteral("modern-light");
    }
    setWindowIcon(QIcon(::iconPath(theme, QStringLiteral("updates-available.svg"))));
    resize(680, 420);

    ensureNotRoot();
    buildUi();
    setupDBus();
    countsLabel->setText(QStringLiteral("Loading updates..."));
    QTimer::singleShot(0, this, &ViewAndUpgrade::loadState);
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
    treeWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeWidget->setHeaderHidden(true);
    treeWidget->setRootIsDecorated(true);

    // Set up tree widget columns (single column for package info)
    treeWidget->setColumnCount(1);

    selectAllCheckbox->setChecked(true);
    connect(selectAllCheckbox, &QCheckBox::toggled, this, &ViewAndUpgrade::onSelectAllToggled);
    connect(treeWidget, &QTreeWidget::itemChanged, this, &ViewAndUpgrade::onTreeItemChanged);
    connect(buttonRefresh, &QPushButton::clicked, this, &ViewAndUpgrade::refresh);
    connect(buttonUpgrade, &QPushButton::clicked, this, &ViewAndUpgrade::upgrade);
    connect(buttonClose, &QPushButton::clicked, this, &ViewAndUpgrade::close);
    buttonUpgrade->setDefault(true);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(buttonRefresh);
    buttonLayout->addWidget(buttonUpgrade);
    buttonLayout->addWidget(buttonClose);

    refreshProgress->setRange(0, 0);
    refreshProgress->setTextVisible(false);

    QWidget* statusContainer = new QWidget(this);
    statusContainer->setContentsMargins(0, 0, 0, 0);
    statusLayout = new QStackedLayout(statusContainer);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->addWidget(countsLabel);
    statusLayout->addWidget(refreshProgress);
    statusLayout->setCurrentWidget(countsLabel);
    const int statusHeight = qMax(countsLabel->sizeHint().height(),
                                  refreshProgress->sizeHint().height());
    statusContainer->setMaximumHeight(statusHeight);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(statusContainer);
    mainLayout->addWidget(selectAllCheckbox);
    mainLayout->addWidget(treeWidget);
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

    setRefreshing(true);

    QDBusPendingCall pending = iface->asyncCall(QStringLiteral("Refresh"));
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        watcher->deleteLater();
        loadState();
    });
}

void ViewAndUpgrade::loadState() {
    if (!iface || !iface->isValid()) {
        countsLabel->setText(QStringLiteral("System monitor is not available."));
        return;
    }

    QDBusPendingCall pending = iface->asyncCall(QStringLiteral("GetState"));
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();
        if (!reply.isValid()) {
            countsLabel->setText(QStringLiteral("Unable to query system monitor."));
            setRefreshing(false);
            return;
        }

        applyState(reply.value());
        setRefreshing(false);
    });
}

void ViewAndUpgrade::applyState(const QString& payload) {
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        countsLabel->setText(QStringLiteral("Received invalid state from monitor."));
        setRefreshing(false);
        return;
    }

    QJsonObject state = doc.object();
    QJsonObject counts = state[QStringLiteral("counts")].toObject();

    int repoCount = counts[QStringLiteral("upgrade")].toInt();
    int aurCount = counts[QStringLiteral("aur_upgrade")].toInt();
    int totalCount = counts[QStringLiteral("total_upgrade")].toInt();

    QString countsText = QString(QStringLiteral("Upgrades: %1 repo + %2 AUR (%3 total) | Remove: %4 | Held: %5"))
        .arg(repoCount)
        .arg(aurCount)
        .arg(totalCount)
        .arg(counts[QStringLiteral("remove")].toInt())
        .arg(counts[QStringLiteral("held")].toInt());
    countsLabel->setText(countsText);

    treeWidget->clear();

    // Create repository updates branch
    QTreeWidgetItem* repoItem = nullptr;
    if (repoCount > 0) {
        repoItem = new QTreeWidgetItem(treeWidget);
        repoItem->setText(0, QStringLiteral("Official Repository Updates (%1)").arg(repoCount));
        repoItem->setFlags(repoItem->flags() | Qt::ItemIsUserCheckable);
        repoItem->setCheckState(0, Qt::Checked);
        repoItem->setData(0, Qt::UserRole, QStringLiteral("repo_branch"));

        QJsonArray packages = state[QStringLiteral("packages")].toArray();
        for (const QJsonValue& value : packages) {
            QTreeWidgetItem* item = new QTreeWidgetItem(repoItem);
            item->setText(0, value.toString());
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Checked);
            item->setData(0, Qt::UserRole, QStringLiteral("repo_package"));
        }
    }

    // Create AUR updates branch
    QTreeWidgetItem* aurItem = nullptr;
    if (aurCount > 0) {
        aurItem = new QTreeWidgetItem(treeWidget);
        aurItem->setText(0, QStringLiteral("AUR Updates (%1)").arg(aurCount));
        aurItem->setFlags(aurItem->flags() | Qt::ItemIsUserCheckable);
        aurItem->setCheckState(0, Qt::Checked);
        aurItem->setData(0, Qt::UserRole, QStringLiteral("aur_branch"));

        QJsonArray aurPackages = state[QStringLiteral("aur_packages")].toArray();
        for (const QJsonValue& value : aurPackages) {
            QTreeWidgetItem* item = new QTreeWidgetItem(aurItem);
            item->setText(0, value.toString());
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Checked);
            item->setData(0, Qt::UserRole, QStringLiteral("aur_package"));
        }
    }

    // Expand branches by default
    if (repoItem) repoItem->setExpanded(true);
    if (aurItem) aurItem->setExpanded(true);

    // Update Select All checkbox state
    selectAllCheckbox->setChecked(true);
}

void ViewAndUpgrade::setRefreshing(bool refreshing) {
    if (!statusLayout) {
        return;
    }
    QWidget *target = refreshing ? static_cast<QWidget *>(refreshProgress)
                                 : static_cast<QWidget *>(countsLabel);
    statusLayout->setCurrentWidget(target);
}



bool ViewAndUpgrade::launchInTerminal(const QString& command, const QStringList& args, QProcess** monitorProcess) {
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

    QStringList fullParts;
    fullParts.reserve(args.size() + 1);
    fullParts.append(shellQuoteArgument(command));
    for (const QString& arg : args) {
        fullParts.append(shellQuoteArgument(arg));
    }
    QString fullCommand = fullParts.join(QLatin1Char(' '));

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

            bool success;
            if (monitorProcess) {
                // Create a monitoring process
                *monitorProcess = new QProcess(this);
                connect(*monitorProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                        this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                            Q_UNUSED(exitCode)
                            Q_UNUSED(exitStatus)
                            // Auto-refresh after terminal closes
                            QTimer::singleShot(2000, this, &ViewAndUpgrade::refresh);
                        });
                success = (*monitorProcess)->startDetached(terminal, terminalArgs);
                if (!success) {
                    delete *monitorProcess;
                    *monitorProcess = nullptr;
                }
            } else {
                success = QProcess::startDetached(terminal, terminalArgs);
            }

            if (success) {
                return true;
            }
        }
    }

    return false; // No suitable terminal found
}

void ViewAndUpgrade::upgrade() {
    // Collect selected packages from tree
    QStringList selectedPackages;
    bool hasAurPackages = false;

    QTreeWidgetItemIterator it(treeWidget, QTreeWidgetItemIterator::Checked);
    while (*it) {
        QTreeWidgetItem* item = *it;
        QString itemType = item->data(0, Qt::UserRole).toString();

        if (itemType == QStringLiteral("repo_package") || itemType == QStringLiteral("aur_package")) {
            // Extract package name (first word before space)
            QString packageInfo = item->text(0);
            QStringView infoView(packageInfo);
            qsizetype spaceIndex = infoView.indexOf(u' ');
            QString packageName =
                (spaceIndex < 0 ? infoView : infoView.left(spaceIndex))
                    .toString();
            selectedPackages.append(packageName);

            if (itemType == QStringLiteral("aur_package")) {
                hasAurPackages = true;
            }
        }
        ++it;
    }

    if (selectedPackages.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Packages Selected"),
                                QStringLiteral("Please select at least one package to upgrade."));
        return;
    }

    // Use terminal upgrade if AUR packages are selected or AUR is enabled
    bool aurEnabled = readSetting(QStringLiteral("Settings/aur_enabled"), false).toBool();
    if (hasAurPackages || aurEnabled) {
        QString aurHelper = readSetting(QStringLiteral("Settings/aur_helper"), QStringLiteral("")).toString();
        if (aurHelper.isEmpty()) {
            aurHelper = detectAurHelper();
            if (aurHelper.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("AUR Helper Not Found"),
                                    QStringLiteral("No AUR helper found. Please install paru, yay, or another AUR helper."));
                return;
            }
        }

        // Try to launch AUR helper in a terminal with monitoring
        QProcess* terminalProcess = nullptr;
        bool terminalLaunched = launchInTerminal(aurHelper, selectedPackages, &terminalProcess);
        if (!terminalLaunched) {
            QMessageBox::warning(this, QStringLiteral("Terminal Not Found"),
                                QStringLiteral("Could not find a suitable terminal emulator to run the update.\n\n"
                                              "Please install a terminal emulator like konsole, gnome-terminal, alacritty, or xterm."));
            return;
        }

        countsLabel->setText(QStringLiteral("Upgrade in progress in terminal..."));
        // Schedule refresh after terminal closes (will be handled by terminal monitoring)
        QTimer::singleShot(3000, this, &ViewAndUpgrade::refresh);
        return;
    }

    // Standard mode: use pacman with the upgrade dialog (for repo-only packages)
    QStringList command;
    command << QStringLiteral("pkexec") << QStringLiteral("pacman") << QStringLiteral("-S") << QStringLiteral("--noconfirm");
    command.append(selectedPackages);

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

void ViewAndUpgrade::onSelectAllToggled(bool checked) {
    QTreeWidgetItemIterator it(treeWidget);
    while (*it) {
        QTreeWidgetItem* item = *it;
        QString itemType = item->data(0, Qt::UserRole).toString();
        if (itemType == QStringLiteral("repo_branch") || itemType == QStringLiteral("aur_branch") ||
            itemType == QStringLiteral("repo_package") || itemType == QStringLiteral("aur_package")) {
            item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        }
        ++it;
    }
}

void ViewAndUpgrade::onTreeItemChanged(QTreeWidgetItem* item, int column) {
    if (column != 0) return;

    QString itemType = item->data(0, Qt::UserRole).toString();
    Qt::CheckState checkState = item->checkState(0);

    if (itemType == QStringLiteral("repo_branch")) {
        // When repo branch is toggled, toggle all repo packages
        for (int i = 0; i < item->childCount(); ++i) {
            item->child(i)->setCheckState(0, checkState);
        }
    } else if (itemType == QStringLiteral("aur_branch")) {
        // When AUR branch is toggled, toggle all AUR packages
        for (int i = 0; i < item->childCount(); ++i) {
            item->child(i)->setCheckState(0, checkState);
        }
    }

    // Update Select All checkbox state based on whether all items are checked
    bool allChecked = true;
    QTreeWidgetItemIterator it(treeWidget);
    while (*it) {
        QTreeWidgetItem* checkItem = *it;
        QString checkItemType = checkItem->data(0, Qt::UserRole).toString();
        if ((checkItemType == QStringLiteral("repo_package") || checkItemType == QStringLiteral("aur_package")) &&
            checkItem->checkState(0) != Qt::Checked) {
            allChecked = false;
            break;
        }
        ++it;
    }
    selectAllCheckbox->setChecked(allChecked);
}
