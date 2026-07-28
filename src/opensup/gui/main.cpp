#include <QApplication>
#include "main_window.h"
#include "theme_manager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("OpenSUP");
    app.setApplicationVersion("1.0.0");

    ThemeManager theme;  // Detect and apply system theme before window shows

    MainWindow window;
    window.show();

    return app.exec();
}
