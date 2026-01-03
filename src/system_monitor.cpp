#include "system_monitor.h"
#include "common.h"
#include <QDebug>
#include <QJsonArray>
#include <QDateTime>
#include <QStringTokenizer>
#include <QStringView>
#include <QDBusInterface>

const QRegularExpression SystemMonitor::UPDATE_RE = QRegularExpression(QStringLiteral(R"(^(\S+)\s+(\S+)\s+->\s+(\S+))"));

SystemMonitor::SystemMonitor(bool requireChecksum)
    : QObject()
    , requireChecksum(requireChecksum)
    , settingsIface(nullptr)
    , state(readState())
    , checkTimer(new QTimer(this))
    , idleTimer(new QTimer(this))
    , lastActivity(QDateTime::currentSecsSinceEpoch())
    , checkInterval(readSetting(QStringLiteral("Settings/check_interval"), DEFAULT_CHECK_INTERVAL).toInt())
    , idleTimeout(readSetting(QStringLiteral("Settings/idle_timeout"), DEFAULT_IDLE_TIMEOUT).toInt())
    , pendingUpgradeCount(0)
{

    // Connect to settings service to receive AUR settings changes
    settingsIface = new QDBusInterface(QStringLiteral("org.mxlinux.UpdaterSettings"),
                                       QStringLiteral("/org/mxlinux/UpdaterSettings"),
                                       QStringLiteral("org.mxlinux.UpdaterSettings"),
                                       QDBusConnection::sessionBus(), this);
    if (settingsIface->isValid()) {
        connect(settingsIface, SIGNAL(settingsChanged(QString, QString)), this,
                SLOT(onSettingsChanged(QString, QString)));
        fprintf(stderr, "DEBUG: Connected to settings service for AUR updates\n");
    } else {
        fprintf(stderr, "DEBUG: Failed to connect to settings service\n");
    }
    fflush(stderr);

    connect(checkTimer, &QTimer::timeout, this, qOverload<>(&SystemMonitor::refresh));
    checkTimer->start(checkInterval * 1000);

    connect(idleTimer, &QTimer::timeout, this, &SystemMonitor::checkIdle);
    idleTimer->start(30 * 1000);
}

