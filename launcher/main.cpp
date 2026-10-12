#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("EmuLauncher");
    QCoreApplication::setApplicationName("emu-launcher");

    MainWindow w;
    w.show();
    return app.exec();
}
