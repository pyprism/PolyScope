#include "core/SmokeTest.h"

#include "adb/CommandRunner.h"
#include "core/AppContext.h"
#include "core/Logging.h"
#include "core/ToolPaths.h"
#include "models/DeviceModel.h"
#include "models/MdnsModel.h"
#include "scrcpy/ScrcpyManager.h"
#include "tools/DeviceActions.h"
#include "tools/DeviceMonitor.h"
#include "tools/FileBrowserModel.h"
#include "tools/LogcatModel.h"
#include "tools/PackageModel.h"
#include "tools/ServicesModel.h"
#include "tools/ShellConsole.h"

#include <QCoreApplication>
#include <QSet>
#include <QTimer>

namespace polyscope {

namespace {
constexpr auto kFakeSerial = "POLYSCOPE_NO_SUCH_DEVICE";
}

SmokeTest::SmokeTest(AppContext* context, QObject* parent) : QObject(parent), m_context(context) {}

void SmokeTest::check(const QString& name, bool ok, const QString& detail) {
    if (!ok) {
        ++m_failures;
    }
    m_report << QStringLiteral("%1 %2%3").arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), name,
                                              detail.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(detail));
}

void SmokeTest::run() {
    const QString fake = QString::fromLatin1(kFakeSerial);
    auto* tools = m_context->tools();
    auto* commands = m_context->commands();

    // ---- results gathered asynchronously; evaluated after a fixed delay -------------------
    struct Result {
        bool ok = false;
        QString output;
        int exitCode = 0;
    };
    auto results = std::make_shared<QHash<int, Result>>();
    auto ids = std::make_shared<QSet<int>>();
    auto orderingViolations = std::make_shared<int>(0);
    connect(commands, &adb::CommandRunner::finished, this,
            [results, ids, orderingViolations](int id, const QString&, const QString&, bool ok, const QString& out, int code) {
                if (!ids->contains(id)) {
                    ++*orderingViolations; // result delivered before run() returned its id
                }
                results->insert(id, {ok, out, code});
            });
    const int versionId = commands->run(QStringLiteral("server.version"), QString(), {});
    ids->insert(versionId);
    const int noDeviceId = commands->run(QStringLiteral("sys.ps"), QString(), {});
    ids->insert(noDeviceId);
    const int fakeId = commands->run(QStringLiteral("conn.get-state"), fake, {});
    ids->insert(fakeId);
    const int badParamId = commands->run(QStringLiteral("input.tap"), fake, {{QStringLiteral("x"), QStringLiteral("abc")}});
    ids->insert(badParamId);
    const int rawId = commands->runRaw(QString(), QStringLiteral("adb version"));
    ids->insert(rawId);

    // ---- scrcpy ---------------------------------------------------------------------------
    auto* scrcpy = m_context->scrcpy();
    auto scrcpyFinished = std::make_shared<int>(0);
    auto scrcpyExit = std::make_shared<int>(0);
    connect(scrcpy, &ScrcpyManager::finished, this, [scrcpyFinished, scrcpyExit](const QString&, int code, const QString&) {
        ++*scrcpyFinished;
        *scrcpyExit = code;
    });
    QString scrcpyStartError = QStringLiteral("skipped");
    if (tools->scrcpyReady()) {
        scrcpyStartError = scrcpy->start(fake, {});
    }

    // ---- models --------------------------------------------------------------------------
    auto* files = new FileBrowserModel(this);
    files->setSerial(fake);
    auto* packages = new PackageModel(this);
    auto packageResult = std::make_shared<int>(-1);
    connect(packages, &PackageModel::operationFinished, this, [packageResult](const QString&, bool ok, const QString&) {
        *packageResult = ok ? 1 : 0;
    });
    packages->setSerial(fake);
    auto* services = new ServicesModel(this);
    auto serviceResult = std::make_shared<int>(-1);
    connect(services, &ServicesModel::operationFinished, this, [serviceResult](const QString&, bool ok, const QString&) {
        *serviceResult = ok ? 1 : 0;
    });
    services->setSerial(fake);

    auto* logcat = new LogcatModel(this);
    logcat->setSerial(fake);
    logcat->start();
    const bool runningAfterStart = logcat->running();
    logcat->setBuffers(QStringLiteral("main")); // restart while running
    const bool runningAfterRestart = logcat->running();
    logcat->stop();
    logcat->start(); // must start immediately after stop()
    const bool runningAfterStopStart = logcat->running();

    auto* shell = new ShellConsole(this);
    auto shellOutput = std::make_shared<QString>();
    connect(shell, &ShellConsole::output, this, [shellOutput](const QString& t) { *shellOutput += t; });
    shell->setSerial(fake);
    shell->start();

    auto* mdns = new MdnsModel(this);
    mdns->refresh();

    auto actionResult = std::make_shared<int>(-1);
    connect(m_context->actions(), &DeviceActions::operationFinished, this,
            [actionResult](const QString& op, bool ok, const QString&) {
                if (op == QStringLiteral("screenshot")) {
                    *actionResult = ok ? 1 : 0;
                }
            });
    m_context->actions()->screenshot(fake);

    // ---- DeviceModel (canned listing) ------------------------------------------------------
    auto* devices = m_context->devices();
    devices->applyListing(QStringLiteral("AAA device usb:1-1 model:Pixel_7 transport_id:1\n"
                                         "192.168.1.5:5555 offline transport_id:2\n"));
    check(QStringLiteral("DeviceModel adds rows"), devices->rowCount() == 2);
    check(QStringLiteral("DeviceModel wireless flag"),
          devices->get(QStringLiteral("192.168.1.5:5555")).value(QStringLiteral("wireless")).toBool());
    devices->applyListing(QStringLiteral("AAA device usb:1-1 model:Pixel_7 transport_id:1\n"));
    check(QStringLiteral("DeviceModel removes rows"), devices->rowCount() == 1);
    devices->applyListing(QString());
    check(QStringLiteral("DeviceModel clears"), devices->rowCount() == 0);

    check(QStringLiteral("tools: adb ready"), tools->adbReady(), tools->adbError());
    check(QStringLiteral("logcat running after start"), runningAfterStart);
    check(QStringLiteral("logcat running after buffer change"), runningAfterRestart);
    check(QStringLiteral("logcat starts right after stop"), runningAfterStopStart);

    QTimer::singleShot(6000, this, [=, this]() {
        check(QStringLiteral("Commands server.version ok"),
              results->value(versionId).ok && results->value(versionId).output.contains(QStringLiteral("Android Debug Bridge")),
              results->value(versionId).output.left(80));
        check(QStringLiteral("Commands device cmd without device fails"), results->contains(noDeviceId) && !results->value(noDeviceId).ok);
        check(QStringLiteral("Commands unknown device fails"), results->contains(fakeId) && !results->value(fakeId).ok,
              results->value(fakeId).output.left(80));
        check(QStringLiteral("Commands bad parameter rejected"), results->contains(badParamId) && !results->value(badParamId).ok);
        check(QStringLiteral("Commands raw runner ok"), results->value(rawId).ok);
        check(QStringLiteral("Commands results arrive after run() returns"), *orderingViolations == 0);
        if (tools->scrcpyReady()) {
            check(QStringLiteral("scrcpy start returns no error"), scrcpyStartError.isEmpty(), scrcpyStartError);
            check(QStringLiteral("scrcpy exits for unknown device"), *scrcpyFinished == 1 && *scrcpyExit != 0,
                  QStringLiteral("finished=%1 exit=%2").arg(*scrcpyFinished).arg(*scrcpyExit));
            check(QStringLiteral("scrcpy session cleaned up"), scrcpy->runningSerials().isEmpty());
            check(QStringLiteral("scrcpy log captured"), !scrcpy->log(QString::fromLatin1(kFakeSerial)).isEmpty());
        }
        check(QStringLiteral("FileBrowser reports error"), !files->error().isEmpty());
        check(QStringLiteral("PackageModel reports failure"), *packageResult == 0);
        check(QStringLiteral("ServicesModel reports failure"), *serviceResult == 0);
        check(QStringLiteral("Shell output received"), !shellOutput->isEmpty());
        check(QStringLiteral("Screenshot failure reported"), *actionResult == 0);
        check(QStringLiteral("Mdns refresh finished"), !mdns->busy());
        evaluate();
    });
}

