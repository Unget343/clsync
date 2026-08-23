#pragma once

#include "network.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace clsync {

enum class SyncDirection { Download, Upload };
enum class TaskState { Pending, InProgress, Completed, Failed, Retry, Cancelled };
enum class ServiceState { Connecting, Connected, Disconnected, Error, Stopping };

struct SyncConfig {
    std::filesystem::path local_root;
    std::string health_url;
    std::string user_agent = "clsync/1.0";
    std::uintmax_t large_file_threshold = 8 * 1024 * 1024;
    unsigned int small_file_workers = 2;
    unsigned int large_file_workers = 2;
    unsigned int max_retries = 3;
    long network_timeout_ms = 30000;
    std::chrono::milliseconds retry_delay{500};
    std::chrono::milliseconds monitor_interval{1000};
};

struct SyncTask {
    std::string id;
    SyncDirection direction;
    std::string url;
    std::filesystem::path local_path;
    DownloadValidation validation;
};

struct SyncResult {
    SyncTask task;
    TaskState state = TaskState::Pending;
    unsigned int attempts = 0;
    long http_status = 0;
    std::string error;
};

class SyncManager {
public:
    using ResultCallback = std::function<void(const SyncResult&)>;

    explicit SyncManager(SyncConfig config);
    ~SyncManager();

    bool start(std::string& error);
    void stop();

    bool enqueue(SyncTask task, std::string& error);
    bool enqueue_ipc_command(const std::string& command, std::string& error);
    bool wait_for_idle(std::chrono::milliseconds timeout);

    ServiceState service_state() const;
    std::optional<SyncResult> result(const std::string& task_id) const;
    std::vector<SyncResult> results() const;
    void set_result_callback(ResultCallback callback);

private:
    struct QueueItem {
        std::string id;
        bool large;
        std::chrono::steady_clock::time_point ready_at = std::chrono::steady_clock::now();
    };

    bool is_large_task(const SyncTask& task, std::string& error) const;
    std::optional<std::filesystem::path> resolve_local_path(const std::filesystem::path& path,
                                                            bool create_parent, std::string& error) const;
    std::optional<QueueItem> take_task(bool large);
    void worker_loop(bool large);
    void monitor_loop();
    void process_task(const QueueItem& item, Network& network);
    void complete_task(const std::string& id, TaskState state, std::string error, long http_status);
    void requeue_task(const QueueItem& item, std::string error, long http_status);
    void set_service_state(ServiceState state);
    static bool is_retryable(long http_status, const std::string& error);

    SyncConfig config_;
    std::atomic<ServiceState> service_state_{ServiceState::Disconnected};
    mutable std::mutex mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable idle_cv_;
    std::deque<QueueItem> small_queue_;
    std::deque<QueueItem> large_queue_;
    std::unordered_map<std::string, SyncResult> results_;
    ResultCallback result_callback_;
    std::vector<std::thread> workers_;
    std::thread monitor_;
    bool started_ = false;
    bool stopping_ = false;
    std::size_t active_tasks_ = 0;
};

} // namespace clsync
