#include "core/AppContext.h"
#include "core/Logging.h"
#include "core/SmokeTest.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <memory>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("PolyScope"));
    QGuiApplication::setApplicationName(QStringLiteral("PolyScope"));
    QGuiApplication::setApplicationVersion(QStringLiteral(POLYSCOPE_VERSION));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app-256.png")));

    // Basic is the one style that honours custom palettes on every platform; the
    // native macOS/Windows styles ignore them and would break our theme.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    polyscope::AppContext context;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &context, &polyscope::AppContext::shutdown);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("PolyScope", "Main");

    context.start();

    // POLYSCOPE_SELFTEST=1 runs a device-free runtime check and exits (see SmokeTest).
    if (qEnvironmentVariableIntValue("POLYSCOPE_SELFTEST") == 1) {
        auto* smoke = new polyscope::SmokeTest(&context, &app);
        // Give tool discovery (async --version checks) time to finish first.
        QTimer::singleShot(1500, smoke, &polyscope::SmokeTest::run);
    } else if (qEnvironmentVariable("POLYSCOPE_SELFTEST") == QStringLiteral("live")) {
        auto* smoke = new polyscope::SmokeTest(&context, &app);
        QTimer::singleShot(3000, smoke, &polyscope::SmokeTest::runLive);
    }
    // POLYSCOPE_SCREENSHOT=<dir>: render every navigation page to <dir>/page-N.png, then quit.
    // Dev aid for checking the UI without capturing the desktop.
    const QString shotDir = qEnvironmentVariable("POLYSCOPE_SCREENSHOT");
    if (!shotDir.isEmpty()) {
        auto page = std::make_shared<int>(0);
        auto* timer = new QTimer(&app);
        timer->setInterval(3500);
        QObject::connect(timer, &QTimer::timeout, &app, [&engine, shotDir, page]() {
            auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            QObject* nav = window ? window->findChild<QObject*>(QStringLiteral("nav")) : nullptr;
            if (!window || !nav || *page > 9) {
                QCoreApplication::quit();
                return;
            }
            if (*page == 0) {
                window->resize(qEnvironmentVariableIntValue("POLYSCOPE_SCREENSHOT_WIDTH") > 0
                                   ? qEnvironmentVariableIntValue("POLYSCOPE_SCREENSHOT_WIDTH") : 1180,
                               qEnvironmentVariableIntValue("POLYSCOPE_SCREENSHOT_HEIGHT") > 0
                                         ? qEnvironmentVariableIntValue("POLYSCOPE_SCREENSHOT_HEIGHT") : 760);
            }
            if (*page == 3) {
                int n = 0;
                for (QWindow* w : QGuiApplication::allWindows()) {
                    if (auto* q = qobject_cast<QQuickWindow*>(w); q && q != window && q->isVisible()) {
                        q->grabWindow().save(QStringLiteral("%1/window-%2.png").arg(shotDir).arg(n++));
                    }
                }
            }
            if (*page > 0) {
                window->grabWindow().save(QStringLiteral("%1/page-%2.png").arg(shotDir).arg(*page - 1));
            }
            if (*page <= 9) {
                nav->setProperty("currentIndex", *page);
            }
            ++*page;
            if (*page == 11) {
                QCoreApplication::quit();
            }
        });
        timer->start();
    }
    return app.exec();
}
