#pragma once

#include <curl/curl.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
using namespace std;

struct DownloadValidation {
    optional<uintmax_t> expected_size;
    string expected_sha256;
};

class Network {
private:
    CURL *curl;
    long timeout_ms_;
    unsigned int retries_;
    string user_agent_;
    long last_http_status_ = 0;

    void configure_request(const string& url, string& response);
    bool perform(const function<void()>& configure, string& response);
public:
    explicit Network(long timeout_ms = 30000, unsigned int retries = 0, string user_agent = "clsync/1.0");
    ~Network();

    long last_http_status() const { return last_http_status_; }

    bool http_post(const string& url, const string& data, string& response);
    bool http_get(const string& url, string& response);
    bool http_put(const string& url, const string& data, string& response);
    bool http_delete(const string& url, string& response);
    bool http_upload_file(const string& url, const string& file_path, string& response);
    bool http_download_file(const string& url, const string& file_path, string& response,
                            const DownloadValidation& validation = {});
};
