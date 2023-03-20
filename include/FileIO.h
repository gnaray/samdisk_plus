#ifndef IO_H
#define IO_H

#ifndef HAVE_O_BINARY
#define O_BINARY    0
#endif

#ifndef O_DIRECT
#define O_DIRECT    0
#endif

#ifndef _WIN32

// ToDo: fix BlockDevice so it doesn't need these
typedef void* HANDLE;
#define INVALID_HANDLE_VALUE reinterpret_cast<void *>(-1)

#define O_SEQUENTIAL    0
#define O_BINARY        0

#define DeviceIoControl(a,b,c,d,e,f,g,h)    (*g = 0)

#define CTL_CODE(a,b,c,d)   (b)
#define FILE_READ_DATA      0
#define FILE_WRITE_DATA     0
#define METHOD_BUFFERED     0
#define METHOD_OUT_DIRECT   0
#define METHOD_IN_DIRECT    0

#endif // WIN32


#ifdef _WIN32
#define PATH_SEPARATOR_CHR  '\\'
#else
#define PATH_SEPARATOR_CHR  '/'
#endif


#endif // IO_H
