#pragma once

#include <QCoreApplication>
#include <QString>

namespace polyscope {

// An operation can finish after the user switched to another device. Its message is still worth
// showing (the user asked for it) but must not read as if it concerned the current device.
inline QString labelForSerial(const QString& message, const QString& startedOn, const QString& current) {
    if (startedOn == current) {
        return message;
    }
    return message + QCoreApplication::translate("StaleResult", " (on %1)").arg(startedOn);
}

} // namespace polyscope
