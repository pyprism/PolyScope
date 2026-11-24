#pragma once

#include <QtGlobal>

namespace polyscope {

// Brings the windows of another process (scrcpy) in front of ours.
// macOS does not raise windows of non-bundled child processes, so scrcpy's window would
// otherwise open behind PolyScope. Returns false when the process has no activatable app yet
// (its window may not exist for a moment after launch); callers retry. No-op elsewhere,
// where window managers already raise newly mapped windows.
bool bringProcessToFront(qint64 pid);

} // namespace polyscope
