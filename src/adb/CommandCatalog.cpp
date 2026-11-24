#include "adb/CommandCatalog.h"

#include "adb/AdbParsers.h"

#include <QRegularExpression>

namespace polyscope::adb {

namespace {

using Params = QList<CommandParam>;

QString qs(const char* s) {
    return QString::fromUtf8(s);
}

CommandParam textParam(const char* key, const char* label, bool required = true,
                       const char* placeholder = "") {
    CommandParam p;
    p.key = qs(key);
    p.label = qs(label);
    p.required = required;
    p.placeholder = qs(placeholder);
    return p;
}

CommandParam intParam(const char* key, const char* label, const char* def = "",
                      bool required = true) {
    CommandParam p = textParam(key, label, required);
    p.type = QStringLiteral("int");
    p.defaultValue = qs(def);
    return p;
}

CommandParam choiceParam(const char* key, const char* label, const QStringList& choices,
                         const char* def = "", bool required = true) {
    CommandParam p = textParam(key, label, required);
    p.type = QStringLiteral("choice");
    p.choices = choices;
    p.defaultValue = qs(def);
    return p;
}

CommandParam boolParam(const char* key, const char* label, const char* flag, bool def = false) {
    CommandParam p = textParam(key, label, false);
    p.type = QStringLiteral("bool");
    p.flag = qs(flag);
    p.defaultValue = def ? QStringLiteral("true") : QString();
    return p;
}

CommandParam fileParam(const char* key, const char* label, const char* type = "file",
                       bool required = true) {
    CommandParam p = textParam(key, label, required);
    p.type = qs(type);
    return p;
}

CommandParam flagText(const char* key, const char* label, const char* flag,
                      bool required = false) {
    CommandParam p = textParam(key, label, required);
    p.flag = qs(flag);
    return p;
}

class Builder {
public:
    explicit Builder(QList<CommandSpec>& out) : m_out(out) {}

    Builder& group(const char* name) {
        m_group = qs(name);
        return *this;
    }
    // adb <args>
    void host(const char* id, const char* title, const QStringList& tmpl, const Params& params = {},
              bool danger = false, int timeout = 30000) {
        add(id, title, CommandMode::Host, tmpl, params, danger, timeout);
    }
    // adb -s SERIAL <args>
    void dev(const char* id, const char* title, const QStringList& tmpl, const Params& params = {},
             bool danger = false, int timeout = 30000) {
        add(id, title, CommandMode::Device, tmpl, params, danger, timeout);
    }
    // adb -s SERIAL shell "<cmd>"
    void sh(const char* id, const char* title, const char* cmd, const Params& params = {},
            bool danger = false, int timeout = 30000) {
        add(id, title, CommandMode::Shell, {qs(cmd)}, params, danger, timeout);
    }

private:
    void add(const char* id, const char* title, CommandMode mode, const QStringList& tmpl,
             const Params& params, bool danger, int timeout) {
        CommandSpec spec;
        spec.id = qs(id);
        spec.group = m_group;
        spec.title = qs(title);
        spec.mode = mode;
        spec.tmpl = tmpl;
        spec.params = params;
        spec.dangerous = danger;
        spec.timeoutMs = timeout;
        m_out.append(spec);
    }

