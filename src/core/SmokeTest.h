#pragma once

#include <QObject>
#include <QStringList>

namespace polyscope {

class AppContext;

// Device-free runtime self-test, enabled with POLYSCOPE_SELFTEST=1. It drives the real
// adb/scrcpy binaries with a serial that cannot exist, so every failure path and
// the async plumbing are exercised without hardware. Exits 0 when all checks pass.
class SmokeTest : public QObject {
    Q_OBJECT
public:
    explicit SmokeTest(AppContext* context, QObject* parent = nullptr);
    void run();
    // POLYSCOPE_SELFTEST=live: read-only checks against the first online device, plus a short mirror.
    void runLive();

private:
    void check(const QString& name, bool ok, const QString& detail = QString());
    void evaluate();

    AppContext* m_context;
    QStringList m_report;
    int m_failures = 0;
};

} // namespace polyscope
