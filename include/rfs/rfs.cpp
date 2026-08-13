#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "rfs.h"

using std::ifstream;
using std::string;
namespace fs = std::filesystem;

namespace {

std::vector<string> collect_mounts_from_file(const fs::path& path)
{
    std::vector<string> mounts;
    ifstream input(path);
    if (!input.is_open()) {
        return mounts;
    }

    string line;
    while (std::getline(input, line)) {
        std::istringstream stream(line);
        string device;
        string mount_point;
        string fs_type;
        string options;

        if (!(stream >> device >> mount_point >> fs_type >> options)) {
            continue;
        }

        const bool is_readable = options.find("ro") != string::npos || options.find("rw") != string::npos;
        const bool looks_like_partition =
            device.rfind("/dev/", 0) == 0 ||
            device.rfind("/mnt/", 0) == 0 ||
            fs_type == "ext4" ||
            fs_type == "f2fs" ||
            fs_type == "vfat" ||
            fs_type == "exfat" ||
            fs_type == "erofs" ||
            fs_type == "overlay" ||
            fs_type == "fuse" ||
            fs_type == "sdcardfs";

        if (is_readable && looks_like_partition) {
            mounts.push_back(mount_point);
        }
    }

    std::sort(mounts.begin(), mounts.end());
    mounts.erase(std::unique(mounts.begin(), mounts.end()), mounts.end());
    return mounts;
}

} // namespace

void* mountd(char *path) 
{
    fs::path rfs_path(path);
    string mnt = ".mounted";

    // We use a loop to search for a file in the current directory
    for (const auto& entry : fs::directory_iterator(rfs_path)) 
    {
        if (entry.is_regular_file() &&
             entry.path().filename() == mnt) {
             static string s_path = entry.path().string();
             return (void*)s_path.c_str();
        }
    }

    return nullptr; 
}

int mountd_dir(char *path, char *mnt)
{
    fs::path rfs_path(path);
    ifstream mounted_file(rfs_path / ".mounted");
    if (!mounted_file.is_open()) return -1;
  
    return 0;
}

int mountd_parse()
{
    fs::path rfs_path(".");
    ifstream mounted_file(rfs_path / ".mounted");
    if (!mounted_file.is_open()) return -1;

    string line;
    while (getline(mounted_file, line)) 
    {
        if (line.find("[dr.mnt.fs]: ") != string::npos) 
        {
            string mnt = line.substr(line.find(":") + 1);
            
        }
    }

    mounted_file.close();
    return 0;
}

std::vector<std::string> get_readable_partitions()
{
    std::array<const char*, 2> candidates = {"/proc/self/mounts", "/proc/mounts"};
    for (const char* candidate : candidates) {
        const auto mounts = collect_mounts_from_file(candidate);
        if (!mounts.empty()) {
            return mounts;
        }
    }

    return {};
}