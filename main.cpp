#include <stdio.h>
#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{

    QApplication app(argc, argv);
    new MainWindow();
    app.exec();

    return 0;
}
