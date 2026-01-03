#pragma once

#include <QDialog>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
#include <QProgressDialog>
#include <QProcess>
#include <QDBusInterface>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QCheckBox>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QProgressBar>
#include <QStackedLayout>

class ViewAndUpgrade : public QDialog {
    Q_OBJECT

public:
    explicit ViewAndUpgrade(QWidget* parent = nullptr);
    ~ViewAndUpgrade();

protected:
    void closeEvent(QCloseEvent* event) override;

private Q_SLOTS:
    void refresh();
    void upgrade();
    void onUpgradeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onUpgradeError(QProcess::ProcessError error);
    void onUpgradeOutput();
    void onFirstOutput();
    void onUpgradeCancel();
    void onSelectAllToggled(bool checked);
    void onTreeItemChanged(QTreeWidgetItem* item, int column);

private:
    bool launchInTerminal(const QString& command, const QStringList& args, QProcess** monitorProcess = nullptr);
    void loadState();
    void applyState(const QString& payload);
    void setRefreshing(bool refreshing);

private:
    void buildUi();
    void setupDBus();

    QLabel* countsLabel;
    QProgressBar* refreshProgress;
    QStackedLayout* statusLayout;
    QCheckBox* selectAllCheckbox;
    QTreeWidget* treeWidget;
    QPushButton* buttonRefresh;
    QPushButton* buttonUpgrade;
    QPushButton* buttonClose;

    QDBusInterface* iface;
    QDBusInterface* trayIface;
    QProgressDialog* progressDialog;
    QProcess* upgradeProcess;

    // Upgrade dialog components
    QDialog* upgradeDialog;
    QTextEdit* upgradeOutput;
    QDialogButtonBox* upgradeButtons;
};
