#pragma once

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QDBusConnection>
#include <QProcess>
#include <QRegularExpression>
#include <QMutex>
#include <QMutexLocker>
#include <QAtomicInteger>
#include <QStringList>

class SystemMonitor : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mxlinux.UpdateNotifierSystemMonitor")

public:
    explicit SystemMonitor(bool requireChecksum = true);

public Q_SLOTS:
    QString GetState();
    QString GetStateSummary();
    void Refresh();
    void DelayRefresh(int seconds);
    void SetCheckInterval(int seconds);
    void SetIdleTimeout(int seconds);
    void SetRefreshPaused(bool paused);
    void UpdateAurSetting(const QString& key, const QString& value);

Q_SIGNALS:
    void stateChanged(const QString& state);
    void summaryChanged(const QString& summary);

private Q_SLOTS:
    void refresh();
    void checkIdle();

private:
    void refresh(bool syncDb);
    bool syncPacmanDb();
    bool isUpdateAvailable(const QString& pkg);
    bool isPacmanLocked() const;
    QJsonObject buildState(const QStringList& repoLines, const QStringList& aurLines);
    QJsonObject parsePacmanConf(const QString& path = QStringLiteral("/etc/pacman.conf"));
    QList<QJsonObject> parseUpdateLines(const QStringList& lines);
    QString getLocalVersion(const QString& pkg);
    QString getSyncVersion(const QString& pkg);
    QString pacmanFieldOutput(const QStringList& args, const QString& field);
    QStringList getGroupPackages(const QString& group);
    QStringList getReplacedPackages(const QString& pkg);
    QStringList runPacmanQuery();
    QStringList runAurQuery();
    void touch();

    bool requireChecksum;
    QString cachedStateJson; // Cache serialized JSON to avoid repeated serialization
    qint64 lastStateChange;  // Track when state was last modified
    QString cachedSummaryJson; // Cache summary JSON to avoid repeated serialization
    qint64 lastSummaryChange;  // Track when summary was last modified
    QTimer* checkTimer;
    QTimer* idleTimer;
    qint64 lastActivity;
    int checkInterval;
    int idleTimeout;
    int pendingUpgradeCount;
    bool refreshPaused = false;
    bool refreshDelayed = false;
    QAtomicInteger<bool> refreshRetryScheduled;
    QMutex stateMutex;
    static const QRegularExpression UPDATE_RE;
};
