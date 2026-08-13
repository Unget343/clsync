#pragma once

#include "../include/curl/include/curl/curl.h"
#include <string>
#include <vector>
using namespace std;

class Network {
private:
    CURL *curl;
    curl_mime *form = nullptr;
    struct curl_slist *headers = nullptr;
public:
    Network();
    ~Network();

    bool http_post(const string& url, const string& data, string& response);
    bool http_get(const string& url, string& response);
    bool http_put(const string& url, const string& data, string& response);
    bool http_delete(const string& url, string& response);
    bool http_upload_file(const string& url, const string& file_path, string& response);
    bool http_download_file(const string& url, const string& file_path, string& response);
};
