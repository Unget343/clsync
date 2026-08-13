#include "network.h"
#include <fstream>
#include <iostream>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    if (userp) {
        ((string*)userp)->append((char*)contents, total_size);
    }
    return total_size;
}

static size_t FileWriteCallback(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t total_size = size * nmemb;
    if (stream) {
        ((std::ofstream*)stream)->write((char*)ptr, total_size);
    }
    return total_size;
}

Network::Network() {
    // curl_global_init is typically called once per application lifetime.
    // Calling it here for simplicity, but in a multi-threaded complex app, 
    // it's better initialized in main().
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
}

Network::~Network() {
    if (headers) {
        curl_slist_free_all(headers);
        headers = nullptr;
    }
    if (form) {
        curl_mime_free(form);
        form = nullptr;
    }
    if (curl) {
        curl_easy_cleanup(curl);
        curl = nullptr;
    }
    curl_global_cleanup();
}

bool Network::http_get(const string& url, string& response) {
    if (!curl) return false;
    
    response.clear();
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    return res == CURLE_OK;
}

bool Network::http_post(const string& url, const string& data, string& response) {
    if (!curl) return false;
    
    response.clear();
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    
    // Explicitly set POST method (though CURLOPT_POSTFIELDS implies it)
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    return res == CURLE_OK;
}

bool Network::http_put(const string& url, const string& data, string& response) {
    if (!curl) return false;
    
    response.clear();
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    return res == CURLE_OK;
}

bool Network::http_delete(const string& url, string& response) {
    if (!curl) return false;
    
    response.clear();
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    return res == CURLE_OK;
}

bool Network::http_upload_file(const string& url, const string& file_path, string& response) {
    if (!curl) return false;
    
    if (form) {
        curl_mime_free(form);
        form = nullptr;
    }
    
    response.clear();
    curl_easy_reset(curl);
    
    form = curl_mime_init(curl);
    curl_mimepart *field = curl_mime_addpart(form);
    curl_mime_name(field, "file");
    curl_mime_filedata(field, file_path.c_str());
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    
    curl_mime_free(form);
    form = nullptr;
    
    return res == CURLE_OK;
}

bool Network::http_download_file(const string& url, const string& file_path, string& response) {
    if (!curl) return false;
    
    std::ofstream out_file(file_path, std::ios::binary);
    if (!out_file.is_open()) {
        response = "Failed to open file for writing.";
        return false;
    }
    
    response.clear();
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FileWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_file);
    
    CURLcode res = curl_easy_perform(curl);
    out_file.close();
    
    if (res == CURLE_OK) {
        response = "Success";
        return true;
    } else {
        response = curl_easy_strerror(res);
        return false;
    }
}
