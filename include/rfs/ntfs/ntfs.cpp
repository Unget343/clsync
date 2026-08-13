#include "ntfs.h"
#include <limits.h>

#ifdef __WIN_RFS__
#include <windows.h>
#endif

namespace rfs {
namespace ntfs {

NtfsFile::NtfsFile() : rfs::File() {
}

NtfsFile::~NtfsFile() {
}

bool NtfsFile::open(const std::string& path, std::ios_base::openmode mode) {
    // Intercept access to .mounted to hide it from inner file systems
    if (path == ".mounted" || 
        (path.length() >= 9 && path.substr(path.length() - 9) == "/.mounted") ||
        (path.length() >= 9 && path.substr(path.length() - 9) == "\\.mounted")) {
        last_error_ = 5; // Access Denied
        return false;
    }
    
    return rfs::File::open(path, mode);
}

bool NtfsFile::get_file_info(WIN32_FIND_DATAW* info) {
    if (!info || path_.empty()) return false;

    // Hide .mounted from get_file_info requests
    if (path_ == ".mounted" || 
        (path_.length() >= 9 && path_.substr(path_.length() - 9) == "/.mounted") ||
        (path_.length() >= 9 && path_.substr(path_.length() - 9) == "\\.mounted")) {
        last_error_ = 2; // File Not Found
        return false;
    }

#ifdef __WIN_RFS__
    // Use Windows API to fetch attributes (e.g. for NTFS)
    // Convert path to wide string
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &path_[0], (int)path_.size(), NULL, 0);
    std::wstring wpath(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &path_[0], (int)path_.size(), &wpath[0], size_needed);

    HANDLE hFind = FindFirstFileW(wpath.c_str(), info);
    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
        return true;
    }
    return false;
#else
    // Stub for non-Windows platforms parsing NTFS, or fail gracefully
    last_error_ = 1; // not supported
    return false;
#endif
}

bool NtfsFile::open_stream(const std::string& path, const std::string& stream_name, std::ios_base::openmode mode) {
    // In NTFS, Alternate Data Streams are accessed via "filename:streamname"
    std::string full_path = path + ":" + stream_name;
    return open(full_path, mode);
}

} // namespace ntfs
} // namespace rfs