    QList<CommandSpec>& m_out;
    QString m_group;
};

QStringList L(std::initializer_list<const char*> items) {
    QStringList list;
    for (const char* item : items) {
        list << qs(item);
    }
    return list;
}

QList<CommandSpec> buildCatalog() {
    QList<CommandSpec> list;
    Builder b(list);
    const QStringList enableDisable = L({"enable", "disable"});

    // ---------------------------------------------------------------- Server
    b.group("Server");
    b.host("server.start", "Start server", L({"start-server"}));
    b.host("server.kill", "Kill server", L({"kill-server"}), {}, true);
    b.host("server.version", "Version", L({"version"}));
    b.host("server.devices", "List devices (long)", L({"devices", "-l"}));
    b.host("server.help", "Help", L({"help"}));
    b.host("mdns.check", "mDNS check", L({"mdns", "check"}));
    b.host("mdns.services", "mDNS services", L({"mdns", "services"}));

    // ------------------------------------------------------------ Connection
    b.group("Connection");
    b.host("conn.connect", "Connect (TCP/IP)", L({"connect", "{address}"}),
           {textParam("address", "Host[:port]", true, "192.168.1.20:5555")});
    b.host("conn.disconnect", "Disconnect", L({"disconnect", "{address}"}),
           {textParam("address", "Host[:port] (empty = all)", false)});
    b.host("conn.pair", "Pair (Wireless debugging)", L({"pair", "{address}", "{code}"}),
           {textParam("address", "Host:port", true, "192.168.1.20:37000"),
            textParam("code", "Pairing code")});
    b.host("conn.keygen", "Generate key", L({"keygen", "{file}"}), {fileParam("file", "Key file")});
    b.dev("conn.get-state", "Get state", L({"get-state"}));
    b.dev("conn.get-serialno", "Get serial number", L({"get-serialno"}));
    b.dev("conn.get-devpath", "Get device path", L({"get-devpath"}));
    b.dev("conn.wait-for-device", "Wait for device", L({"wait-for-device"}), {}, false, 120000);
    b.dev("conn.wait-for-state", "Wait for state",
          L({"wait-for-{transport}-{state}"}),
          {choiceParam("transport", "Transport", L({"any", "usb", "local"}), "any"),
           choiceParam("state", "State",
                       L({"device", "recovery", "rescue", "sideload", "bootloader", "disconnect"}),
                       "device")},
          false, 120000);
    b.dev("conn.reconnect", "Reconnect (host side)", L({"reconnect"}));
    b.dev("conn.reconnect-device", "Reconnect (device side)", L({"reconnect", "device"}));
    b.host("conn.reconnect-offline", "Reconnect offline devices", L({"reconnect", "offline"}));
    b.dev("conn.tcpip", "Restart adbd on TCP port", L({"tcpip", "{port}"}),
          {intParam("port", "Port", "5555")});
    b.dev("conn.usb", "Restart adbd on USB", L({"usb"}));
    b.dev("conn.attach", "Attach USB device", L({"attach"}));
    b.dev("conn.detach", "Detach USB device", L({"detach"}));

    // ------------------------------------------------------------ Forwarding
    b.group("Port forwarding");
    b.host("fwd.list", "Forward: list", L({"forward", "--list"}));
    b.dev("fwd.add", "Forward: add", L({"forward", "{*norebind}", "{local}", "{remote}"}),
          {boolParam("norebind", "No rebind", "--no-rebind"),
           textParam("local", "Local", true, "tcp:8000"),
           textParam("remote", "Remote", true, "tcp:8080")});
    b.dev("fwd.remove", "Forward: remove", L({"forward", "--remove", "{local}"}),
          {textParam("local", "Local", true, "tcp:8000")});
    b.dev("fwd.remove-all", "Forward: remove all", L({"forward", "--remove-all"}), {}, true);
    b.dev("rev.list", "Reverse: list", L({"reverse", "--list"}));
    b.dev("rev.add", "Reverse: add", L({"reverse", "{*norebind}", "{remote}", "{local}"}),
          {boolParam("norebind", "No rebind", "--no-rebind"),
           textParam("remote", "Remote", true, "tcp:8080"),
           textParam("local", "Local", true, "tcp:8000")});
    b.dev("rev.remove", "Reverse: remove", L({"reverse", "--remove", "{remote}"}),
          {textParam("remote", "Remote", true, "tcp:8080")});
    b.dev("rev.remove-all", "Reverse: remove all", L({"reverse", "--remove-all"}), {}, true);

    // ----------------------------------------------------------------- Files
    b.group("Files");
    b.dev("file.push", "Push", L({"push", "{*sync}", "{*compress}", "{local}", "{remote}"}),
          {boolParam("sync", "Only newer (--sync)", "--sync"),
           flagText("compress", "Compression algorithm (-z)", "-z"),
           fileParam("local", "Local files", "files"),
           textParam("remote", "Remote path", true, "/sdcard/Download/")},
          false, 0);
    b.dev("file.pull", "Pull", L({"pull", "{*preserve}", "{*compress}", "{remote}", "{local}"}),
          {boolParam("preserve", "Preserve timestamps (-a)", "-a"),
           flagText("compress", "Compression algorithm (-z)", "-z"),
           textParam("remote", "Remote path", true, "/sdcard/DCIM"),
           fileParam("local", "Local folder", "dir")},
          false, 0);
    b.dev("file.sync", "Sync partition", L({"sync", "{*list}", "{partition}"}),
          {boolParam("list", "List only (-l)", "-l"),
           choiceParam("partition", "Partition",
                       L({"all", "data", "odm", "oem", "product", "system", "system_ext", "vendor"}),
                       "all")},
          true, 0);
    b.sh("file.ls", "List directory", "ls -la {path}", {textParam("path", "Path", true, "/sdcard/")});
    b.sh("file.stat", "Stat", "stat {path}", {textParam("path", "Path")});
    b.sh("file.cat", "Show file", "cat {path}", {textParam("path", "Path")});
    b.sh("file.mkdir", "Make directory", "mkdir -p {path}", {textParam("path", "Path")});
    b.sh("file.rm", "Delete", "rm -rf {path}", {textParam("path", "Path")}, true);
    b.sh("file.mv", "Move / rename", "mv {from} {to}",
         {textParam("from", "From"), textParam("to", "To")}, true);
    b.sh("file.cp", "Copy", "cp -r {from} {to}", {textParam("from", "From"), textParam("to", "To")});
    b.sh("file.chmod", "Change mode", "chmod {!mode} {path}",
         {textParam("mode", "Mode", true, "644"), textParam("path", "Path")}, true);
    b.sh("file.df", "Storage usage", "df -h");
    b.sh("file.du", "Directory size", "du -sh {path}", {textParam("path", "Path", true, "/sdcard/")});

    // ------------------------------------------------------------------ Apps
    b.group("Apps");
    b.dev("app.install", "Install APK",
          L({"install", "{*replace}", "{*test}", "{*downgrade}", "{*grant}", "{*instant}",
             "{*lock}", "{*sdcard}", "{apk}"}),
          {boolParam("replace", "Replace (-r)", "-r", true), boolParam("test", "Allow test (-t)", "-t"),
           boolParam("downgrade", "Downgrade (-d)", "-d"),
           boolParam("grant", "Grant permissions (-g)", "-g"),
           boolParam("instant", "Instant app", "--instant"),
           boolParam("lock", "Forward lock (-l)", "-l"),
           boolParam("sdcard", "Install on SD card (-s)", "-s"), fileParam("apk", "APK")},
          false, 300000);
    b.dev("app.install-multiple", "Install split APKs",
          L({"install-multiple", "{*replace}", "{*test}", "{*downgrade}", "{*grant}", "{apks}"}),
          {boolParam("replace", "Replace (-r)", "-r", true), boolParam("test", "Allow test (-t)", "-t"),
           boolParam("downgrade", "Downgrade (-d)", "-d"),
           boolParam("grant", "Grant permissions (-g)", "-g"), fileParam("apks", "APK files", "files")},
          false, 300000);
    b.dev("app.install-multi-package", "Install multiple packages",
          L({"install-multi-package", "{*replace}", "{*grant}", "{apks}"}),
          {boolParam("replace", "Replace (-r)", "-r", true),
           boolParam("grant", "Grant permissions (-g)", "-g"), fileParam("apks", "APK files", "files")},
          false, 300000);
    b.dev("app.uninstall", "Uninstall", L({"uninstall", "{*keep}", "{package}"}),
          {boolParam("keep", "Keep data (-k)", "-k"), textParam("package", "Package")}, true);
    b.sh("app.uninstall-user", "Uninstall for user 0 (system apps)", "pm uninstall --user 0 {package}",
         {textParam("package", "Package")}, true);
    b.sh("app.install-existing", "Restore removed system app", "cmd package install-existing {package}",
         {textParam("package", "Package")});
    b.sh("app.list", "List packages", "pm list packages {!filter}",
         {choiceParam("filter", "Filter", L({"", "-3", "-s", "-d", "-e", "-f", "-i", "-u"}), "-3", false)});
    b.sh("app.path", "APK path", "pm path {package}", {textParam("package", "Package")});
    b.sh("app.dump", "Package details", "dumpsys package {package}", {textParam("package", "Package")});
    b.sh("app.grant", "Grant permission", "pm grant {package} {permission}",
         {textParam("package", "Package"),
          textParam("permission", "Permission", true, "android.permission.CAMERA")});
    b.sh("app.revoke", "Revoke permission", "pm revoke {package} {permission}",
         {textParam("package", "Package"),
          textParam("permission", "Permission", true, "android.permission.CAMERA")});
    b.sh("app.clear", "Clear data", "pm clear {package}", {textParam("package", "Package")}, true);
    b.sh("app.disable", "Disable (user 0)", "pm disable-user --user 0 {package}",
         {textParam("package", "Package")}, true);
    b.sh("app.enable", "Enable", "pm enable {package}", {textParam("package", "Package")});
    b.sh("app.force-stop", "Force stop", "am force-stop {package}", {textParam("package", "Package")});
    b.sh("app.launch", "Launch", "monkey -p {package} -c android.intent.category.LAUNCHER 1",
         {textParam("package", "Package")});
    b.sh("app.start", "Start activity", "am start -n {component}",
         {textParam("component", "Component", true, "com.example/.MainActivity")});
    b.sh("app.start-intent", "Start intent", "am start -a {action} {*data}",
         {textParam("action", "Action", true, "android.intent.action.VIEW"),
          flagText("data", "Data URI (-d)", "-d")});
    b.sh("app.broadcast", "Send broadcast", "am broadcast -a {action}",
         {textParam("action", "Action")});
    b.sh("app.start-service", "Start service", "am startservice -n {component}",
         {textParam("component", "Component")});
    b.sh("app.stop-service", "Stop service", "am stopservice -n {component}",
         {textParam("component", "Component")}, true);
    b.sh("app.running-services", "Running services", "dumpsys activity services");
    b.sh("app.set-home", "Set default launcher", "cmd package set-home-activity {component}",
         {textParam("component", "Component")}, true);
    b.sh("app.appops-get", "App ops: get", "appops get {package}", {textParam("package", "Package")});
    b.sh("app.appops-set", "App ops: set", "appops set {package} {op} {mode}",
         {textParam("package", "Package"), textParam("op", "Operation", true, "CAMERA"),
          choiceParam("mode", "Mode", L({"allow", "deny", "ignore", "default", "foreground"}), "allow")});
    b.sh("app.features", "System features", "pm list features");
    b.sh("app.permissions", "Permission groups", "pm list permissions -g -d");

    // ------------------------------------------------------ System & services
    b.group("System & services");
    b.sh("sys.service-list", "Binder services", "service list");
    b.sh("sys.service-check", "Check binder service", "service check {name}",
         {textParam("name", "Service", true, "activity")});
    b.sh("sys.service-call", "Call binder service", "service call {name} {!code}",
         {textParam("name", "Service"), intParam("code", "Transaction code")}, true);
    b.sh("sys.dumpsys-list", "Dumpsys services", "dumpsys -l");
    b.sh("sys.dumpsys", "Dumpsys service", "dumpsys {service}", {textParam("service", "Service", true, "battery")},
         false, 60000);
    b.sh("sys.dumpsys-all", "Dumpsys (everything)", "dumpsys", {}, false, 180000);
    b.sh("sys.meminfo", "Process memory", "dumpsys meminfo {package}",
         {textParam("package", "Package or pid", false)}, false, 60000);
    b.sh("sys.cpuinfo", "CPU usage", "dumpsys cpuinfo");
    b.sh("sys.ps", "Processes", "ps -A");
    b.sh("sys.top", "Top (snapshot)", "top -b -n 1");
    b.sh("sys.kill", "Kill process", "kill {!pid}", {intParam("pid", "PID")}, true);
    b.sh("sys.killall", "Kill by name", "killall {name}", {textParam("name", "Process name")}, true);
    b.dev("sys.jdwp", "JDWP processes", L({"jdwp"}));
    b.sh("sys.alarms", "Alarms", "dumpsys alarm", {}, false, 60000);
    b.sh("sys.jobs", "Scheduled jobs", "dumpsys jobscheduler", {}, false, 60000);
    b.sh("sys.accounts", "Accounts", "dumpsys account");
    b.sh("sys.content-query", "Content provider query", "content query --uri {uri}",
         {textParam("uri", "URI", true, "content://settings/system")});
    b.sh("sys.settings-list", "Settings: list", "settings list {namespace}",
         {choiceParam("namespace", "Namespace", L({"system", "secure", "global"}), "global")});
    b.sh("sys.settings-get", "Settings: get", "settings get {namespace} {key}",
         {choiceParam("namespace", "Namespace", L({"system", "secure", "global"}), "global"),
          textParam("key", "Key")});
    b.sh("sys.settings-put", "Settings: put", "settings put {namespace} {key} {value}",
         {choiceParam("namespace", "Namespace", L({"system", "secure", "global"}), "global"),
          textParam("key", "Key"), textParam("value", "Value")}, true);
    b.sh("sys.settings-delete", "Settings: delete", "settings delete {namespace} {key}",
         {choiceParam("namespace", "Namespace", L({"system", "secure", "global"}), "global"),
          textParam("key", "Key")}, true);
    b.sh("sys.getprop", "Get property", "getprop {key}", {textParam("key", "Property", false)});
    b.sh("sys.setprop", "Set property", "setprop {key} {value}",
         {textParam("key", "Property"), textParam("value", "Value")}, true);
    b.sh("sys.uname", "Kernel", "uname -a");
    b.sh("sys.uptime", "Uptime", "uptime");
    b.sh("sys.date", "Date", "date");

    // ----------------------------------------------------------------- Input
    b.group("Input");
    b.sh("input.keyevent", "Key event", "input keyevent {!key}",
         {textParam("key", "Key code or KEYCODE_* name", true, "KEYCODE_HOME")});
    b.sh("input.text", "Type text", "input text {text}", {textParam("text", "Text")});
    b.sh("input.tap", "Tap", "input tap {!x} {!y}", {intParam("x", "X"), intParam("y", "Y")});
    b.sh("input.swipe", "Swipe / long press", "input swipe {!x1} {!y1} {!x2} {!y2} {!duration}",
         {intParam("x1", "From X"), intParam("y1", "From Y"), intParam("x2", "To X"),
          intParam("y2", "To Y"), intParam("duration", "Duration ms", "300")});
    b.sh("input.scroll", "Scroll (roll)", "input roll {!dx} {!dy}",
         {intParam("dx", "dx", "0"), intParam("dy", "dy", "10")});

    // --------------------------------------------------------------- Display
    b.group("Display");
    b.sh("disp.record", "Record screen on device", "screenrecord --time-limit {!seconds} {remote}",
         {intParam("seconds", "Seconds (max 180)", "30"),
          textParam("remote", "Remote file", true, "/sdcard/record.mp4")},
         false, 200000);
    b.sh("disp.size", "Show size", "wm size");
    b.sh("disp.size-set", "Set size", "wm size {!size}",
         {textParam("size", "WxH", true, "1080x1920")}, true);
    b.sh("disp.size-reset", "Reset size", "wm size reset");
    b.sh("disp.density", "Show density", "wm density");
    b.sh("disp.density-set", "Set density", "wm density {!dpi}", {intParam("dpi", "DPI", "420")}, true);
    b.sh("disp.density-reset", "Reset density", "wm density reset");
    b.sh("disp.rotation-auto", "Auto-rotate", "settings put system accelerometer_rotation {!state}",
         {choiceParam("state", "State", L({"1", "0"}), "1")});
    b.sh("disp.rotation-set", "Force rotation",
         "settings put system accelerometer_rotation 0 ; settings put system user_rotation {!rotation}",
         {choiceParam("rotation", "Rotation", L({"0", "1", "2", "3"}), "0")});
    b.sh("disp.brightness", "Brightness (0-255)", "settings put system screen_brightness {!level}",
         {intParam("level", "Level", "128")});
    b.sh("disp.animations", "Animation scale",
         "settings put global window_animation_scale {!scale} ; settings put global "
         "transition_animation_scale {!scale} ; settings put global animator_duration_scale {!scale}",
         {textParam("scale", "Scale", true, "1.0")});
    b.sh("disp.wake", "Wake screen", "input keyevent KEYCODE_WAKEUP");
    b.sh("disp.sleep", "Sleep screen", "input keyevent KEYCODE_SLEEP");
    b.sh("disp.notifications", "Expand notifications", "cmd statusbar expand-notifications");
    b.sh("disp.quick-settings", "Expand quick settings", "cmd statusbar expand-settings");
    b.sh("disp.collapse", "Collapse panels", "cmd statusbar collapse");
    b.sh("disp.info", "Display info", "dumpsys display", {}, false, 60000);

    // ---------------------------------------------------- Connectivity & power
    b.group("Network & power");
    b.sh("net.wifi", "Wi-Fi", "svc wifi {!state}", {choiceParam("state", "State", enableDisable, "enable")});
    b.sh("net.data", "Mobile data", "svc data {!state}", {choiceParam("state", "State", enableDisable, "enable")});
    b.sh("net.bluetooth", "Bluetooth", "svc bluetooth {!state}",
         {choiceParam("state", "State", enableDisable, "enable")});
    b.sh("net.nfc", "NFC", "svc nfc {!state}", {choiceParam("state", "State", enableDisable, "enable")});
    b.sh("net.airplane", "Airplane mode", "cmd connectivity airplane-mode {!state}",
         {choiceParam("state", "State", enableDisable, "enable")});
    b.sh("net.wifi-status", "Wi-Fi status", "cmd wifi status");
    b.sh("net.ip", "IP addresses", "ip -f inet addr show");
    b.sh("net.sockets", "Listening sockets", "netstat -tuln");
    b.sh("net.ping", "Ping", "ping -c 4 {host}", {textParam("host", "Host", true, "8.8.8.8")}, false, 30000);
    b.sh("power.battery", "Battery", "dumpsys battery");
    b.sh("power.battery-set", "Battery: simulate", "dumpsys battery set {!what} {!value}",
         {choiceParam("what", "Property", L({"level", "ac", "usb", "wireless", "status"}), "level"),
          textParam("value", "Value", true, "50")}, true);
    b.sh("power.battery-reset", "Battery: reset", "dumpsys battery reset");
    b.sh("power.doze", "Doze control", "dumpsys deviceidle {!action}",
         {choiceParam("action", "Action", L({"force-idle", "unforce", "step", "get deep"}), "force-idle")});
    b.sh("power.stayon", "Stay awake while charging", "svc power stayon {!mode}",
         {choiceParam("mode", "Mode", L({"true", "false", "usb", "ac", "wireless"}), "true")});
    b.sh("power.wakelocks", "Wake locks", "dumpsys power", {}, false, 60000);

    // ------------------------------------------------------------------ Logs
    b.group("Logs & diagnostics");
    b.sh("log.dump", "Logcat (dump)", "logcat -d -b {!buffer} -v {!format} -t {!lines}",
         {choiceParam("buffer", "Buffer", L({"main", "system", "crash", "events", "radio", "all"}), "main"),
          choiceParam("format", "Format",
                      L({"threadtime", "brief", "tag", "process", "time", "long", "raw"}), "threadtime"),
          intParam("lines", "Last N lines", "500")},
         false, 60000);
    b.sh("log.clear", "Logcat: clear", "logcat -c", {}, true);
    b.sh("log.buffers", "Logcat: buffer sizes", "logcat -g");
    b.dev("log.bugreport", "Bug report", L({"bugreport", "{path}"}),
          {fileParam("path", "Output folder or .zip", "dir")}, false, 900000);
    b.sh("log.tombstones", "Tombstones", "ls -l /data/tombstones", {}, false, 20000);
    b.sh("log.anr", "ANR traces", "ls -l /data/anr", {}, false, 20000);

    // -------------------------------------------------------------- Recovery
    b.group("Power & recovery");
    b.dev("rec.reboot", "Reboot", L({"reboot", "{mode}"}),
          {choiceParam("mode", "Mode",
                       L({"", "bootloader", "recovery", "sideload", "sideload-auto-reboot", "fastboot"}),
                       "", false)},
          true);
    b.sh("rec.poweroff", "Power off", "reboot -p", {}, true);
    b.dev("rec.sideload", "Sideload OTA", L({"sideload", "{file}"}), {fileParam("file", "OTA zip")}, true,
          0);
    b.dev("rec.emu", "Emulator console", L({"emu", "{command}"}), {textParam("command", "Command")});

    // ------------------------------------------------------------ Privileged
    b.group("Privileged");
    b.dev("priv.root", "Restart adbd as root", L({"root"}), {}, true);
    b.dev("priv.unroot", "Restart adbd without root", L({"unroot"}), {}, true);
    b.dev("priv.remount", "Remount partitions", L({"remount", "{*reboot}"}),
          {boolParam("reboot", "Reboot if needed (-R)", "-R")}, true);
    b.dev("priv.disable-verity", "Disable dm-verity", L({"disable-verity"}), {}, true);
    b.dev("priv.enable-verity", "Enable dm-verity", L({"enable-verity"}), {}, true);

    // ------------------------------------------------------------------ Info
    b.group("Device info");
    b.sh("info.props", "All properties", "getprop");
    b.sh("info.build", "Build fingerprint", "getprop ro.build.fingerprint");
    b.sh("info.meminfo", "Memory", "cat /proc/meminfo");
    b.sh("info.cpuinfo", "CPU", "cat /proc/cpuinfo");
    b.sh("info.sensors", "Sensors", "dumpsys sensorservice", {}, false, 60000);
    b.sh("info.window", "Window displays", "dumpsys window displays", {}, false, 60000);
    b.sh("info.thermal", "Thermal", "dumpsys thermalservice", {}, false, 60000);
    b.sh("info.id", "Current user", "id");
    return list;
}

const QRegularExpression& placeholderRe() {
    static const QRegularExpression re(QStringLiteral("\\{([!*]?)([A-Za-z0-9_]+)\\}"));
    return re;
}

const QRegularExpression& rawSafeRe() {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_.:/@+,=-]*$"));
    return re;
}

const CommandParam* findParam(const CommandSpec& spec, const QString& key) {
    for (const CommandParam& p : spec.params) {
        if (p.key == key) {
            return &p;
        }
    }
    return nullptr;
}

QString valueOf(const CommandParam& p, const QVariantMap& values) {
    const QVariant v = values.value(p.key);
    if (v.isValid() && !v.isNull()) {
        if (p.type == QStringLiteral("bool")) {
            return v.toBool() ? QStringLiteral("true") : QString();
        }
        if (v.typeId() == QMetaType::QStringList) {
            return v.toStringList().join(QLatin1Char('\n'));
        }
        return v.toString().trimmed();
    }
    return p.defaultValue;
}

// Validates a single value against its param definition.
QString validate(const CommandParam& p, const QString& value) {
    if (p.type == QStringLiteral("bool")) {
        return {};
    }
    if (value.isEmpty()) {
        return p.required ? QStringLiteral("Missing value: %1").arg(p.label) : QString();
    }
    if (p.type == QStringLiteral("int")) {
        bool ok = false;
        value.toLongLong(&ok);
        if (!ok) {
            return QStringLiteral("%1 must be a number").arg(p.label);
        }
    } else if (p.type == QStringLiteral("choice") && !p.choices.contains(value)) {
        return QStringLiteral("%1 must be one of: %2").arg(p.label, p.choices.join(QStringLiteral(", ")));
    }
    return {};
}

// Flag form: bool -> flag; otherwise "flag value" (or "flag=value" if the flag ends in '=').
QStringList flagForm(const CommandParam& p, const QString& value) {
    if (value.isEmpty() || p.flag.isEmpty()) {
        return {};
    }
    if (p.type == QStringLiteral("bool")) {
        return {p.flag};
    }
    if (p.flag.endsWith(QLatin1Char('='))) {
        return {p.flag + value};
    }
    return {p.flag, value};
}

} // namespace

