#pragma once

#include <array>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QStringView>
#include <QVariant>
#include <unistd.h>

const QString APP_ORG = QStringLiteral("MX-Linux");
const QString APP_NAME = QStringLiteral("mx-arch-updater");
const QString APP_VERSION = QStringLiteral("25.12-1");
const QString ENV_ROOT = QStringLiteral("MX_ARCH_UPDATER_PATH");
const QString STATE_DIR_PATH = QStringLiteral("/var/lib/mx-arch-updater");
const QString STATE_FILE_PATH = STATE_DIR_PATH + QStringLiteral("/state.json");
const QString DEFAULT_DATA_ROOT_PATH =
    QStringLiteral("/usr/share/mx-arch-updater");

const int DEFAULT_CHECK_INTERVAL = 60 * 60; // 60 minutes
const int DEFAULT_IDLE_TIMEOUT = 4 * 60;

inline constexpr std::array<QLatin1StringView, 8> ICON_THEMES = {
    QLatin1StringView("wireframe-dark"), QLatin1StringView("wireframe-light"),
    QLatin1StringView("black-red"),      QLatin1StringView("green-black"),
    QLatin1StringView("modern"),         QLatin1StringView("modern-light"),
    QLatin1StringView("pulse"),          QLatin1StringView("pulse-light")};
inline constexpr std::array<QLatin1StringView, 2> UPGRADE_MODES = {
    QLatin1StringView("standard"),
    QLatin1StringView("include AUR updates")};

QJsonObject defaultState();
QJsonObject readState(const QString &path = STATE_FILE_PATH,
                      bool requireChecksum = true);
QSettings *settings();
QString envRoot();
QString iconPath(const QString &theme, const QString &name);
bool isKnownIconTheme(QStringView theme);
QString stateChecksum(const QJsonObject &state);
QString getDesktopFileName(const QString &executable);
QVariant readSetting(const QString &key,
                     const QVariant &defaultValue = QVariant());
void ensureNotRoot();
void writeSetting(const QString &key, const QVariant &value);
void writeState(const QJsonObject &state,
                const QString &path = STATE_FILE_PATH);
