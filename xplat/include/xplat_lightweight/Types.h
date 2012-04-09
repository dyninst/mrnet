/****************************************************************************
 * Copyright © 2003-2012 Dorian C. Arnold, Philip C. Roth, Barton P. Miller *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#ifndef __xplat_types_h
#define __xplat_types_h

#if defined(os_windows)
# include "xplat_lightweight/Types-win.h"
#else
# include "xplat_lightweight/Types-unix.h"
#endif // defined(os_windows)

#ifndef true
# define true (1)
#endif

#ifndef false
# define false (0)
#endif

#ifndef bool_t
# define bool_t int
#endif

#endif // __xplat_types_h
