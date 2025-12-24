#ifndef VIEW_AND_UPGRADE_H
#define VIEW_AND_UPGRADE_H

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QProgressDialog>
#include <QProcess>
#include <QDBusInterface>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>

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

private:
    void buildUi();
    void setupDBus();

    QLabel* countsLabel;
    QListWidget* listWidget;
    QPushButton* buttonRefresh;
    QPushButton* buttonUpgrade;
    QPushButton* buttonClose;

    QDBusInterface* iface;
    QProgressDialog* progressDialog;
    QProcess* upgradeProcess;
};

#endif // VIEW_AND_UPGRADE_H