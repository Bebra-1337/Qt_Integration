#include <QApplication>
#include <QSurfaceFormat>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    // xcb — обязательно для XReparentWindow на Wayland машинах
    qputenv("QT_QPA_PLATFORM", "xcb");

    QSurfaceFormat fmt;
    fmt.setVersion(4, 5);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("GodotQtHost");
    app.setOrganizationName("dev");

    MainWindow w;
    w.show();

    return app.exec();
}
