#include "adb/AdbParsers.h"

#include <QRegularExpression>

namespace polyscope::adb {

namespace {

QStringList splitLines(const QString& text) {
    QString normalized = text;
    normalized.remove(QLatin1Char('\r'));
    return normalized.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

} // namespace

QList<DeviceEntry> parseDevices(const QString& text) {
    QList<DeviceEntry> result;
    for (const QString& rawLine : splitLines(text)) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('*')) ||
            line.startsWith(QStringLiteral("List of devices"))) {
            continue;
        }
        const QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                              Qt::SkipEmptyParts);
        if (tokens.size() < 2) {
            continue;
        }
        DeviceEntry entry;
        entry.serial = tokens.at(0);
        qsizetype next = 2;
        entry.state = tokens.at(1);
        if (entry.state == QStringLiteral("no") && tokens.size() > 2 &&
            tokens.at(2) == QStringLiteral("permissions")) {
            entry.state = QStringLiteral("no permissions");
            next = 3;
            // Skip the parenthesised reason, which may contain spaces.
            while (next < tokens.size() && !tokens.at(next).contains(QLatin1Char(':'))) {
                ++next;
            }
        }
        for (qsizetype i = next; i < tokens.size(); ++i) {
            const qsizetype colon = tokens.at(i).indexOf(QLatin1Char(':'));
            if (colon <= 0) {
                continue;
            }
            const QString key = tokens.at(i).left(colon);
            const QString value = tokens.at(i).mid(colon + 1);
            if (key == QStringLiteral("model")) {
                entry.model = QString(value).replace(QLatin1Char('_'), QLatin1Char(' '));
            } else if (key == QStringLiteral("product")) {
                entry.product = value;
            } else if (key == QStringLiteral("device")) {
                entry.device = value;
            } else if (key == QStringLiteral("transport_id")) {
                entry.transportId = value;
            } else if (key == QStringLiteral("usb")) {
                entry.usb = value;
            }
        }
        result.append(entry);
    }
    return result;
}

QHash<QString, QString> parseGetprop(const QString& text) {
    static const QRegularExpression re(QStringLiteral("^\\[([^\\]]+)\\]:\\s*\\[(.*)\\]\\s*$"));
    QHash<QString, QString> props;
    for (const QString& line : splitLines(text)) {
        const auto match = re.match(line);
        if (match.hasMatch()) {
            props.insert(match.captured(1), match.captured(2));
        }
    }
    return props;
}

BatteryInfo parseBattery(const QString& dumpsysBattery) {
    BatteryInfo info;
    int statusCode = 0;
    for (const QString& rawLine : splitLines(dumpsysBattery)) {
        const QString line = rawLine.trimmed();
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }
        const QString key = line.left(colon).trimmed();
        const QString value = line.mid(colon + 1).trimmed();
        bool ok = false;
        if (key == QStringLiteral("level")) {
            const int level = value.toInt(&ok);
            if (ok) {
                info.level = level;
            }
        } else if (key == QStringLiteral("status")) {
            statusCode = value.toInt(&ok);
        } else if (key == QStringLiteral("temperature")) {
            const int tenths = value.toInt(&ok);
            if (ok) {
                info.temperatureC = tenths / 10.0;
            }
        } else if (key == QStringLiteral("voltage")) {
            const int mv = value.toInt(&ok);
            if (ok) {
                info.voltageMv = mv;
            }
        } else if (value == QStringLiteral("true") && info.power.isEmpty()) {
            if (key == QStringLiteral("AC powered")) {
                info.power = QStringLiteral("AC");
            } else if (key == QStringLiteral("USB powered")) {
                info.power = QStringLiteral("USB");
            } else if (key == QStringLiteral("Wireless powered")) {
                info.power = QStringLiteral("Wireless");
            }
        }
    }
    switch (statusCode) {
    case 2:
        info.status = QStringLiteral("Charging");
        break;
    case 3:
        info.status = QStringLiteral("Discharging");
        break;
    case 4:
        info.status = QStringLiteral("Not charging");
        break;
    case 5:
        info.status = QStringLiteral("Full");
        break;
    default:
        info.status = QStringLiteral("Unknown");
        break;
    }
    return info;
}

QList<MdnsService> parseMdnsServices(const QString& text) {
    QList<MdnsService> services;
    for (const QString& line : splitLines(text)) {
        const QStringList parts =
            line.trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 3 || !parts.at(1).startsWith(QLatin1Char('_'))) {
            continue; // header or noise
        }
        const QString& address = parts.at(2);
        const qsizetype colon = address.lastIndexOf(QLatin1Char(':'));
        if (colon <= 0) {
            continue;
        }
        bool ok = false;
        const int port = address.mid(colon + 1).toInt(&ok);
        if (!ok) {
            continue;
        }
        MdnsService service;
        service.name = parts.at(0);
        service.type = parts.at(1);
        service.host = address.left(colon);
        service.port = port;
        services.append(service);
    }
    return services;
}

