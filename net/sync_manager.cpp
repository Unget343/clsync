#include "sync_manager.h"

#include "log/log.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace clsync {
namespace {

bool is_descendant_of(const std::filesystem::path& child, const std::filesystem::path& parent) {
    auto child_part = child.begin();
    for (auto parent_part = parent.begin(); parent_part != parent.end(); ++parent_part, ++child_part) {
        if (child_part == child.end() || *child_part != *parent_part) return false;
    }
    return true;
}

std::string state_name(ServiceState state) {
    switch (state) {
    case ServiceState::Connecting: return "CONNECTING";
    case ServiceState::Connected: return "CONNECTED";
    case ServiceState::Disconnected: return "DISCONNECTED";
    case ServiceState::Error: return "ERROR";
    case ServiceState::Stopping: return "STOPPING";
    }
    return "UNKNOWN";
}

bool valid_checksum(const std::string& value) {
    return value.empty() || (value.size() == 64 && std::all_of(value.begin(), value.end(),
        [](unsigned char character) { return std::isxdigit(character) != 0; }));
}

bool is_http_url(const std::string& value) {
    std::string scheme = value.substr(0, value.find(':'));
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return scheme == "http" || scheme == "https";
}

} // namespace

SyncManager::SyncManager(SyncConfig config) : config_(std::move(config)) {}

SyncManager::~SyncManager() {
    stop();
}

bool SyncManager::start(std::string& error) {
    std::lock_guard lock(mutex_);
    if (started_) return true;
    if (config_.local_root.empty() || config_.health_url.empty()) {
        error = "local_root and health_url are required.";
        return false;
    }
    if (config_.small_file_workers == 0 || config_.large_file_workers == 0 || config_.network_timeout_ms <= 0) {
        error = "worker counts and network timeout must be positive.";
        return false;
    }

    std::error_code filesystem_error;
    std::filesystem::create_directories(config_.local_root, filesystem_error);
    if (filesystem_error) {
        error = "Cannot create local root: " + filesystem_error.message();
        return false;
    }

    stopping_ = false;
    started_ = true;
    service_state_.store(ServiceState::Connecting);
    for (unsigned int index = 0; index < config_.small_file_workers; ++index)
        workers_.emplace_back(&SyncManager::worker_loop, this, false);
    for (unsigned int index = 0; index < config_.large_file_workers; ++index)
        workers_.emplace_back(&SyncManager::worker_loop, this, true);
    monitor_ = std::thread(&SyncManager::monitor_loop, this);
    output("sync", 0, "sync manager started");
    return true;
}

void SyncManager::stop() {
    {
        std::lock_guard lock(mutex_);
        if (!started_) return;
        stopping_ = true;
        started_ = false;
        for (auto& [_, result] : results_) {
            if (result.state == TaskState::Pending || result.state == TaskState::Retry)
                result.state = TaskState::Cancelled;
        }
        small_queue_.clear();
        large_queue_.clear();
    }
    set_service_state(ServiceState::Stopping);
    queue_cv_.notify_all();
    idle_cv_.notify_all();
    if (monitor_.joinable()) monitor_.join();
    for (auto& worker : workers_) worker.join();
    workers_.clear();
    output("sync", 0, "sync manager stopped");
}

bool SyncManager::is_large_task(const SyncTask& task, std::string& error) const {
    if (task.direction == SyncDirection::Download) {
        if (!resolve_local_path(task.local_path, false, error)) return false;
        return task.validation.expected_size && *task.validation.expected_size >= config_.large_file_threshold;
    }

    const auto path = resolve_local_path(task.local_path, false, error);
    if (!path) return false;
    std::error_code filesystem_error;
    const auto size = std::filesystem::file_size(*path, filesystem_error);
    if (filesystem_error) {
        error = "Cannot read upload file size: " + filesystem_error.message();
        return false;
    }
    return size >= config_.large_file_threshold;
}

std::optional<std::filesystem::path> SyncManager::resolve_local_path(
    const std::filesystem::path& path, bool create_parent, std::string& error) const {
    const auto normalized = path.lexically_normal();
    if (normalized.empty() || normalized.is_absolute() || normalized == ".") {
        error = "Local path must be a non-empty relative path.";
        return std::nullopt;
    }
    for (const auto& part : normalized) {
        if (part == "..") {
            error = "Local path must not escape local_root.";
            return std::nullopt;
        }
    }

    std::error_code filesystem_error;
    const auto root = std::filesystem::weakly_canonical(config_.local_root, filesystem_error);
    if (filesystem_error) {
        error = "Cannot resolve local root: " + filesystem_error.message();
        return std::nullopt;
    }
    const auto candidate = root / normalized;
    if (create_parent) std::filesystem::create_directories(candidate.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Cannot create output directory: " + filesystem_error.message();
        return std::nullopt;
    }
    const auto parent = std::filesystem::weakly_canonical(candidate.parent_path(), filesystem_error);
    if (filesystem_error || !is_descendant_of(parent, root)) {
        error = "Local path resolves outside local_root.";
        return std::nullopt;
    }
    return candidate;
}

