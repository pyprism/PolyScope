#include "core/AppContext.h"

#include "adb/AdbClient.h"
#include "adb/AdbTracker.h"
#include "adb/CommandRunner.h"
#include "core/AppSettings.h"
#include "core/ToolPaths.h"
#include "models/DeviceModel.h"
#include "scrcpy/ScrcpyManager.h"
#include "tools/DeviceActions.h"

namespace polyscope {

AppContext* AppContext::s_instance = nullptr;

AppContext::AppContext(QObject* parent) : QObject(parent) {
    s_instance = this;
    m_tools = new ToolPaths(this);
    m_settings = new AppSettings(this);
    m_adb = new adb::AdbClient(m_tools, this);
    m_tracker = new adb::AdbTracker(m_adb, this);
    m_devices = new DeviceModel(m_adb, m_tracker, this);
    m_scrcpy = new ScrcpyManager(m_tools, m_settings, this);
    m_actions = new DeviceActions(m_adb, m_settings, this);
    m_commands = new adb::CommandRunner(m_adb, m_settings, this);

    // (Re)start tracking whenever the adb binary changes.
    connect(m_tools, &ToolPaths::adbChanged, this, [this]() {
        if (m_tools->adbReady()) {
            m_tracker->restart();
        } else {
            m_tracker->stop();
        }
    });
}

AppContext::~AppContext() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

AppContext* AppContext::instance() {
    return s_instance;
}

void AppContext::start() {
    m_tools->refresh();
}

void AppContext::shutdown() {
    m_tracker->stop();
    m_scrcpy->shutdown();
}

} // namespace polyscope
