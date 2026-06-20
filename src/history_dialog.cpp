#include "history_dialog.h"
#include "common.h"
#include <QFile>
#include <QTextStream>
#include <QDialogButtonBox>
#include <QLabel>

HistoryDialog::HistoryDialog(QWidget *parent)
    : QDialog(parent), historyText(new QTextEdit(this)) {
  setWindowTitle(QStringLiteral("Package History"));
  QString iconPath =
      ::iconPath(QStringLiteral(""), QStringLiteral("update-notifier-settings.svg"));
  if (QFile::exists(iconPath)) {
    setWindowIcon(QIcon(iconPath));
  }
  resize(800, 600);

  historyText->setReadOnly(true);
  historyText->setLineWrapMode(QTextEdit::NoWrap);
  historyText->setFontFamily(QStringLiteral("monospace"));

  QDialogButtonBox *buttons =
      new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->addWidget(
      new QLabel(QStringLiteral("Recent package transactions:"), this));
  layout->addWidget(historyText);
  layout->addWidget(buttons);

  loadHistory();
}

void HistoryDialog::loadHistory() {
  QFile logFile(QStringLiteral("/var/log/pacman.log"));
  if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    historyText->setPlainText(
        QStringLiteral("Unable to open pacman log file: /var/log/pacman.log"));
    return;
  }

  QTextStream in(&logFile);
  QStringList lines;
  lines.reserve(501);
  constexpr int MAX_LINES = 500;

  // Read lines and maintain a ring buffer of the last 500 matching entries
  while (!in.atEnd()) {
    QString line = in.readLine();
    if (line.contains(QStringLiteral(" installed ")) ||
        line.contains(QStringLiteral(" upgraded ")) ||
        line.contains(QStringLiteral(" removed "))) {
      lines.append(line);
      if (lines.size() > MAX_LINES) {
        lines.removeFirst();
      }
    }
  }

  historyText->setPlainText(lines.join(QStringLiteral("\n")));

  // Scroll to the bottom to show most recent entries
  QTextCursor cursor = historyText->textCursor();
  cursor.movePosition(QTextCursor::End);
  historyText->setTextCursor(cursor);
}
