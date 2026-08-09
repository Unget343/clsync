#pragma once

#include <filesystem>
#include <string>
#include <vector>
using namespace std;


// We search for the ".mounted" file in the current directory and then return a pointer to that file. 
// The ".mounted" file is required to return information about which file system is used for this directory in RFS
void* mountd(char *path);

// A function that takes pointers to a directory, followed
// by parameters specifying the file system to which it should be mounted
int mountd_dir(char *path, char *mnt);

// Returns all partitions that are currently mounted and available for reading
// on the Android device. The list is taken from /proc/self/mounts (or /proc/mounts).
std::vector<std::string> get_readable_partitions();

// mounting the /dev (devfs) partition for reading and writing device information 
char* dev();
