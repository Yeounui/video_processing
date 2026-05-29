#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>

namespace {

void applyWslgRuntimeFix()
{
    const QByteArray waylandDisplay = qgetenv("WAYLAND_DISPLAY");
    if (waylandDisplay.isEmpty())
        return;

    const QString displayName = QString::fromLocal8Bit(waylandDisplay);
    const QString currentRuntimeDir = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
    const QString currentSocket = currentRuntimeDir + QLatin1Char('/') + displayName;
    const QString wslgRuntimeDir = QStringLiteral("/mnt/wslg/runtime-dir");
    const QString wslgSocket = wslgRuntimeDir + QLatin1Char('/') + displayName;

    if (QFileInfo::exists(currentSocket) || !QFileInfo::exists(wslgSocket))
        return;

    qputenv("XDG_RUNTIME_DIR", wslgRuntimeDir.toLocal8Bit());

    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "wayland");

    // Force OpenGL RHI so QSGRenderNode raw-GL calls work (D10 hybrid shader requires GL context).
    // WSLg Mesa d3d12 driver supports OpenGL 4.1+ via D3D12 passthrough.
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "opengl");
}

}

int main(int argc, char *argv[])
{
    applyWslgRuntimeFix();

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    engine.loadFromModule("QtUi", "App");

    if (engine.rootObjects().isEmpty())
        return -1;

    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
        window->showNormal();
        window->raise();
        window->requestActivate();

        QPointer<QQuickWindow> windowPtr(window);
        QTimer::singleShot(100, &app, [windowPtr]() {
            if (!windowPtr)
                return;

            windowPtr->showNormal();
            windowPtr->raise();
            windowPtr->requestActivate();
        });
    }

    return app.exec();
}
