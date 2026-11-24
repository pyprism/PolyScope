#include "core/WindowActivation.h"

namespace polyscope {

#ifndef Q_OS_MACOS
bool bringProcessToFront(qint64) {
    return true;
}
#endif

} // namespace polyscope
