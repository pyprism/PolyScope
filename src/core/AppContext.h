#pragma once

#include <QObject>

namespace polyscope {

namespace adb {
class AdbClient;
class AdbTracker;
class CommandRunner;
} // namespace adb
class AppSettings;
class DeviceActions;
class DeviceModel;
class ScrcpyManager;
class ToolPaths;

// Owns the long-lived services. QML singletons and instantiable models fetch their
// dependencies from here instead of using globals scattered across files.
class AppContext : public QObject {
    Q_OBJECT
public:
    explicit AppContext(QObject* parent = nullptr);
    ~AppContext() override;

    static AppContext* instance();

    ToolPaths* tools() const { return m_tools; }
    AppSettings* settings() const { return m_settings; }
    adb::AdbClient* adb() const { return m_adb; }
    adb::AdbTracker* tracker() const { return m_tracker; }
    DeviceModel* devices() const { return m_devices; }
    ScrcpyManager* scrcpy() const { return m_scrcpy; }
    DeviceActions* actions() const { return m_actions; }
    adb::CommandRunner* commands() const { return m_commands; }

    // Begin tool discovery and device tracking.
    void start();
    // Stop child processes (called from aboutToQuit).
    void shutdown();

private:
    static AppContext* s_instance;

    ToolPaths* m_tools;
    AppSettings* m_settings;
    adb::AdbClient* m_adb;
    adb::AdbTracker* m_tracker;
    DeviceModel* m_devices;
    ScrcpyManager* m_scrcpy;
    DeviceActions* m_actions;
    adb::CommandRunner* m_commands;
};

} // namespace polyscope
