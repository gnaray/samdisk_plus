#ifndef FILEIO_H
#define FILEIO_H

#include <fcntl.h>

#ifndef HAVE_O_BINARY
// Still might have O_BINARY from WindowsStub.h, then override it.
#undef O_BINARY
#define O_BINARY    0
#endif

#ifndef O_DIRECT
#define O_DIRECT    0
#endif

#ifndef _WIN32
#define PATH_SEPARATOR_CHR  '/'
#else
#define PATH_SEPARATOR_CHR  '\\'
#endif // _WIN32

#define MAX_PATH             512 // 260 in Windows originally.

#endif // FILEIO_H
