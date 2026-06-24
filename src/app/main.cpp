#include "app/MainWindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Owera MindFlow"));
    QApplication::setOrganizationName(QStringLiteral("Owera"));
    QApplication::setDesktopFileName(QStringLiteral("com.owera.MindFlow"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/mindflow.svg")));

    mindflow::MainWindow window;
    window.show();
    return app.exec();
}