void SmokeTest::runLive() {
    auto* devices = m_context->devices();
    const QStringList online = devices->onlineSerials();
    if (online.isEmpty()) {
        check(QStringLiteral("live: an online device exists"), false, QStringLiteral("none reported by adb tracker"));
        evaluate();
        return;
    }
    const QString serial = online.first();
    qCInfo(lcTools).noquote() << "SELFTEST live device" << serial;
    check(QStringLiteral("live: tracker lists device"), true, serial);

    devices->refreshDetails(serial);

    auto* commands = m_context->commands();
    auto rawResult = std::make_shared<QString>();
    const int rawId = commands->runRaw(serial, QStringLiteral("shell getprop ro.product.model"));
    connect(commands, &adb::CommandRunner::finished, this,
            [rawResult, rawId](int id, const QString&, const QString&, bool ok, const QString& out, int) {
                if (id == rawId && ok) {
                    *rawResult = out.trimmed();
                }
            });

    auto* files = new FileBrowserModel(this);
    files->setSerial(serial);
    files->cd(QStringLiteral("/sdcard"));

    auto* packages = new PackageModel(this);
    packages->setSerial(serial);
    packages->setScope(QStringLiteral("all"));
    packages->refresh();

    auto* services = new ServicesModel(this);
    services->setSerial(serial);
    services->setMode(QStringLiteral("binder"));
    services->refresh();

    auto sections = std::make_shared<qsizetype>(0);
    connect(m_context->actions(), &DeviceActions::overviewReady, this,
            [sections](const QString&, const QVariantList& list) { *sections = list.size(); });
    m_context->actions()->overview(serial);

    auto* shell = new ShellConsole(this);
    auto shellOutput = std::make_shared<QString>();
    connect(shell, &ShellConsole::output, this, [shellOutput](const QString& t) { *shellOutput += t; });
    shell->setSerial(serial);
    shell->start();
    QTimer::singleShot(1000, shell, [shell]() { shell->send(QStringLiteral("echo polyscope-live-ok")); });

    auto* monitor = new DeviceMonitor(this);
    monitor->setSerial(serial);
    monitor->setActive(true);
    monitor->refreshStorage();
    auto details = std::make_shared<QVariantMap>();
    auto stopOk = std::make_shared<int>(-1);
    connect(monitor, &DeviceMonitor::packageDetailsReady, this,
            [details](const QString&, const QVariantMap& d) { *details = d; });
    connect(monitor, &DeviceMonitor::actionFinished, this,
            [stopOk](const QString&, bool ok, const QString&) { *stopOk = ok ? 1 : 0; });
    QTimer::singleShot(2500, monitor, [monitor]() {
        monitor->loadPackageDetails(QStringLiteral("com.android.settings"));
        monitor->packageAction(QStringLiteral("com.android.settings"), QStringLiteral("force-stop"));
    });

    auto* logcat = new LogcatModel(this);
    logcat->setSerial(serial);
    logcat->start();

    auto* scrcpy = m_context->scrcpy();
    const QString scrcpyError = m_context->tools()->scrcpyReady() ? scrcpy->start(serial, {}) : QStringLiteral("skipped");

    auto mirroring = std::make_shared<bool>(false);
    QTimer::singleShot(9000, this, [=]() { *mirroring = scrcpy->isRunning(serial); });
    QTimer::singleShot(9500, this, [scrcpy, serial]() { scrcpy->stop(serial); });

    QTimer::singleShot(13000, this, [=, this]() {
        const QVariantMap info = devices->get(serial);
        check(QStringLiteral("live: device details loaded"), info.value(QStringLiteral("detailsLoaded")).toBool(),
              info.value(QStringLiteral("manufacturer")).toString() + QLatin1Char(' ') + info.value(QStringLiteral("name")).toString());
        check(QStringLiteral("live: battery read"), info.value(QStringLiteral("batteryLevel")).toInt() >= 0,
              QString::number(info.value(QStringLiteral("batteryLevel")).toInt()));
        check(QStringLiteral("live: raw getprop via Commands"), !rawResult->isEmpty(), *rawResult);
        check(QStringLiteral("live: /sdcard listing"), files->error().isEmpty() && files->rowCount() > 0,
              QStringLiteral("%1 rows %2").arg(files->rowCount()).arg(files->error()));
        check(QStringLiteral("live: packages listed"), packages->totalCount() > 0, QString::number(packages->totalCount()));
        check(QStringLiteral("live: services listed"), services->totalCount() > 0, QString::number(services->totalCount()));
        check(QStringLiteral("live: overview sections"), *sections >= 2, QString::number(*sections));
        check(QStringLiteral("live: shell round trip"), shellOutput->contains(QStringLiteral("polyscope-live-ok")));
        check(QStringLiteral("live: monitor CPU history"), monitor->cpuHistory().size() >= 2, QString::number(monitor->cpuHistory().size()));
        check(QStringLiteral("live: monitor memory"), monitor->memTotalKb() > 0 && monitor->memUsedPercent() > 0,
              QString::number(monitor->memUsedPercent(), 'f', 1));
        check(QStringLiteral("live: monitor battery"), monitor->batteryLevel() >= 0);
        check(QStringLiteral("live: monitor top processes"), !monitor->topCpu().isEmpty() && !monitor->topMem().isEmpty());
        check(QStringLiteral("live: monitor storage breakdown"),
              !monitor->storage().value(QStringLiteral("categories")).toList().isEmpty() && !monitor->storageBusy(),
              monitor->storage().value(QStringLiteral("error")).toString());
        check(QStringLiteral("live: monitor battery temperature/voltage history"),
              !monitor->batteryTempHistory().isEmpty() && !monitor->batteryVoltageHistory().isEmpty());
        check(QStringLiteral("live: package details"), details->value(QStringLiteral("found")).toBool(),
              details->value(QStringLiteral("versionName")).toString());
        check(QStringLiteral("live: package force-stop"), *stopOk == 1);
        monitor->setActive(false);
        check(QStringLiteral("live: logcat streamed"), logcat->totalCount() > 0, QString::number(logcat->totalCount()));
        check(QStringLiteral("live: scrcpy started"), scrcpyError.isEmpty(), scrcpyError);
        check(QStringLiteral("live: scrcpy still running after 9s"), *mirroring, scrcpy->log(serial).right(200));
        check(QStringLiteral("live: scrcpy stopped cleanly"), !scrcpy->isRunning(serial));
        logcat->stop();
        shell->stop();
        evaluate();
    });
}

void SmokeTest::evaluate() {
    for (const QString& line : std::as_const(m_report)) {
        qCInfo(lcTools).noquote() << "SELFTEST" << line;
    }
    qCInfo(lcTools).noquote() << "SELFTEST" << (m_failures == 0 ? QStringLiteral("ALL PASSED") : QStringLiteral("%1 FAILED").arg(m_failures));
    QCoreApplication::exit(m_failures == 0 ? 0 : 1);
}

} // namespace polyscope
