#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>

int main(int argc, char *argv[])
{
    qputenv("QSG_RHI_BACKEND", "opengl");

    QGuiApplication app(argc, argv);

    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QQmlApplicationEngine engine;

    engine.loadFromModule("QtUi", "App");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}