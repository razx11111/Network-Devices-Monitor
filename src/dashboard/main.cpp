#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setStyle("Fusion"); // Better look on Linux
    
    MainWindow w;
    w.show();
    
    return a.exec();
}