QList<FileEntry> parseLsLong(const QString& text) {
    static const QRegularExpression re(QStringLiteral(
        "^([\\-dlcbps])([rwxsStT\\-]{9})[.+@]?\\s+\\d+\\s+\\S+\\s+\\S+\\s+(?:\\d+,\\s*)?(\\d+)\\s+"
        "(\\d{4}-\\d{2}-\\d{2}\\s+\\d{2}:\\d{2})\\s+(.+)$"));
    QList<FileEntry> entries;
    for (const QString& line : splitLines(text)) {
        const auto match = re.match(line.trimmed());
        if (!match.hasMatch()) {
            continue;
        }
        FileEntry entry;
        const QChar type = match.captured(1).at(0);
        entry.isDir = type == QLatin1Char('d');
        entry.isLink = type == QLatin1Char('l');
        entry.permissions = match.captured(1) + match.captured(2);
        entry.size = match.captured(3).toLongLong();
        entry.modified = match.captured(4);
        QString name = match.captured(5);
        if (entry.isLink) {
            const qsizetype arrow = name.indexOf(QStringLiteral(" -> "));
            if (arrow > 0) {
                entry.linkTarget = name.mid(arrow + 4);
                name = name.left(arrow);
            }
        }
        if (name == QStringLiteral(".") || name == QStringLiteral("..")) {
            continue;
        }
        entry.name = name;
        entries.append(entry);
    }
    return entries;
}

QList<BinderService> parseServiceList(const QString& text) {
    static const QRegularExpression re(QStringLiteral("^\\d+\\s+([^:\\s]+):\\s*\\[(.*)\\]\\s*$"));
    QList<BinderService> result;
    for (const QString& line : splitLines(text)) {
        const auto match = re.match(line.trimmed());
        if (match.hasMatch()) {
            result.append({match.captured(1), match.captured(2)});
        }
    }
    return result;
}

QStringList parseDumpsysList(const QString& text) {
    QStringList names;
    for (const QString& line : splitLines(text)) {
        const QString name = line.trimmed();
        if (!name.isEmpty() && !name.startsWith(QStringLiteral("Currently running"))) {
            names.append(name);
        }
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QList<RunningService> parseRunningServices(const QString& text) {
    static const QRegularExpression re(
        QStringLiteral("ServiceRecord\\{\\w+ u(\\d+) ([^\\s/}]+)/([^\\s}]+)"));
    QList<RunningService> result;
    QStringList seen;
    for (const QString& line : splitLines(text)) {
        const auto match = re.match(line);
        if (!match.hasMatch()) {
            continue;
        }
        RunningService service;
        service.userId = match.captured(1).toInt();
        service.package = match.captured(2);
        service.component = match.captured(2) + QLatin1Char('/') + match.captured(3);
        if (!seen.contains(service.component)) {
            seen.append(service.component);
            result.append(service);
        }
    }
    return result;
}

QStringList parsePackages(const QString& text) {
    QStringList packages;
    const QString prefix = QStringLiteral("package:");
    for (const QString& line : splitLines(text)) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(prefix)) {
            packages.append(trimmed.mid(prefix.size()));
        }
    }
    packages.sort(Qt::CaseInsensitive);
    return packages;
}

QString parseIpAddress(const QString& text) {
    static const QRegularExpression re(QStringLiteral("inet\\s+(\\d{1,3}(?:\\.\\d{1,3}){3})/"));
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const QString ip = it.next().captured(1);
        if (!ip.startsWith(QStringLiteral("127."))) {
            return ip;
        }
    }
    return {};
}

bool messageIndicatesFailure(const QString& text) {
    const QString t = text.toLower();
    return t.contains(QStringLiteral("failed")) || t.contains(QStringLiteral("cannot")) ||
           t.contains(QStringLiteral("unable")) || t.contains(QStringLiteral("error"));
}

bool shellOutputIndicatesFailure(const QString& text) {
    static const QRegularExpression failure(
        QStringLiteral("(?m)^(Failure|Error|Exception|Security exception)\\b"));
    return failure.match(text).hasMatch();
}

bool hasExplicitSerial(const QStringList& args) {
    static const QStringList withValue = {QStringLiteral("-s"), QStringLiteral("-t"), QStringLiteral("-H"),
                                          QStringLiteral("-P"), QStringLiteral("-L")};
    for (qsizetype i = 0; i < args.size() && args.at(i).startsWith(QLatin1Char('-')); ++i) {
        if (args.at(i) == QStringLiteral("-s")) {
            return true;
        }
        if (withValue.contains(args.at(i))) {
            ++i; // skip its value
        }
    }
    return false;
}

bool isWirelessSerial(const QString& serial) {
    return serial.contains(QLatin1Char(':')) || serial.contains(QStringLiteral("._adb-tls-"));
}

QByteArray encodeServerRequest(const QByteArray& payload) {
    return QByteArray::number(payload.size(), 16).rightJustified(4, '0') + payload;
}

std::optional<QByteArray> takeServerFrame(QByteArray& buffer, bool* malformed) {
    if (malformed) {
        *malformed = false;
    }
    if (buffer.size() < 4) {
        return std::nullopt;
    }
    // toInt() would also accept "+1f" or " 1f"; the wire format is exactly four hex digits.
    static const QRegularExpression hex4(QStringLiteral("^[0-9a-fA-F]{4}$"));
    bool ok = hex4.match(QString::fromLatin1(buffer.left(4))).hasMatch();
    const int length = ok ? buffer.left(4).toInt(&ok, 16) : 0;
    if (!ok || length < 0) {
        if (malformed) {
            *malformed = true;
        }
        buffer.clear();
        return std::nullopt;
    }
    if (buffer.size() < 4 + length) {
        return std::nullopt;
    }
    QByteArray payload = buffer.mid(4, length);
    buffer.remove(0, 4 + length);
    return payload;
}

QString shellQuote(const QString& arg) {
    QString escaped = arg;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

} // namespace polyscope::adb
