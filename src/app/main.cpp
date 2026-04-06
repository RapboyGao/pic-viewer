#include "app/main_window.h"

#include <QApplication>
#include <QIcon>
#include <QPalette>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("pic-viewer");
    QApplication::setOrganizationName("pic-viewer");
    QApplication::setOrganizationDomain("pic-viewer.local");
    QApplication::setApplicationVersion("0.1.0");
    app.setWindowIcon(QIcon(":/icons/app_icon.ico"));

    QPalette palette = app.palette();
    palette.setColor(QPalette::Window, QColor(0, 0, 0));
    palette.setColor(QPalette::WindowText, QColor(240, 240, 240));
    palette.setColor(QPalette::Base, QColor(18, 18, 18));
    palette.setColor(QPalette::AlternateBase, QColor(28, 28, 28));
    palette.setColor(QPalette::Text, QColor(240, 240, 240));
    palette.setColor(QPalette::Button, QColor(36, 36, 36));
    palette.setColor(QPalette::ButtonText, QColor(240, 240, 240));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(palette);
    app.setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #000000;
            color: #f0f0f0;
        }
        QMenuBar {
            background: #111111;
            color: #f0f0f0;
        }
        QMenuBar::item:selected {
            background: #2a2a2a;
        }
        QMenu {
            background: #111111;
            color: #f0f0f0;
            border: 1px solid #333333;
        }
        QMenu::item:selected {
            background: #2a2a2a;
        }
        QStatusBar {
            background: #000000;
            color: #f0f0f0;
        }
        QStatusBar QLabel {
            color: #f0f0f0;
        }
        QListWidget {
            background: #000000;
            color: #f0f0f0;
            border: none;
        }
        QListWidget::item {
            color: #f0f0f0;
        }
        QListWidget::item:selected {
            background: #2a82da;
            color: #ffffff;
        }
    )");

    QString startupPath;
    if (argc > 1) {
        startupPath = QString::fromLocal8Bit(argv[1]);
    }

    MainWindow window(startupPath);
    window.show();

    return app.exec();
}
