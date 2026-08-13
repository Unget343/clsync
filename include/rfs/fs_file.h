#pragma once

#include <fstream>
#include <string>

namespace rfs {

bool is_target_mounted_file(const std::string& path);

class File {
protected:
    std::fstream stream_;
    std::string path_;
    int last_error_;

public:
    File();
    virtual ~File();

    // Basic file operations
    virtual bool open(const std::string& path, std::ios_base::openmode mode);
    virtual void close();
    
    virtual std::streamsize read(void* buffer, std::streamsize size);
    virtual std::streamsize write(const void* buffer, std::streamsize size);
    
    // Changing the pointer position
    virtual bool seek(std::streamoff offset, std::ios_base::seekdir dir);
    
    // Getting the file size
    virtual std::streamsize size();

    // Status checks
    bool is_open() const;
    int get_last_error() const;
    const std::string& get_path() const;

protected:
    // Update internal error state based on stream status
    void update_error_state();
};

} // namespace rfs
