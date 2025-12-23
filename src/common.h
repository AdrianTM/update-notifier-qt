#ifndef COMMON_H
#define COMMON_H

#include <QSettings>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QVariant>
#include <QStringList>
#include <unistd.h>

const QString APP_ORG = QStringLiteral("MX-Linux");
const QString APP_NAME = QStringLiteral("mx-arch-updater");

const QString ENV_ROOT = QStringLiteral("MX_ARCH_UPDATER_PATH");

const QString STATE_DIR_PATH = QStringLiteral("/var/lib/mx-arch-updater");
const QString STATE_FILE_PATH = STATE_DIR_PATH + QStringLiteral("/state.json");

const QString DEFAULT_DATA_ROOT_PATH = QStringLiteral("/usr/share/mx-arch-updater");
const QString DEFAULT_HELPER_ROOT_PATH = QStringLiteral("/usr/lib/mx-arch-updater");

const int DEFAULT_CHECK_INTERVAL = 60 * 30;
const int DEFAULT_IDLE_TIMEOUT = 4 * 60;

const QStringList ICON_THEMES = {QStringLiteral("wireframe-dark"), QStringLiteral("wireframe-light"), QStringLiteral("classic"), QStringLiteral("pulse")};
const QStringList UPGRADE_MODES = {QStringLiteral("basic"), QStringLiteral("full")};

void ensureNotRoot();
QSettings* settings();
QJsonObject defaultState();
QString stateChecksum(const QJsonObject& state);
void writeState(const QJsonObject& state, const QString& path = STATE_FILE_PATH);
QJsonObject readState(const QString& path = STATE_FILE_PATH, bool requireChecksum = true);
QString envRoot();
QString iconPath(const QString& theme, const QString& name);
QString helperPath(const QString& name);
QVariant readSetting(const QString& key, const QVariant& defaultValue = QVariant());
void writeSetting(const QString& key, const QVariant& value);

#endif // COMMON_H