#include "../include/log/log.h"
#include "../include/reborn/reborn.h"
#include "sync_manager.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>

namespace {

std::string task_state_name(clsync::TaskState state) {
    switch (state) {
    case clsync::TaskState::Pending: return "PENDING";
    case clsync::TaskState::InProgress: return "IN_PROGRESS";
    case clsync::TaskState::Completed: return "COMPLETED";
    case clsync::TaskState::Failed: return "FAILED";
    case clsync::TaskState::Retry: return "RETRY";
    case clsync::TaskState::Cancelled: return "CANCELLED";
    }
    return "UNKNOWN";
}

bool set_unsigned_from_environment(const char* name, unsigned int& value, std::string& error, bool allow_zero = false) {
    const char* raw = std::getenv(name);
    if (!raw) return true;
    while (std::isspace(static_cast<unsigned char>(*raw))) ++raw;
    if (*raw == '\0' || *raw == '-') {
        error = std::string("Invalid ") + name;
        return false;
    }
    try {
        std::size_t pos = 0;
        const auto parsed = std::stoul(raw, &pos);
        while (raw[pos] != '\0') {
            if (!std::isspace(static_cast<unsigned char>(raw[pos]))) {
                error = std::string("Invalid ") + name;
                return false;
            }
            ++pos;
        }
        if ((!allow_zero && parsed == 0) || parsed > (std::numeric_limits<unsigned int>::max)()) {
            throw std::out_of_range("range");
        }
        value = static_cast<unsigned int>(parsed);
        return true;
    } catch (const std::exception&) {
        error = std::string("Invalid ") + name;
        return false;
    }
}

bool load_configuration(clsync::SyncConfig& config, std::string& error) {
    const auto user_agent = std::getenv("CLSYNC_USER_AGENT");
    if (user_agent) config.user_agent = user_agent;
    if (!set_unsigned_from_environment("CLSYNC_SMALL_WORKERS", config.small_file_workers, error, false) ||
        !set_unsigned_from_environment("CLSYNC_LARGE_WORKERS", config.large_file_workers, error, false) ||
        !set_unsigned_from_environment("CLSYNC_MAX_RETRIES", config.max_retries, error, true)) return false;

    unsigned int timeout_ms = 0;
    if (set_unsigned_from_environment("CLSYNC_TIMEOUT_MS", timeout_ms, error, false)) {
        if (timeout_ms) config.network_timeout_ms = timeout_ms;
    } else return false;

    unsigned int retry_delay_ms = 0;
    if (set_unsigned_from_environment("CLSYNC_RETRY_DELAY_MS", retry_delay_ms, error, true)) {
        if (retry_delay_ms) config.retry_delay = std::chrono::milliseconds(retry_delay_ms);
    } else return false;

    unsigned int monitor_interval_ms = 0;
    if (set_unsigned_from_environment("CLSYNC_MONITOR_INTERVAL_MS", monitor_interval_ms, error, false)) {
        if (monitor_interval_ms) config.monitor_interval = std::chrono::milliseconds(monitor_interval_ms);
    } else return false;

    const char* threshold = std::getenv("CLSYNC_LARGE_FILE_THRESHOLD");
    if (threshold) {
        while (std::isspace(static_cast<unsigned char>(*threshold))) ++threshold;
        if (*threshold == '\0' || *threshold == '-') {
            error = "Invalid CLSYNC_LARGE_FILE_THRESHOLD";
            return false;
        }
        try {
            std::size_t pos = 0;
            const auto parsed = std::stoull(threshold, &pos);
            while (threshold[pos] != '\0') {
                if (!std::isspace(static_cast<unsigned char>(threshold[pos]))) {
                    error = "Invalid CLSYNC_LARGE_FILE_THRESHOLD";
                    return false;
                }
                ++pos;
            }
            config.large_file_threshold = parsed;
        } catch (const std::exception&) {
            error = "Invalid CLSYNC_LARGE_FILE_THRESHOLD";
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: clsync <local-root> <health-url> <IPC command> [IPC command ...]\n";
        return 2;
    }

    clsync::SyncConfig config;
    config.local_root = argv[1];
    config.health_url = argv[2];
    std::string error;
    if (!load_configuration(config, error)) {
        output("sync", -1, error);
        return 2;
    }

    const char* socket_path = std::getenv("CLSYNC_SOCKET_PATH");
    auto socket = std::make_shared<reborn::Socket<reborn::Request>>(socket_path ? socket_path : "clsync.sock");
    if (!*socket) {
        output("sync", -1, "failed to create Reborn IPC socket");
        return 1;
    }

    auto ipc_mutex = std::make_shared<std::mutex>();
    clsync::SyncManager manager(std::move(config));
    manager.set_result_callback([socket, ipc_mutex](const clsync::SyncResult& result) {
        const auto message = "SYNC " + result.task.id + " " + task_state_name(result.state) +
                             " attempts=" + std::to_string(result.attempts);
        std::lock_guard lock(*ipc_mutex);
        socket->sendRequest(message.c_str());
    });
    if (!manager.start(error)) {
        output("sync", -1, error);
        return 1;
    }

    {
        std::lock_guard lock(*ipc_mutex);
        socket->sendRequest("SYNC READY");
    }
    for (int index = 3; index < argc; ++index) {
        if (!manager.enqueue_ipc_command(argv[index], error)) {
            output("sync", -1, error);
            {
                std::lock_guard lock(*ipc_mutex);
                socket->sendRequest("SYNC ERROR");
                socket->processRequests();
            }
            manager.stop();
            return 1;
        }
    }

    const bool completed = manager.wait_for_idle(std::chrono::minutes(5));
    bool succeeded = completed;
    for (const auto& result : manager.results())
        succeeded = succeeded && result.state == clsync::TaskState::Completed;
    {
        std::lock_guard lock(*ipc_mutex);
        socket->sendRequest(succeeded ? "SYNC COMPLETED" : "SYNC FAILED");
        manager.stop();
        socket->processRequests();
    }
    return succeeded ? 0 : 1;
}
