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

QStringList shellQuoteArguments(const QStringList &args) {
    QStringList quoted;
    quoted.reserve(args.size());
    for (const QString &arg : args) {
        quoted.append(shellQuoteArgument(arg));
    }
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
    setWindowTitle(QStringLiteral("Update Notifier Qt"));
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
    if (iface && iface->isValid()) {
        iface->call(QStringLiteral("SetRefreshPaused"), false);
    }
    if (upgradeProcess && upgradeProcess->state() == QProcess::Running) {
        // Upgrade in progress - disconnect signals to prevent calling slots on destroyed object
        upgradeProcess->disconnect(this);
        // Let the upgrade complete - don't kill it as that could corrupt the system
        // The process will be cleaned up when it finishes due to deleteLater() calls
    }
    if (refreshTimer) {
        refreshTimer->stop();
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
    if (iface && iface->isValid()) {
        iface->call(QStringLiteral("SetRefreshPaused"), false);
    }
}

void ViewAndUpgrade::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (iface && iface->isValid()) {
        iface->call(QStringLiteral("SetRefreshPaused"), true);
    }
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
        QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
        QStringLiteral("/org/mxlinux/UpdateNotifierSystemMonitor"),
        QStringLiteral("org.mxlinux.UpdateNotifierSystemMonitor"),
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

    // Add completion message and wait for user to press Enter
    // This ensures the terminal stays open after the command completes
    static const QString completionMessage =
        QStringLiteral("; echo ''; echo '===================='; echo 'Update completed!'; "
                       "echo 'Press Enter to close this window...'; echo '===================='; "
                       "read -r; exit");

    QStringList fullParts;
    fullParts.reserve(args.size() + 1);
    fullParts.append(shellQuoteArgument(command));
    for (const QString& arg : args) {
        fullParts.append(shellQuoteArgument(arg));
    }

    // Build complete command string in one operation by appending completion message to parts list
    QString fullCommand = fullParts.join(QLatin1Char(' ')) + completionMessage;

    for (const QString& terminal : terminals) {
        // Check if terminal is available
        if (!QStandardPaths::findExecutable(terminal).isEmpty()) {
            // Terminal is available, try to launch it
            QStringList terminalArgs;

            if (terminal == QStringLiteral("konsole")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("gnome-terminal")) {
                terminalArgs << QStringLiteral("--") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("alacritty")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("xfce4-terminal")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("mate-terminal")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("lxterminal")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("xterm")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
            } else if (terminal == QStringLiteral("urxvt")) {
                terminalArgs << QStringLiteral("-e") << QStringLiteral("bash") << QStringLiteral("-c") << fullCommand;
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
                            if (!refreshTimer) {
                                refreshTimer = new QTimer(this);
                                refreshTimer->setSingleShot(true);
                                connect(refreshTimer, &QTimer::timeout, this, &ViewAndUpgrade::refresh);
                            }
                            refreshTimer->start(2000);
                        });
                (*monitorProcess)->start(terminal, terminalArgs);
                success = (*monitorProcess)->waitForStarted(2000);
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
    // Collect selected packages from tree, separated by type
    QStringList repoPackages;
    QStringList aurPackages;

    // Iterate once to collect packages (avoiding double iteration for capacity estimation)
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

            if (itemType == QStringLiteral("repo_package")) {
                repoPackages.append(packageName);
            } else if (itemType == QStringLiteral("aur_package")) {
                aurPackages.append(packageName);
            }
        }
        ++it;
    }

    if (repoPackages.isEmpty() && aurPackages.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("No Packages Selected"),
                                QStringLiteral("Please select at least one package to upgrade."));
        return;
    }

    // Determine command and arguments based on package type
    QString command;
    QStringList args;
    QString fullBashCommand;

    if (!repoPackages.isEmpty() && !aurPackages.isEmpty()) {
        // MIXED: Sequential execution in same terminal - repo packages first, then AUR
        QString aurHelper = readSetting(QStringLiteral("Settings/aur_helper"), QStringLiteral("")).toString();
        if (aurHelper.isEmpty()) {
            aurHelper = detectAurHelper();
            if (aurHelper.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("AUR Helper Not Found"),
                                    QStringLiteral("No AUR helper found. Please install paru, yay, or another AUR helper."));
                return;
            }
        }

        // Build compound bash command for sequential execution
        QStringList repoArgs = shellQuoteArguments(repoPackages);
        QStringList aurArgs = shellQuoteArguments(aurPackages);
        QString sudoPacmanCommand = QStringLiteral("sudo pacman -S %1").arg(repoArgs.join(QLatin1Char(' ')));
        QString aurCommand = QStringLiteral("%1 %2").arg(shellQuoteArgument(aurHelper), aurArgs.join(QLatin1Char(' ')));

        fullBashCommand = QStringLiteral(
            "echo 'Upgrading repository packages...'; "
            "echo 'Command: %1'; "
            "%2; "
            "if [ $? -eq 0 ]; then "
            "  echo ''; echo '===================='; echo 'Repository update completed! Continuing with AUR updates...'; echo '===================='; echo ''; "
            "  echo 'Upgrading AUR packages...'; "
            "  echo 'Command: %3'; "
            "  %4; "
            "  echo ''; echo '===================='; echo 'Update completed!'; echo 'Press Enter to close this window...'; echo '===================='; read -r; exit; "
            "else "
            "  echo ''; echo '===================='; echo 'Repository package upgrade failed. Stopping.'; echo 'Press Enter to close this window...'; echo '===================='; read -r; exit 1; "
            "fi"
        ).arg(shellQuoteArgument(sudoPacmanCommand),
              sudoPacmanCommand,
              shellQuoteArgument(aurCommand),
              aurCommand);

        command = QStringLiteral("bash");
        args << QStringLiteral("-c") << fullBashCommand;

    } else if (!repoPackages.isEmpty()) {
        // REPO-ONLY: Use sudo + pacman (changed from pkexec) with command display
        QStringList quotedRepoPackages = shellQuoteArguments(repoPackages);
        QString pacmanCommand = QStringLiteral("sudo pacman -S %1").arg(quotedRepoPackages.join(QLatin1Char(' ')));
        fullBashCommand = QStringLiteral(
            "echo 'Upgrading repository packages...'; "
            "echo 'Command: %1'; "
            "%2"
        ).arg(shellQuoteArgument(pacmanCommand), pacmanCommand);

        command = QStringLiteral("bash");
        args << QStringLiteral("-c") << fullBashCommand;

    } else if (!aurPackages.isEmpty()) {
        // AUR-ONLY: Use AUR helper
        command = readSetting(QStringLiteral("Settings/aur_helper"), QStringLiteral("")).toString();
        if (command.isEmpty()) {
            command = detectAurHelper();
            if (command.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("AUR Helper Not Found"),
                                    QStringLiteral("No AUR helper found. Please install paru, yay, or another AUR helper."));
                return;
            }
        }
        args = aurPackages;
    }

    // Launch upgrade in terminal
    QProcess* terminalProcess = nullptr;
    if (iface && iface->isValid()) {
        iface->call(QStringLiteral("DelayRefresh"), 120);
    }
    bool terminalLaunched = launchInTerminal(command, args, &terminalProcess);
    if (!terminalLaunched) {
        QMessageBox::warning(this, QStringLiteral("Terminal Not Found"),
                            QStringLiteral("Could not find a suitable terminal emulator to run the update.\n\n"
                                          "Please install a terminal emulator like konsole, gnome-terminal, alacritty, or xterm."));
        return;
    }

    countsLabel->setText(QStringLiteral("Upgrade in progress in terminal..."));
    // Refresh the UI from cached state only; avoid forcing a pacman refresh mid-upgrade
    QTimer::singleShot(3000, this, &ViewAndUpgrade::loadState);
}

