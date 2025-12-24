#include "tray_service.h"
#include "tray_app.h"

TrayService::TrayService(TrayApp* trayApp)
    : QObject(trayApp)
    , trayApp(trayApp)
{
}

void TrayService::ShowView() {
    trayApp->openView();
}

void TrayService::ShowSettings() {
    trayApp->openSettings();
}

void TrayService::Refresh() {
    trayApp->refresh();
}

void TrayService::Quit() {
    trayApp->quit();
}