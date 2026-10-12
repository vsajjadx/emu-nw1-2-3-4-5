#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("EmuLauncher");
    QCoreApplication::setApplicationName("emu-launcher");

    MainWindow w;
    w.show();

    // Games passed on the command line (e.g. "Open with" from Explorer) are added to the library.
    QStringList args = app.arguments();
    args.removeFirst();
    if (!args.isEmpty())
        w.addPaths(args);

    return app.exec();
}
