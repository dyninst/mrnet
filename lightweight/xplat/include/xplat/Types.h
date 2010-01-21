#if !defined (__xplat_types_h)
#define xplat_types_h

#if defined(os_windows)
#include "xplat/Types-win.h"
#else
#include "xplat/Types-unix.h"
#endif //defined(WIN32)

#endif // xplat_types_h
