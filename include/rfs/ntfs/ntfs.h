#pragma once

#ifdef __GNUC__
#pragma interface
#pragma GCC attribute("-Wextra")
#endif

#ifdef _WIN32_
#   define __WIN_RFS__
#   if defined(__MSVC__)
#       include <windows.h>
#   endif
#endif

#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <wchar.h>
#if __DEBUG__
#include <cerrno>
#endif

typedef struct _FILETIME 
{
    unsigned long dwLowDateTime;
    unsigned long dwHighDateTime;
} FILETIME;

typedef struct _WIN32_FIND_DATAW 
{
    unsigned long dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    unsigned long nFileSizeHigh;
    unsigned long nFileSizeLow;
    unsigned long dwReserved0;
    unsigned long dwReserved1;
    wchar_t cFileName[260];
    wchar_t cAlternateFileName[14];
    #if __DEBUG__
    size_t sFileSize;
    size_t defaultSectionSize;
    #endif
} WIN32_FIND_DATAW;

