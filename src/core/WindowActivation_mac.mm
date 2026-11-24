#include "core/WindowActivation.h"

#import <AppKit/AppKit.h>

namespace polyscope {

bool bringProcessToFront(qint64 pid) {
    NSRunningApplication* target = [NSRunningApplication runningApplicationWithProcessIdentifier:static_cast<pid_t>(pid)];
    if (target == nil || target.terminated) {
        return false;
    }
    if (@available(macOS 14.0, *)) {
        // Cooperative activation: we are the active app, so we may hand activation over.
        [NSApp yieldActivationToApplication:target];
        return [target activateFromApplication:NSRunningApplication.currentApplication options:0];
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations" // only reached on macOS < 14
    return [target activateWithOptions:NSApplicationActivateIgnoringOtherApps];
#pragma clang diagnostic pop
}

} // namespace polyscope
