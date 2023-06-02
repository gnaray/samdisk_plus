#pragma once

#include "config.h"

#if defined(_WIN32) && !defined(WINVER)
#define WINVER 0x0500
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0501
#define _RICHEDIT_VER 0x0100
#endif


// Handle, O_*, etc. moved to FileIO.h


#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE //_CRT_SECURE_NO_WARNING is newer.
#define _CRT_NONSTDC_NO_DEPRECATE
#define _WINSOCK_DEPRECATED_NO_WARNINGS
// #define _ITERATOR_DEBUG_LEVEL 0  // ToDo: remove?

#pragma warning(default:4062)       // enumerator 'identifier' in a switch of enum 'enumeration' is not handled
// #pragma warning(default:4242)    // 'identifier' : conversion from 'type1' to 'type2', possible loss of data
// #pragma warning(default:4265)    // 'class': class has virtual functions, but destructor is not virtual
#endif // _MSC_VER


#if !defined(HAVE_STRCASECMP) && defined(HAVE__STRCMPI)
#define strcasecmp  _stricmp
#define HAVE_STRCASECMP
#endif

#if !defined(HAVE_SNPRINTF) && defined(HAVE__SNPRINTF)
#define snprintf    _snprintf
#define HAVE_SNPRINTF
#endif

#ifndef HAVE_LSEEK64
#ifdef HAVE__LSEEKI64
#define lseek64 _lseeki64
#else
#define lseek64 lseek
#endif
#endif

#ifdef _WIN32
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
//#define STRICT 1
#else
#endif // WIN32
