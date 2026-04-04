#include "app/main_window.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("pic-viewer");
    QApplication::setApplicationVersion("0.1.0");

    QString startupPath;
    if (argc > 1) {
        startupPath = QString::fromLocal8Bit(argv[1]);
    }

    MainWindow window(startupPath);
    window.show();

    return app.exec();
}
