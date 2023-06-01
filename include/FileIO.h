#ifndef FILEIO_H
#define FILEIO_H

#ifdef _WIN32
#include <stdlib.h> // MAX_PATH
#endif
#include <fcntl.h>
// See (fcntl.h): https://www.rpi.edu/dept/cis/software/g77-mingw32/include/fcntl.h
// ref: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/open-wopen?view=msvc-170


#ifndef O_BINARY
#define O_BINARY    0 // 0x8000 on Windows but does not exist in Unix.
#endif

#ifndef O_DIRECT
#define O_DIRECT    0 // 040000 on Unix but does not exist in Windows.
#endif

#ifndef O_SEQUENTIAL
#define O_SEQUENTIAL    0 // 0x0020 on Windows but does not exist in Unix.
#endif

#ifndef _WIN32
#define PATH_SEPARATOR_CHR  '/'
#define MAX_PATH             512 // 260 on Windows.
#else
#define PATH_SEPARATOR_CHR  '\\'
#endif // _WIN32

#endif // FILEIO_H
