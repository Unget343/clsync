#include "network.h"
#include "sha256.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace {

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    const size_t total_size = size * nmemb;
    if (userp) {
        static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total_size);
    }
    return total_size;
}

size_t FileWriteCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    const size_t total_size = size * nmemb;
    if (!stream) return 0;

    auto* out_file = static_cast<std::ofstream*>(stream);
    out_file->write(static_cast<char*>(ptr), total_size);
    return out_file->good() ? total_size : 0;
}

std::once_flag curl_initialized;

} // namespace

Network::Network(long timeout_ms, unsigned int retries, string user_agent)
    : curl(nullptr), timeout_ms_(timeout_ms), retries_(retries),
      user_agent_(user_agent.empty() ? "clsync/1.0" : std::move(user_agent)) {
    std::call_once(curl_initialized, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    curl = curl_easy_init();
}

Network::~Network() {
    if (curl) {
        curl_easy_cleanup(curl);
        curl = nullptr;
    }
}

void Network::configure_request(const string& url, string& response) {
    response.clear();
    last_http_status_ = 0;
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent_.c_str());
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
}

bool Network::perform(const function<void()>& configure, string& response) {
    if (!curl) return false;

    CURLcode result = CURLE_FAILED_INIT;
    for (unsigned int attempt = 0; attempt <= retries_; ++attempt) {
        configure();
        result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &last_http_status_);
        if (result == CURLE_OK && (last_http_status_ == 0 || (last_http_status_ >= 200 && last_http_status_ < 300)))
            return true;
        if (result == CURLE_OK)
            response = "Unexpected HTTP status: " + std::to_string(last_http_status_);
    }

    if (response.empty()) response = curl_easy_strerror(result);
    return false;
}

bool Network::http_get(const string& url, string& response) {
    return perform([&] { configure_request(url, response); }, response);
}

bool Network::http_post(const string& url, const string& data, string& response) {
    return perform([&] {
        configure_request(url, response);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(data.size()));
    }, response);
}

bool Network::http_put(const string& url, const string& data, string& response) {
    return perform([&] {
        configure_request(url, response);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(data.size()));
    }, response);
}

bool Network::http_delete(const string& url, string& response) {
    return perform([&] {
        configure_request(url, response);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }, response);
}

bool Network::http_upload_file(const string& url, const string& file_path, string& response) {
    if (!curl) return false;

    std::error_code error;
    if (!std::filesystem::is_regular_file(file_path, error)) {
        response = "File does not exist.";
        return false;
    }

    curl_mime* form = nullptr;
    const bool success = perform([&] {
        if (form) curl_mime_free(form);
        configure_request(url, response);
        form = curl_mime_init(curl);
        curl_mimepart* field = curl_mime_addpart(form);
        curl_mime_name(field, "file");
        curl_mime_filedata(field, file_path.c_str());
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);
    }, response);
    if (form) curl_mime_free(form);
    return success;
}

bool Network::http_download_file(const string& url, const string& file_path, string& response,
                                 const DownloadValidation& validation) {
    if (!curl) return false;

    const std::filesystem::path target(file_path);
    const std::filesystem::path partial = target.string() + ".part";
    std::error_code error;

    for (unsigned int attempt = 0; attempt <= retries_; ++attempt) {
        std::filesystem::remove(partial, error);
        std::ofstream out_file(partial, std::ios::binary | std::ios::trunc);
        if (!out_file.is_open()) {
            response = "Failed to open file for writing.";
            return false;
        }

        configure_request(url, response);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FileWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out_file);
        const CURLcode result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &last_http_status_);
        out_file.close();

        if (result == CURLE_OK && (last_http_status_ == 0 || (last_http_status_ >= 200 && last_http_status_ < 300))) {
            error.clear();
            const auto actual_size = std::filesystem::file_size(partial, error);
            if (error || (validation.expected_size && actual_size != *validation.expected_size)) {
                response = "File validation failed: expected_size=" +
                           (validation.expected_size ? std::to_string(*validation.expected_size) : "unknown") +
                           " actual_size=" + (error ? "unavailable" : std::to_string(actual_size));
                continue;
            }
            if (!validation.expected_sha256.empty()) {
                const auto actual_sha256 = clsync::crypto::sha256_file(partial);
                auto expected_sha256 = validation.expected_sha256;
                std::transform(expected_sha256.begin(), expected_sha256.end(), expected_sha256.begin(),
                               [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
                if (!actual_sha256 || *actual_sha256 != expected_sha256) {
                    response = "File validation failed: SHA-256 mismatch.";
                    continue;
                }
            }
            std::filesystem::remove(target, error);
            std::filesystem::rename(partial, target, error);
            if (!error) {
                response = "Success";
                return true;
            }
            response = error.message();
        } else if (result != CURLE_OK) {
            response = curl_easy_strerror(result);
        } else {
            response = "Unexpected HTTP status: " + std::to_string(last_http_status_);
        }
    }

    std::filesystem::remove(partial, error);
    return false;
}
