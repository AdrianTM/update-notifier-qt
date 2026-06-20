#pragma once

#include <QDialog>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
#include <QProcess>
#include <QDBusInterface>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QShowEvent>
#include <QCheckBox>
#include <QProgressBar>
#include <QStackedLayout>
#include <QTimer>

class ViewAndUpgrade : public QDialog {
    Q_OBJECT

public:
    explicit ViewAndUpgrade(QWidget* parent = nullptr);
    ~ViewAndUpgrade();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private Q_SLOTS:
    void refresh();
    void upgrade();
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
    QProcess* upgradeProcess;
    QTimer* refreshTimer = nullptr;
    bool suppressItemChanged = false;
};