bool SyncManager::enqueue(SyncTask task, std::string& error) {
    if (task.id.empty() || !is_http_url(task.url) || task.local_path.empty() || !valid_checksum(task.validation.expected_sha256)) {
        error = "Task requires id, HTTP(S) URL, relative local path, and a valid optional SHA-256.";
        return false;
    }
    const bool large = is_large_task(task, error);
    if (!error.empty()) return false;
    const auto task_id = task.id;

    {
        std::lock_guard lock(mutex_);
        if (!started_ || stopping_) {
            error = "Sync manager is not running.";
            return false;
        }
        if (results_.find(task.id) != results_.end()) {
            error = "Task id already exists.";
            return false;
        }
        results_.emplace(task_id, SyncResult{std::move(task)});
        (large ? large_queue_ : small_queue_).push_back({task_id, large});
    }
    output("sync", 0, "queued task " + task_id);
    queue_cv_.notify_all();
    return true;
}

bool SyncManager::enqueue_ipc_command(const std::string& command, std::string& error) {
    std::istringstream stream(command);
    std::string operation;
    SyncTask task;
    std::string expected_size;
    std::string expected_sha256;
    stream >> operation;
    if (operation == "STOP") {
        stop();
        return true;
    }
    if (operation == "DOWNLOAD") {
        task.direction = SyncDirection::Download;
        stream >> task.id >> task.url >> task.local_path >> expected_size >> expected_sha256;
        if (expected_size != "-" && !expected_size.empty()) {
            try { task.validation.expected_size = std::stoull(expected_size); }
            catch (const std::exception&) { error = "Invalid expected size."; return false; }
        }
        if (expected_sha256 != "-" && !expected_sha256.empty()) task.validation.expected_sha256 = expected_sha256;
    } else if (operation == "UPLOAD") {
        task.direction = SyncDirection::Upload;
        stream >> task.id >> task.url >> task.local_path;
    } else {
        error = "Unsupported IPC command.";
        return false;
    }
    if (stream.fail()) {
        error = "Malformed IPC command.";
        return false;
    }
    return enqueue(std::move(task), error);
}

std::optional<SyncManager::QueueItem> SyncManager::take_task(bool large) {
    std::unique_lock lock(mutex_);
    auto& queue = large ? large_queue_ : small_queue_;
    while (!stopping_) {
        if (service_state_.load() != ServiceState::Connected) {
            queue_cv_.wait(lock, [&] { return stopping_ || service_state_.load() == ServiceState::Connected; });
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto available = std::find_if(queue.begin(), queue.end(), [&](const QueueItem& item) {
            return item.ready_at <= now;
        });
        if (available != queue.end()) {
            const auto item = *available;
            queue.erase(available);
            auto& result = results_.at(item.id);
            result.state = TaskState::InProgress;
            ++result.attempts;
            ++active_tasks_;
            return item;
        }

        if (queue.empty()) {
            queue_cv_.wait(lock, [&] {
                return stopping_ || service_state_.load() != ServiceState::Connected || !queue.empty();
            });
        } else {
            const auto next_ready = std::min_element(queue.begin(), queue.end(), [](const QueueItem& left, const QueueItem& right) {
                return left.ready_at < right.ready_at;
            });
            queue_cv_.wait_until(lock, next_ready->ready_at);
        }
    }
    return std::nullopt;
}

void SyncManager::worker_loop(bool large) {
    Network network(config_.network_timeout_ms, 0, config_.user_agent);
    while (const auto item = take_task(large)) process_task(*item, network);
}

void SyncManager::monitor_loop() {
    Network network(config_.network_timeout_ms, 0, config_.user_agent);
    while (true) {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) return;
        }
        std::string response;
        const bool connected = network.http_get(config_.health_url, response);
        set_service_state(connected ? ServiceState::Connected : ServiceState::Disconnected);
        if (!connected) output("sync", -1, "service health check failed: " + response);

        std::unique_lock lock(mutex_);
        if (queue_cv_.wait_for(lock, config_.monitor_interval, [&] { return stopping_; })) return;
    }
}

