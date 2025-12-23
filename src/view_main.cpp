#include <QApplication>
#include "view_and_upgrade.h"
#include "common.h"

int main(int argc, char *argv[]) {
    ensureNotRoot();

    QApplication app(argc, argv);
    ViewAndUpgrade dialog;
    dialog.show();
    return app.exec();
}