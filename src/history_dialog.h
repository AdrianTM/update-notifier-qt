#pragma once

#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPushButton>

class HistoryDialog : public QDialog {
  Q_OBJECT

public:
  explicit HistoryDialog(QWidget *parent = nullptr);

private:
  void loadHistory();

  QTextEdit *historyText;
};
