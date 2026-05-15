#include <QApplication>
#include "sysinfowidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    SysInfoWidget w;
    w.showOnPrimaryScreen();

    return app.exec();
}