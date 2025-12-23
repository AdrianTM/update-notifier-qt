#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

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
    void touch();
    QStringList runPacmanQuery();
    QJsonObject buildState(const QStringList& lines);
    QList<QJsonObject> parseUpdateLines(const QStringList& lines);
    QJsonObject parsePacmanConf(const QString& path = QStringLiteral("/etc/pacman.conf"));
    QString pacmanFieldOutput(const QStringList& args, const QString& field);
    QString getLocalVersion(const QString& pkg);
    QString getSyncVersion(const QString& pkg);
    bool isUpdateAvailable(const QString& pkg);
    QStringList getReplacedPackages(const QString& pkg);
    QStringList getGroupPackages(const QString& group);

    bool requireChecksum;
    QJsonObject state;
    QTimer* checkTimer;
    QTimer* idleTimer;
    qint64 lastActivity;
    int checkInterval;
    int idleTimeout;

    static const QRegularExpression UPDATE_RE;
};

#endif // SYSTEM_MONITOR_H