void ViewAndUpgrade::onUpgradeFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    // Refresh system monitor data first
    if (iface && iface->isValid()) {
        iface->call(QStringLiteral("Refresh"));
    }

    // Refresh the UI to show updated package list
    refresh();

    // Also refresh the tray icon immediately
    if (!trayIface) {
        trayIface = new QDBusInterface(
            QStringLiteral("org.mxlinux.UpdateNotifierTrayIcon"),
            QStringLiteral("/org/mxlinux/UpdaterSystemTrayIcon"),
            QStringLiteral("org.mxlinux.UpdateNotifierTrayIcon"),
            QDBusConnection::sessionBus(),
            this
        );
        if (!trayIface->isValid()) {
            qWarning() << "Failed to connect to tray icon D-Bus service:"
                       << trayIface->lastError().message();
            delete trayIface;
            trayIface = nullptr;
        }
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
    suppressItemChanged = true;
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
    suppressItemChanged = false;
}

void ViewAndUpgrade::onTreeItemChanged(QTreeWidgetItem* item, int column) {
    if (column != 0) return;
    if (suppressItemChanged) return;

    QString itemType = item->data(0, Qt::UserRole).toString();
    Qt::CheckState checkState = item->checkState(0);

    if (itemType == QStringLiteral("repo_branch")) {
        // When repo branch is toggled, toggle all repo packages
        suppressItemChanged = true;
        for (int i = 0; i < item->childCount(); ++i) {
            item->child(i)->setCheckState(0, checkState);
        }
        suppressItemChanged = false;
    } else if (itemType == QStringLiteral("aur_branch")) {
        // When AUR branch is toggled, toggle all AUR packages
        suppressItemChanged = true;
        for (int i = 0; i < item->childCount(); ++i) {
            item->child(i)->setCheckState(0, checkState);
        }
        suppressItemChanged = false;
    } else if (itemType == QStringLiteral("repo_package") || itemType == QStringLiteral("aur_package")) {
        QTreeWidgetItem* parent = item->parent();
        if (parent) {
            bool allChecked = true;
            for (int i = 0; i < parent->childCount(); ++i) {
                if (parent->child(i)->checkState(0) != Qt::Checked) {
                    allChecked = false;
                    break;
                }
            }
            suppressItemChanged = true;
            parent->setCheckState(0, allChecked ? Qt::Checked : Qt::Unchecked);
            suppressItemChanged = false;
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
    bool previous = selectAllCheckbox->blockSignals(true);
    selectAllCheckbox->setChecked(allChecked);
    selectAllCheckbox->blockSignals(previous);
}