void SyncManager::process_task(const QueueItem& item, Network& network) {
    SyncTask task;
    unsigned int attempts;
    {
        std::lock_guard lock(mutex_);
        const auto& result = results_.at(item.id);
        task = result.task;
        attempts = result.attempts;
    }
    output("sync", 0, "starting task " + item.id);

    std::string error;
    const auto local_path = resolve_local_path(task.local_path, task.direction == SyncDirection::Download, error);
    if (!local_path) {
        complete_task(item.id, TaskState::Failed, error, 0);
        output("sync", -1, "failed task " + item.id + ": " + error);
        return;
    }
    output("sync", 0, "transferring task " + item.id);
    bool success = false;
    success = task.direction == SyncDirection::Download
        ? network.http_download_file(task.url, local_path->string(), error, task.validation)
        : network.http_upload_file(task.url, local_path->string(), error);
    const auto status = network.last_http_status();
    if (success) {
        complete_task(item.id, TaskState::Completed, "", status);
        output("sync", 0, "completed task " + item.id + " http=" + std::to_string(status));
        return;
    }

    if (is_retryable(status, error) && attempts <= config_.max_retries) {
        if (status == 0 || status >= 500) set_service_state(ServiceState::Disconnected);
        requeue_task(item, error, status);
        output("sync", -1, "retrying task " + item.id + " attempt=" + std::to_string(attempts) +
                            " http=" + std::to_string(status) + ": " + error);
        return;
    }
    complete_task(item.id, TaskState::Failed, error, status);
    output("sync", -1, "failed task " + item.id + " attempt=" + std::to_string(attempts) +
                        " http=" + std::to_string(status) + ": " + error);
}

void SyncManager::complete_task(const std::string& id, TaskState state, std::string error, long http_status) {
    ResultCallback callback;
    SyncResult result;
    {
        std::lock_guard lock(mutex_);
        auto& stored = results_.at(id);
        stored.state = state;
        stored.error = std::move(error);
        stored.http_status = http_status;
        result = stored;
        callback = result_callback_;
        --active_tasks_;
        if (active_tasks_ == 0 && small_queue_.empty() && large_queue_.empty()) idle_cv_.notify_all();
    }
    if (callback) callback(result);
}

void SyncManager::requeue_task(const QueueItem& item, std::string error, long http_status) {
    ResultCallback callback;
    SyncResult result;
    {
        std::lock_guard lock(mutex_);
        auto& stored = results_.at(item.id);
        stored.state = TaskState::Retry;
        stored.error = std::move(error);
        stored.http_status = http_status;
        if (stopping_) {
            stored.state = TaskState::Cancelled;
        } else {
            auto retry_item = item;
            retry_item.ready_at = std::chrono::steady_clock::now() + config_.retry_delay;
            (item.large ? large_queue_ : small_queue_).push_back(std::move(retry_item));
        }
        result = stored;
        callback = result_callback_;
        --active_tasks_;
        if (active_tasks_ == 0 && small_queue_.empty() && large_queue_.empty()) idle_cv_.notify_all();
    }
    if (callback) callback(result);
    queue_cv_.notify_all();
}

bool SyncManager::wait_for_idle(std::chrono::milliseconds timeout) {
    std::unique_lock lock(mutex_);
    return idle_cv_.wait_for(lock, timeout, [&] {
        return active_tasks_ == 0 && small_queue_.empty() && large_queue_.empty();
    });
}

ServiceState SyncManager::service_state() const {
    return service_state_.load();
}

std::optional<SyncResult> SyncManager::result(const std::string& task_id) const {
    std::lock_guard lock(mutex_);
    const auto found = results_.find(task_id);
    return found == results_.end() ? std::nullopt : std::optional<SyncResult>(found->second);
}

std::vector<SyncResult> SyncManager::results() const {
    std::lock_guard lock(mutex_);
    std::vector<SyncResult> output;
    output.reserve(results_.size());
    for (const auto& [_, result] : results_) output.push_back(result);
    return output;
}

void SyncManager::set_result_callback(ResultCallback callback) {
    std::lock_guard lock(mutex_);
    result_callback_ = std::move(callback);
}

void SyncManager::set_service_state(ServiceState state) {
    const auto previous = service_state_.exchange(state);
    if (previous != state) output("sync", 0, "service state " + state_name(state));
    queue_cv_.notify_all();
}

bool SyncManager::is_retryable(long http_status, const std::string& error) {
    return http_status == 0 || http_status == 408 || http_status == 425 || http_status == 429 ||
           http_status >= 500 || error.find("validation failed") != std::string::npos;
}

} // namespace clsync
