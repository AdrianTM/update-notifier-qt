#include "common.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>

void ensureNotRoot() {
  if (geteuid() == 0) {
    qCritical() << QStringLiteral(
        "This application must run in a user session, not as root.");
    QCoreApplication::exit(1);
  }
}



QSettings &settings() {
  static QSettings instance(APP_ORG, APP_NAME);
  return instance;
}

QJsonObject defaultState() {
  QJsonObject state;
  state[QStringLiteral("checked_at")] = 0;
  QJsonObject counts;
  counts[QStringLiteral("upgrade")] = 0;
  counts[QStringLiteral("remove")] = 0;
  counts[QStringLiteral("held")] = 0;
  state[QStringLiteral("counts")] = counts;
  state[QStringLiteral("packages")] = QJsonArray();
  state[QStringLiteral("errors")] = QJsonArray();
  state[QStringLiteral("status")] = QStringLiteral("idle");
  return state;
}

QString stateChecksum(const QJsonObject &state) {
  QJsonDocument doc(state);
  QByteArray payload = doc.toJson(QJsonDocument::Compact);
  return QString::fromUtf8(
      QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

void writeState(const QJsonObject &state, const QString &path) {
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

QJsonObject readState(const QString &path, bool requireChecksum) {
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
  // 1. Check environment variable (manual override)
  QString root = qEnvironmentVariable(ENV_ROOT.toUtf8().constData());
  if (!root.isEmpty()) {
    return root;
  }

  // 2. Auto-detect development mode: if running from build/, use source tree
  QString appDir = QCoreApplication::applicationDirPath();
  QDir buildDir(appDir);
  if (buildDir.dirName() == QStringLiteral("build")) {
    buildDir.cdUp(); // Go to project root
    QString devPath = buildDir.absolutePath();
    QString devIconsPath = devPath + QStringLiteral("/icons");
    if (QDir(devIconsPath).exists()) {
      return devPath;
    }
  }

  // 3. Use production path
  if (QDir(DEFAULT_DATA_ROOT_PATH).exists()) {
    return DEFAULT_DATA_ROOT_PATH;
  }

  qCritical() << "Unable to locate data directory. Tried development and "
                 "production paths.";
  return QString();
}

QString iconPath(const QString &theme, const QString &name) {
  // For icons, always use the system-installed location
  QString root = envRoot();
  QStringList candidates;
  candidates.reserve(static_cast<int>(ICON_THEMES.size()) + 1);
  candidates << theme;
  for (QLatin1StringView candidate : ICON_THEMES) {
    if (QStringView(theme) != candidate) {
      candidates << QString(candidate);
    }
  }

  QStringList tried;
  tried.reserve(candidates.size());
  for (const QString &candidate : candidates) {
    QString candidatePath = root + QStringLiteral("/icons/") + candidate +
                            QStringLiteral("/") + name;
    tried << candidatePath;
    if (QFile::exists(candidatePath)) {
      return candidatePath;
    }
  }

  qWarning().nospace() << "iconPath: icon '" << name << "' missing; tried "
                       << tried;
  // Return the first candidate as fallback, even if it doesn't exist
  return root + QStringLiteral("/icons/") + theme + QStringLiteral("/") + name;
}

bool isKnownIconTheme(QStringView theme) {
  for (QLatin1StringView candidate : ICON_THEMES) {
    if (theme == candidate) {
      return true;
    }
  }
  return false;
}

QVariant readSetting(const QString &key, const QVariant &defaultValue) {
  return settings().value(key, defaultValue);
}

void writeSetting(const QString &key, const QVariant &value) {
  settings().setValue(key, value);
  settings().sync();
}

QString getDesktopFileName(const QString &executable) {
  // Search for .desktop files in standard locations
  QStringList searchPaths = {QStringLiteral("/usr/share/applications"),
                             QStringLiteral("/usr/local/share/applications"),
                             QStringLiteral("~/.local/share/applications")};

  for (const QString &basePath : searchPaths) {
    QString expandedPath = basePath;
    if (expandedPath.startsWith(QStringLiteral("~"))) {
      expandedPath = QDir::homePath() + expandedPath.mid(1);
    }

    QDir dir(expandedPath);
    if (!dir.exists())
      continue;

    // Look for .desktop files
    QStringList filters = {QStringLiteral("*.desktop")};
    QStringList desktopFiles = dir.entryList(filters, QDir::Files);

    for (const QString &desktopFile : desktopFiles) {
      QString desktopPath = dir.absoluteFilePath(desktopFile);
      QFile file(desktopPath);
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        continue;

      QTextStream in(&file);
      QString line;
      while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.startsWith(QStringLiteral("Exec="))) {
          QString execLine = line.mid(5).trimmed();
          // Extract the executable name (first part before space or arguments)
          QStringView execView(execLine);
          qsizetype spaceIndex = execView.indexOf(u' ');
          QString execName =
              (spaceIndex < 0 ? execView : execView.left(spaceIndex))
                  .toString();
          // Remove path if present, keep only basename
          execName = QFileInfo(execName).baseName();

          if (execName == executable ||
              execName == executable + QStringLiteral(".bin")) {
            // Found matching executable, now look for Name=
            in.seek(0); // Reset to beginning
            while (!in.atEnd()) {
              line = in.readLine().trimmed();
              if (line.startsWith(QStringLiteral("Name="))) {
                QString name = line.mid(5).trimmed();
                file.close();
                return name;
              }
            }
          }
        }
      }
      file.close();
    }
  }

  // Fallback: return the executable name with first letter capitalized
  QString fallback = executable;
  if (!fallback.isEmpty()) {
    fallback[0] = fallback[0].toUpper();
  }
  return fallback;
}
