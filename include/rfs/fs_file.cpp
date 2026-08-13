#include "fs_file.h"
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <winfsp/winfsp.h>
#endif

namespace rfs {

bool is_target_mounted_file(const std::string& path) {
#ifdef _WIN32
    std::string lower_path = path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    
    size_t sep = lower_path.find_last_of("/\\");
    std::string filename = (sep == std::string::npos) ? lower_path : lower_path.substr(sep + 1);
    
    if (filename == ".mounted" || filename.find(".mounted:") == 0) {
        return true;
    }
#else
    size_t sep = path.find_last_of('/');
    std::string filename = (sep == std::string::npos) ? path : path.substr(sep + 1);
    if (filename == ".mounted") {
        return true;
    }
#endif
    return false;
}

File::File() : last_error_(0) {
}

File::~File() {
    if (is_open()) {
        close();
    }
}

bool File::open(const std::string& path, std::ios_base::openmode mode) {
    if (is_target_mounted_file(path)) {
        last_error_ = 5; // Access Denied
        return false;
    }

    path_ = path;
    stream_.open(path, mode);
    update_error_state();
    return is_open();
}

void File::close() {
    if (stream_.is_open()) {
        stream_.close();
        update_error_state();
    }
}

std::streamsize File::read(void* buffer, std::streamsize size) {
    if (!is_open()) {
        last_error_ = -1;
        return -1;
    }
    
    stream_.read(static_cast<char*>(buffer), size);
    update_error_state();
    
    // gcount() returns the number of characters extracted by the last unformatted input operation
    return stream_.gcount();
}

std::streamsize File::write(const void* buffer, std::streamsize size) {
    if (!is_open()) {
        last_error_ = -1;
        return -1;
    }
    
    // Ensure clear state before writing
    stream_.clear();
    std::streampos pos_before = stream_.tellp();
    
    stream_.write(static_cast<const char*>(buffer), size);
    update_error_state();
    
    if (stream_.fail() && !stream_.bad()) {
        // Return whatever was written before the failure (if measurable)
        return -1;
    }
    
    return size;
}

bool File::seek(std::streamoff offset, std::ios_base::seekdir dir) {
    if (!is_open()) {
        last_error_ = -1;
        return false;
    }
    
    stream_.clear(); // clear any EOF flags before seeking
    stream_.seekg(offset, dir);
    stream_.seekp(offset, dir);
    update_error_state();
    
    return !stream_.fail();
}

std::streamsize File::size() {
    if (!is_open()) {
        last_error_ = -1;
        return -1;
    }
    
    stream_.clear();
    
    // Save current position
    std::streampos current_pos = stream_.tellg();
    if (current_pos == std::streampos(-1)) {
        current_pos = stream_.tellp();
    }
    
    // Seek to end to get size
    stream_.seekg(0, std::ios::end);
    std::streamsize file_size = stream_.tellg();
    
    // Restore position
    stream_.seekg(current_pos);
    stream_.seekp(current_pos);
    
    update_error_state();
    
    return file_size;
}

bool File::is_open() const {
    return stream_.is_open();
}

int File::get_last_error() const {
    return last_error_;
}

const std::string& File::get_path() const {
    return path_;
}

void File::update_error_state() {
    if (stream_.bad()) {
        last_error_ = 2; // Critical I/O error
    } else if (stream_.fail()) {
        last_error_ = 1; // Logical error (e.g. format error, file not found)
    } else {
        last_error_ = 0; // Success
    }
}

} // namespace rfs
