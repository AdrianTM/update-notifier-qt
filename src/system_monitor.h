#pragma once

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QDBusConnection>
#include <QProcess>
#include <QRegularExpression>

class SystemMonitor : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mxlinux.UpdaterSystemMonitor")

public:
    explicit SystemMonitor(bool requireChecksum = true);

public Q_SLOTS:
    QString GetState();
    void Refresh();
    void SetIdleTimeout(int seconds);

Q_SIGNALS:
    void stateChanged(const QString& state);

private Q_SLOTS:
    void refresh();
    void checkIdle();

private:
    void refresh(bool syncDb);
    bool syncPacmanDb();
    bool isUpdateAvailable(const QString& pkg);
    QJsonObject buildState(const QStringList& lines);
    QJsonObject parsePacmanConf(const QString& path = QStringLiteral("/etc/pacman.conf"));
    QList<QJsonObject> parseUpdateLines(const QStringList& lines);
    QString getLocalVersion(const QString& pkg);
    QString getSyncVersion(const QString& pkg);
    QString pacmanFieldOutput(const QStringList& args, const QString& field);
    QStringList getGroupPackages(const QString& group);
    QStringList getReplacedPackages(const QString& pkg);
    QStringList runPacmanQuery();
    void touch();

    bool requireChecksum;
    QJsonObject state;
    QTimer* checkTimer;
    QTimer* idleTimer;
    qint64 lastActivity;
    int checkInterval;
    int idleTimeout;
    int pendingUpgradeCount;

    static const QRegularExpression UPDATE_RE;
};
