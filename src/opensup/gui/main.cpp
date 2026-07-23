#include <QApplication>
#include "main_window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("OpenSUP");
    app.setApplicationVersion("1.0.0");

    MainWindow window;
    window.setWindowTitle("OpenSUP v1.0.0 - PGS Subtitle Encoder");
    window.show();

    return app.exec();
}