QString SystemMonitor::GetState() {
    touch();
    QJsonDocument doc(state);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void SystemMonitor::Refresh() {
    refresh(true);
}

void SystemMonitor::SetIdleTimeout(int seconds) {
    idleTimeout = qMax(30, seconds);
    touch();
}

void SystemMonitor::UpdateAurSetting(const QString& key, const QString& value) {
    // Update the state file with the new AUR setting
    QJsonObject currentState = readState(STATE_FILE_PATH, false);

    if (key == QStringLiteral("Settings/aur_enabled")) {
        currentState[QStringLiteral("aur_enabled")] = (value == QStringLiteral("true"));
    } else if (key == QStringLiteral("Settings/aur_helper")) {
        currentState[QStringLiteral("aur_helper")] = value;
    }
    writeState(currentState);
}

void SystemMonitor::onSettingsChanged(const QString& key, const QString& value) {
    // Update state file when AUR settings change
    if (key == QStringLiteral("Settings/aur_enabled") || key == QStringLiteral("Settings/aur_helper")) {
        QJsonObject currentState = readState(STATE_FILE_PATH, false);
        if (key == QStringLiteral("Settings/aur_enabled")) {
            currentState[QStringLiteral("aur_enabled")] = (value == QStringLiteral("true"));
        } else if (key == QStringLiteral("Settings/aur_helper")) {
            currentState[QStringLiteral("aur_helper")] = value;
        }
        writeState(currentState);
    }
}

void SystemMonitor::refresh() {
    refresh(false);
}

void SystemMonitor::refresh(bool syncDb) {
    touch();
    if (syncDb) {
        syncPacmanDb();
    }

    QStringList repoLines = runPacmanQuery();
    QStringList aurLines;

    // Read AUR settings from state file (not QSettings, since we run as root)
    // The state file is updated by the settings dialog when user saves preferences
    bool aurEnabled = state[QStringLiteral("aur_enabled")].toBool(false);
    QString aurHelper = state[QStringLiteral("aur_helper")].toString();



    if (aurEnabled) {
        aurLines = runAurQuery();
    }

    state = buildState(repoLines, aurLines);

    // Preserve AUR settings in the new state
    state[QStringLiteral("aur_enabled")] = aurEnabled;
    state[QStringLiteral("aur_helper")] = aurHelper;

    writeState(state);
    QJsonDocument doc(state);
    emit stateChanged(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
}

bool SystemMonitor::syncPacmanDb() {
    QProcess process;
    process.start(QStringLiteral("pacman"), QStringList() << QStringLiteral("-Sy"));

    if (!process.waitForStarted(5000)) {
        qWarning() << "Failed to start pacman -Sy:" << process.errorString();
        return false;
    }

    if (!process.waitForFinished(60000)) { // 60 second timeout
        if (process.error() == QProcess::Timedout) {
            qWarning() << "pacman -Sy timed out after 60 seconds";
        } else {
            qWarning() << "pacman -Sy process error:" << process.errorString();
        }
        process.kill();
        return false;
    }

    if (process.exitCode() != 0) {
        qWarning() << "pacman -Sy exited with code:" << process.exitCode();
        return false;
    }

    return true;
}

void SystemMonitor::checkIdle() {
    if (QDateTime::currentSecsSinceEpoch() - lastActivity > idleTimeout) {
        QCoreApplication::quit();
    }
}

QStringList SystemMonitor::runPacmanQuery() {
    QProcess process;
    process.start(QStringLiteral("pacman"), QStringList() << QStringLiteral("-Qu"));

    if (!process.waitForStarted(5000)) {
        qWarning() << "Failed to start pacman process:" << process.errorString();
        return QStringList();
    }

    if (!process.waitForFinished(30000)) { // 30 second timeout
        if (process.error() == QProcess::Timedout) {
            qWarning() << "pacman -Qu timed out after 30 seconds";
        } else {
            qWarning() << "pacman process error:" << process.errorString();
        }
        process.kill();
        return QStringList();
    }

    int exitCode = process.exitCode();
    if (exitCode != 0 && exitCode != 1) {
        qWarning() << "pacman -Qu exited with code:" << exitCode;
        return QStringList();
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lines;
    lines.reserve(output.count(QLatin1Char('\n')) + 1);
    for (QStringView lineView : QStringTokenizer{output, u'\n', Qt::SkipEmptyParts}) {
        lineView = lineView.trimmed();
        if (!lineView.isEmpty()) {
            lines.append(lineView.toString());
        }
    }
    return lines;
}

QStringList SystemMonitor::runAurQuery() {
    // Read aurHelper from state file (set by settings dialog)
    QString aurHelper = state[QStringLiteral("aur_helper")].toString();
    if (aurHelper.isEmpty()) {
        aurHelper = detectAurHelper();
        if (aurHelper.isEmpty()) {
            qWarning() << "No AUR helper available for AUR updates";
            return QStringList(); // No AUR helper available
        }
    }

    qWarning() << "Starting AUR query with helper:" << aurHelper << "command:" << aurHelper << "-Qua";

    QProcess process;
    process.start(aurHelper, QStringList() << QStringLiteral("-Qua"));

    if (!process.waitForStarted(5000)) {
        qWarning() << "Failed to start AUR helper process:" << process.errorString();
        return QStringList();
    }

    if (!process.waitForFinished(60000)) { // 60 second timeout (AUR queries can be slower)
        if (process.error() == QProcess::Timedout) {
            qWarning() << aurHelper << "-Qua timed out after 60 seconds";
        } else {
            qWarning() << aurHelper << "process error:" << process.errorString();
        }
        process.kill();
        return QStringList();
    }

    int exitCode = process.exitCode();
    if (exitCode != 0 && exitCode != 1) {
        qWarning() << aurHelper << "-Qua exited with code:" << exitCode;
        return QStringList();
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    qWarning() << "AUR query output:" << output;

    QStringList lines;
    lines.reserve(output.count(QLatin1Char('\n')) + 1);
    for (QStringView lineView : QStringTokenizer{output, u'\n', Qt::SkipEmptyParts}) {
        lineView = lineView.trimmed();
        if (!lineView.isEmpty()) {
            lines.append(lineView.toString());
        }
    }
    qWarning() << "AUR query parsed" << lines.size() << "lines";
    return lines;
}

QJsonObject SystemMonitor::buildState(const QStringList& repoLines, const QStringList& aurLines) {
    qint64 now = QDateTime::currentSecsSinceEpoch();

    QJsonObject newState = defaultState();
    newState[QStringLiteral("checked_at")] = now;
    newState[QStringLiteral("packages")] = QJsonArray::fromStringList(repoLines);
    newState[QStringLiteral("aur_packages")] = QJsonArray::fromStringList(aurLines);

    QJsonObject counts = newState[QStringLiteral("counts")].toObject();
    counts[QStringLiteral("upgrade")] = repoLines.size();
    counts[QStringLiteral("aur_upgrade")] = aurLines.size();
    counts[QStringLiteral("total_upgrade")] = repoLines.size() + aurLines.size();

    // Note: Held packages count removed - pacman -Qu already excludes ignored packages
    // Note: Replaced packages count removed - rarely used and expensive to calculate
    counts[QStringLiteral("remove")] = 0;
    counts[QStringLiteral("held")] = 0;

    newState[QStringLiteral("counts")] = counts;
    newState[QStringLiteral("status")] = QStringLiteral("ok");

    return newState;
}

QList<QJsonObject> SystemMonitor::parseUpdateLines(const QStringList& lines) {
    QList<QJsonObject> updates;
    for (const QString& line : lines) {
        QRegularExpressionMatch match = UPDATE_RE.match(line);
        if (match.hasMatch()) {
            QJsonObject update;
            update[QStringLiteral("name")] = match.captured(1);
            update[QStringLiteral("old")] = match.captured(2);
            update[QStringLiteral("new")] = match.captured(3);
            update[QStringLiteral("raw")] = line;
            updates.append(update);
        } else {
            QStringView lineView(line);
            qsizetype spaceIndex = lineView.indexOf(u' ');
            QString name = (spaceIndex < 0 ? lineView : lineView.left(spaceIndex)).toString();
            QJsonObject update;
            update[QStringLiteral("name")] = name;
            update[QStringLiteral("old")] = QStringLiteral("");
            update[QStringLiteral("new")] = QStringLiteral("");
            update[QStringLiteral("raw")] = line;
            updates.append(update);
        }
    }
    return updates;
}

QJsonObject SystemMonitor::parsePacmanConf(const QString& path) {
    QJsonObject result;
    QJsonArray ignorePkg;
    QJsonArray ignoreGroup;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result[QStringLiteral("ignore_pkg")] = ignorePkg;
        result[QStringLiteral("ignore_group")] = ignoreGroup;
        return result;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        qsizetype hashIndex = line.indexOf(u'#');
        if (hashIndex >= 0) {
            line = line.left(hashIndex).trimmed();
        }

        if (line.isEmpty() || line.startsWith(QStringLiteral("#"))) {
            continue;
        }

        if (line.startsWith(QStringLiteral("IgnorePkg"))) {
            qsizetype equalsIndex = line.indexOf(u'=');
            QStringView value = equalsIndex >= 0 ? QStringView(line).mid(equalsIndex + 1).trimmed()
                                                 : QStringView();
            for (QStringView pkg : QStringTokenizer{value, u' ', Qt::SkipEmptyParts}) {
                QString cleanedPkg = pkg.toString();
                ignorePkg.append(cleanedPkg.remove(QStringLiteral(",")));
            }
        } else if (line.startsWith(QStringLiteral("IgnoreGroup"))) {
            qsizetype equalsIndex = line.indexOf(u'=');
            QStringView value = equalsIndex >= 0 ? QStringView(line).mid(equalsIndex + 1).trimmed()
                                                 : QStringView();
            for (QStringView group : QStringTokenizer{value, u' ', Qt::SkipEmptyParts}) {
                QString cleanedGroup = group.toString();
                ignoreGroup.append(cleanedGroup.remove(QStringLiteral(",")));
            }
        }
    }

    result[QStringLiteral("ignore_pkg")] = ignorePkg;
    result[QStringLiteral("ignore_group")] = ignoreGroup;
    return result;
}

QString SystemMonitor::pacmanFieldOutput(const QStringList& args, const QString& field) {
    QProcess process;
    process.start(QStringLiteral("pacman"), args);
    if (!process.waitForFinished(10000)) { // 10 second timeout
        return QString();
    }

    if (process.exitCode() != 0) {
        return QString();
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringView fieldView(field);
    for (QStringView lineView : QStringTokenizer{output, u'\n'}) {
        lineView = lineView.trimmed();
        if (lineView.startsWith(fieldView)) {
            qsizetype colonIndex = lineView.indexOf(u':');
            if (colonIndex >= 0 && colonIndex + 1 < lineView.size()) {
                return lineView.mid(colonIndex + 1).trimmed().toString();
            }
        }
    }
    return QString();
}

QString SystemMonitor::getLocalVersion(const QString& pkg) {
    return pacmanFieldOutput(QStringList() << QStringLiteral("-Qi") << pkg, QStringLiteral("Version"));
}

QString SystemMonitor::getSyncVersion(const QString& pkg) {
    return pacmanFieldOutput(QStringList() << QStringLiteral("-Si") << pkg, QStringLiteral("Version"));
}

bool SystemMonitor::isUpdateAvailable(const QString& pkg) {
    QString localVer = getLocalVersion(pkg);
    QString syncVer = getSyncVersion(pkg);

    if (localVer.isEmpty() || syncVer.isEmpty()) {
        return false;
    }

    QProcess process;
    process.start(QStringLiteral("vercmp"), QStringList() << localVer << syncVer);
    if (!process.waitForFinished(5000)) {
        return false;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    return process.exitCode() == 0 && output == QStringLiteral("-1");
}

QStringList SystemMonitor::getReplacedPackages(const QString& pkg) {
    QString replaces = pacmanFieldOutput(QStringList() << QStringLiteral("-Si") << pkg, QStringLiteral("Replaces"));
    if (replaces.isEmpty() || replaces.toLower() == QStringLiteral("none")) {
        return QStringList();
    }

    QStringList items;
    for (QStringView item : QStringTokenizer{replaces, u' ', Qt::SkipEmptyParts}) {
        QString cleanedItem = item.toString();
        cleanedItem.remove(QStringLiteral(","));
        items.append(cleanedItem);
    }
    return items;
}

QStringList SystemMonitor::getGroupPackages(const QString& group) {
    QProcess process;
    process.start(QStringLiteral("pacman"), QStringList() << QStringLiteral("-Sqg") << group);
    if (!process.waitForFinished(10000)) {
        return QStringList();
    }

    if (process.exitCode() != 0) {
        return QStringList();
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList packages;
    packages.reserve(output.count(QLatin1Char('\n')) + 1);
    for (QStringView lineView : QStringTokenizer{output, u'\n', Qt::SkipEmptyParts}) {
        lineView = lineView.trimmed();
        if (!lineView.isEmpty()) {
            packages.append(lineView.toString());
        }
    }
    return packages;
}





void SystemMonitor::touch() {
    lastActivity = QDateTime::currentSecsSinceEpoch();
}
