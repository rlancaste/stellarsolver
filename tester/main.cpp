#include <stdio.h>
#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    MainWindow *win = new MainWindow();
    app.exec();
    
    delete win;

    return 0;
}
