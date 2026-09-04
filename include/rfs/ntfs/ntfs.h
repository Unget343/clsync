#pragma once

#ifdef _WIN32
#   define __WIN_RFS__
#   if defined(_MSC_VER)
#       include <windows.h>
#   endif
#endif

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <wchar.h>
#if __DEBUG__
#include <cerrno>
#endif

#include "../fs_file.h"

#ifndef __WIN_RFS__
// Definitions for non-Windows platforms to parse NTFS structures
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
    size_t sFileSize;           // file size in section table
    size_t defaultSectionSize;  // default section size
    #endif
} WIN32_FIND_DATAW;
#endif // __WIN_RFS__

#define _O_RDONLY    0x0000
#define _O_WDONLY    0x0001
#define _O_RDWR      0x0002
#define _O_CREAT     0x0100
#define _O_TRUNC     0x0200
#define _O_EXCL      0x0400
#define _O_APPEND    0x0800

namespace rfs {
namespace ntfs {

class NtfsFile : public rfs::File {
public:
    NtfsFile();
    virtual ~NtfsFile();

    // Overridden to intercept access to .mounted
    virtual bool open(const std::string& path, std::ios_base::openmode mode) override;

    // NTFS specific capabilities can be added here
    // For example, getting file information based on NTFS features
    bool get_file_info(WIN32_FIND_DATAW* info);

    // Opening Alternate Data Streams (ADS), a key NTFS feature
    bool open_stream(const std::string& path, const std::string& stream_name, std::ios_base::openmode mode);
};

} // namespace ntfs
} // namespace rfs
