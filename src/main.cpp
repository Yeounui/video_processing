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

    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "software");
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