const QList<CommandSpec>& commandCatalog() {
    static const QList<CommandSpec> catalog = buildCatalog();
    return catalog;
}

const CommandSpec* findCommand(const QString& id) {
    for (const CommandSpec& spec : commandCatalog()) {
        if (spec.id == id) {
            return &spec;
        }
    }
    return nullptr;
}

QStringList commandGroups() {
    QStringList groups;
    for (const CommandSpec& spec : commandCatalog()) {
        if (!groups.contains(spec.group)) {
            groups.append(spec.group);
        }
    }
    return groups;
}

ExpandResult expandCommand(const CommandSpec& spec, const QVariantMap& values) {
    ExpandResult result;

    // Validate every declared parameter once.
    QHash<QString, QString> resolved;
    for (const CommandParam& p : spec.params) {
        const QString value = valueOf(p, values);
        const QString error = validate(p, value);
        if (!error.isEmpty()) {
            result.error = error;
            return result;
        }
        resolved.insert(p.key, value);
    }

    const bool shellMode = spec.mode == CommandMode::Shell;
    QStringList tokens;
    if (shellMode) {
        tokens = spec.tmpl.value(0).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    } else {
        tokens = spec.tmpl;
    }

    QStringList out;
    for (const QString& token : std::as_const(tokens)) {
        // Whole-token flag form: {*key}
        const auto whole = placeholderRe().match(token);
        if (whole.hasMatch() && whole.capturedStart() == 0 && whole.capturedLength() == token.size()) {
            const QString mode = whole.captured(1);
            const CommandParam* p = findParam(spec, whole.captured(2));
            if (!p) {
                result.error = QStringLiteral("Unknown parameter: %1").arg(whole.captured(2));
                return result;
            }
            const QString value = resolved.value(p->key);
            if (mode == QStringLiteral("*")) {
                const QStringList flagged = flagForm(*p, value);
                if (shellMode) {
                    for (qsizetype i = 0; i < flagged.size(); ++i) {
                        // value part (index 1, or fused flag=value) must be quoted
                        const bool isValue = i == 1 || (flagged.size() == 1 && p->type != QStringLiteral("bool") &&
                                                        p->flag.endsWith(QLatin1Char('=')));
                        out << (isValue ? shellQuote(flagged.at(i)) : flagged.at(i));
                    }
                } else {
                    out << flagged;
                }
                continue;
            }
            if (value.isEmpty()) {
                continue; // optional token omitted
            }
            if (p->type == QStringLiteral("files") && !shellMode) {
                out << value.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                continue;
            }
        }

        // Inline substitution.
        QString expanded;
        qsizetype last = 0;
        bool skip = false;
        auto it = placeholderRe().globalMatch(token);
        while (it.hasNext()) {
            const auto m = it.next();
            const CommandParam* p = findParam(spec, m.captured(2));
            if (!p) {
                result.error = QStringLiteral("Unknown parameter: %1").arg(m.captured(2));
                return result;
            }
            const QString value = resolved.value(p->key);
            expanded += token.mid(last, m.capturedStart() - last);
            last = m.capturedEnd();
            if (value.isEmpty()) {
                skip = skip || m.capturedLength() == token.size();
                continue;
            }
            if (shellMode) {
                if (m.captured(1) == QStringLiteral("!")) {
                    if (!rawSafeRe().match(value).hasMatch()) {
                        result.error = QStringLiteral("%1 contains unsupported characters").arg(p->label);
                        return result;
                    }
                    expanded += value;
                } else {
                    expanded += shellQuote(value);
                }
            } else {
                expanded += value;
            }
        }
        expanded += token.mid(last);
        if (!skip) {
            out << expanded;
        }
    }

    if (shellMode) {
        result.args = {QStringLiteral("shell"), out.join(QLatin1Char(' '))};
    } else {
        result.args = out;
    }
    result.ok = true;
    return result;
}

} // namespace polyscope::adb
