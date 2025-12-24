#include "common.h"
#include <QDebug>
#include <QJsonArray>
#include <QDateTime>

void ensureNotRoot() {
    if (geteuid() == 0) {
        qCritical() << QStringLiteral("This application must run in a user session, not as root.");
        QCoreApplication::exit(1);
    }
}

namespace {
    QSettings* globalSettings = nullptr;

    void cleanupSettings() {
        if (globalSettings) {
            delete globalSettings;
            globalSettings = nullptr;
        }
    }
}

QSettings* settings() {
    if (!globalSettings) {
        globalSettings = new QSettings(APP_ORG, APP_NAME);
        qAddPostRoutine(cleanupSettings);
    }
    return globalSettings;
}

QJsonObject defaultState() {
    QJsonObject state;
    state[QStringLiteral("checked_at")] = 0;
    QJsonObject counts;
    counts[QStringLiteral("upgrade")] = 0;
    counts[QStringLiteral("new")] = 0;
    counts[QStringLiteral("remove")] = 0;
    counts[QStringLiteral("held")] = 0;
    state[QStringLiteral("counts")] = counts;
    state[QStringLiteral("packages")] = QJsonArray();
    state[QStringLiteral("errors")] = QJsonArray();
    state[QStringLiteral("status")] = QStringLiteral("idle");
    return state;
}

QString stateChecksum(const QJsonObject& state) {
    QJsonDocument doc(state);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

void writeState(const QJsonObject& state, const QString& path) {
    QDir dir(QFileInfo(path).absolutePath());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QJsonObject payload;
    payload[QStringLiteral("state")] = state;
    payload[QStringLiteral("checksum")] = stateChecksum(state);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(payload);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

QJsonObject readState(const QString& path, bool requireChecksum) {
    QFile file(path);
    if (!file.exists()) {
        return defaultState();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return defaultState();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        return defaultState();
    }

    QJsonObject payload = doc.object();
    QJsonObject state = payload[QStringLiteral("state")].toObject();
    QString checksum = payload[QStringLiteral("checksum")].toString();

    if (state.isEmpty()) {
        return defaultState();
    }

    if (requireChecksum && checksum != stateChecksum(state)) {
        return defaultState();
    }

    return state;
}

QString envRoot() {
    QString root = qEnvironmentVariable(ENV_ROOT.toUtf8().constData());
    if (!root.isEmpty()) {
        return root;
    }
    if (QDir(DEFAULT_DATA_ROOT_PATH).exists()) {
        return DEFAULT_DATA_ROOT_PATH;
    }
    qCritical() << ENV_ROOT << QStringLiteral("is not set; run via bin launchers.");
    QCoreApplication::exit(1);
    return QString();
}

QString iconPath(const QString& theme, const QString& name) {
    // For icons, always use the system-installed location
    QString root = envRoot();
    QStringList candidates;
    candidates << theme;
    for (const QString& candidate : ICON_THEMES) {
        if (!candidates.contains(candidate)) {
            candidates << candidate;
        }
    }

    QStringList tried;
    for (const QString& candidate : candidates) {
        QString candidatePath = root + QStringLiteral("/icons/") + candidate + QStringLiteral("/") + name;
        tried << candidatePath;
        if (QFile::exists(candidatePath)) {
            return candidatePath;
        }
    }

    qWarning().nospace() << "iconPath: icon '" << name << "' missing; tried " << tried;
    // Return the first candidate as fallback, even if it doesn't exist
    return root + QStringLiteral("/icons/") + theme + QStringLiteral("/") + name;
}

QString helperPath(const QString& name) {
    QString root = envRoot();
    QString candidate = root + QStringLiteral("/lib/mx-arch-updater/") + name;
    if (QFile::exists(candidate)) {
        return candidate;
    }
    return DEFAULT_HELPER_ROOT_PATH + QStringLiteral("/") + name;
}

QVariant readSetting(const QString& key, const QVariant& defaultValue) {
    return settings()->value(key, defaultValue);
}

void writeSetting(const QString& key, const QVariant& value) {
    QSettings* s = settings();
    s->setValue(key, value);
    s->sync();
}
