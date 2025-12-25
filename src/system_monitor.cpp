#include "system_monitor.h"
#include "common.h"
#include <QDebug>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

const QRegularExpression SystemMonitor::UPDATE_RE = QRegularExpression(QStringLiteral(R"(^(\S+)\s+(\S+)\s+->\s+(\S+))"));

SystemMonitor::SystemMonitor(bool requireChecksum)
    : QObject()
    , requireChecksum(requireChecksum)
    , state(readState())
    , checkTimer(new QTimer(this))
    , idleTimer(new QTimer(this))
    , lastActivity(QDateTime::currentSecsSinceEpoch())
    , checkInterval(readSetting(QStringLiteral("Settings/check_interval"), DEFAULT_CHECK_INTERVAL).toInt())
    , idleTimeout(readSetting(QStringLiteral("Settings/idle_timeout"), DEFAULT_IDLE_TIMEOUT).toInt())
{
    connect(checkTimer, &QTimer::timeout, this, &SystemMonitor::refresh);
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
    refresh();
}

void SystemMonitor::SetIdleTimeout(int seconds) {
    idleTimeout = qMax(30, seconds);
    touch();
}

void SystemMonitor::refresh() {
    touch();
    QStringList lines = runPacmanQuery();
    state = buildState(lines);
    writeState(state);
    QJsonDocument doc(state);
    emit stateChanged(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
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
    for (const QString& line : output.split(QStringLiteral("\n"), Qt::SkipEmptyParts)) {
        lines.append(line.trimmed());
    }
    return lines;
}

QJsonObject SystemMonitor::buildState(const QStringList& lines) {
    QList<QJsonObject> updates = parseUpdateLines(lines);
    qint64 now = QDateTime::currentSecsSinceEpoch();

    QJsonObject newState = defaultState();
    newState[QStringLiteral("checked_at")] = now;
    newState[QStringLiteral("packages")] = QJsonArray::fromStringList(lines);

    QJsonObject counts = newState[QStringLiteral("counts")].toObject();
    counts[QStringLiteral("upgrade")] = updates.size();

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
            QString name = line.split(QStringLiteral(" ")).first();
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
        line = line.split(QStringLiteral("#")).first().trimmed();

        if (line.isEmpty() || line.startsWith(QStringLiteral("#"))) {
            continue;
        }

        if (line.startsWith(QStringLiteral("IgnorePkg"))) {
            QString value = line.split(QStringLiteral("=")).last().trimmed();
            QStringList packages = value.split(QStringLiteral(" "), Qt::SkipEmptyParts);
            for (const QString& pkg : packages) {
                QString cleanedPkg = pkg;
                ignorePkg.append(cleanedPkg.remove(QStringLiteral(",")));
            }
        } else if (line.startsWith(QStringLiteral("IgnoreGroup"))) {
            QString value = line.split(QStringLiteral("=")).last().trimmed();
            QStringList groups = value.split(QStringLiteral(" "), Qt::SkipEmptyParts);
            for (const QString& group : groups) {
                QString cleanedGroup = group;
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
    for (const QString& line : output.split(QStringLiteral("\n"))) {
        if (line.trimmed().startsWith(field)) {
            QStringList parts = line.split(QStringLiteral(":"));
            if (parts.size() >= 2) {
                return parts.mid(1).join(QStringLiteral(":")).trimmed();
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
    for (const QString& item : replaces.split(QStringLiteral(" "), Qt::SkipEmptyParts)) {
        QString cleanedItem = item;
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
    for (const QString& line : output.split(QStringLiteral("\n"), Qt::SkipEmptyParts)) {
        packages.append(line.trimmed());
    }
    return packages;
}

void SystemMonitor::touch() {
    lastActivity = QDateTime::currentSecsSinceEpoch();
}