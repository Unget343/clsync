#include "log/log.h"
#include "network.h"
#include "reborn/reborn.h"
#include "rfs/fs_file.h"
#include "rfs/ntfs/ntfs.h"
#include "sync_manager.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

namespace {

int report(bool success, const std::string& response) {
    std::cout << (success ? "OK" : "ERR") << '\n' << response;
    return success ? 0 : 1;
}

int run_network(int argc, char* argv[]) {
    if (argc < 4) return 2;

    const std::string action(argv[2]);
    const std::string url(argv[3]);
    Network network(action == "retry-get" ? 1000 : action == "timeout-get" ? 50 : 30000,
                    action == "retry-get" ? 1 : 0);
    std::string response;

    if (action == "get" || action == "retry-get" || action == "timeout-get")
        return report(network.http_get(url, response), response);
    if (action == "delete")
        return report(network.http_delete(url, response), response);
    if (argc < 5) return 2;
    if (action == "post")
        return report(network.http_post(url, argv[4], response), response);
    if (action == "put")
        return report(network.http_put(url, argv[4], response), response);
    if (action == "upload")
        return report(network.http_upload_file(url, argv[4], response), response);
    if (action == "download")
        return report(network.http_download_file(url, argv[4], response), response);
    return 2;
}

int run_file(int argc, char* argv[]) {
    if (argc < 4) return 2;

    const std::string action(argv[2]);
    const std::string path(argv[3]);
    if (action == "reject-mounted") {
        rfs::File file;
        return report(!file.open(path, std::ios::in | std::ios::binary), "");
    }
    if (action != "roundtrip" || argc < 5) return 2;

    const std::string expected(argv[4]);
    rfs::File file;
    if (!file.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc))
        return report(false, "open failed");
    if (file.write(expected.data(), static_cast<std::streamsize>(expected.size())) !=
        static_cast<std::streamsize>(expected.size()))
        return report(false, "write failed");
    if (!file.seek(0, std::ios::beg)) return report(false, "seek failed");

    std::string actual(expected.size(), '\0');
    const auto bytes_read = file.read(actual.data(), static_cast<std::streamsize>(actual.size()));
    return report(bytes_read == static_cast<std::streamsize>(expected.size()) && actual == expected, actual);
}

int run_ntfs(int argc, char* argv[]) {
    if (argc != 4 || std::string(argv[2]) != "reject-mounted") return 2;
    rfs::ntfs::NtfsFile file;
    return report(!file.open(argv[3], std::ios::in | std::ios::binary), "");
}

int run_ipc(int argc, char* argv[]) {
    if (argc < 4) return 2;

    const std::string action(argv[2]);
    reborn::Socket<reborn::Request> socket(argv[3]);
    if (!socket) return report(false, "socket creation failed");

    if (action == "invalid")
        return report(socket.sendRequest("") == ERR && socket.processRequests() == ERR, "");
    if (action == "many" && argc == 5) {
        const auto count = std::stoul(argv[4]);
        for (unsigned long index = 0; index < count; ++index)
            if (socket.sendRequest(("SYNC " + std::to_string(index)).c_str()) != OK) return report(false, "enqueue failed");
        return report(socket.processRequests() == OK, "");
    }
    if (action != "message" || argc != 5) return 2;

    const bool success = socket.sendRequest(argv[4]) == OK && socket.processRequests() == OK &&
                         std::string(socket.getAnswer()) == "OK";
    socket.close();
    return report(success, socket.getAnswer() ? socket.getAnswer() : "");
}

int run_workflow(int argc, char* argv[]) {
    if (argc != 6) return 2;

    reborn::Socket<reborn::Request> socket(argv[2]);
    if (!socket || socket.sendRequest("SYNC") != OK || socket.processRequests() != OK)
        return report(false, "IPC failed");

    Network network;
    std::string response;
    const std::string upload_url = std::string(argv[4]) + "/upload";
    const std::string download_url = std::string(argv[4]) + "/files/uploaded";
    if (!network.http_upload_file(upload_url, argv[3], response)) return report(false, response);
    if (!network.http_download_file(download_url, argv[5], response)) return report(false, response);
    socket.close();
    return report(true, response);
}

int run_sync(int argc, char* argv[]) {
    if (argc < 5) return 2;

    const std::string action(argv[2]);
    clsync::SyncConfig config;
    config.local_root = argv[3];
    config.health_url = argv[4];
    config.monitor_interval = std::chrono::milliseconds(50);
    config.retry_delay = std::chrono::milliseconds(50);
    clsync::SyncManager manager(config);
    std::string error;
    if (!manager.start(error)) return report(false, error);

    bool accepted = false;
    if (action == "download" && argc >= 7) {
        clsync::SyncTask task{"task", clsync::SyncDirection::Download, argv[5], argv[6]};
        if (argc >= 8 && std::string(argv[7]) != "-") task.validation.expected_size = std::stoull(argv[7]);
        if (argc >= 9 && std::string(argv[8]) != "-") task.validation.expected_sha256 = argv[8];
        accepted = manager.enqueue(std::move(task), error);
    } else if (action == "upload" && argc == 7) {
        accepted = manager.enqueue({"task", clsync::SyncDirection::Upload, argv[5], argv[6]}, error);
    } else if (action == "batch-download" && argc >= 7 && (argc - 5) % 2 == 0) {
        accepted = true;
        for (int index = 5; index < argc; index += 2) {
            if (!manager.enqueue({"task" + std::to_string((index - 5) / 2), clsync::SyncDirection::Download,
                                  argv[index], argv[index + 1]}, error)) {
                accepted = false;
                break;
            }
        }
    } else if (action == "ipc" && argc == 6) {
        accepted = manager.enqueue_ipc_command(argv[5], error);
    } else if (action == "reject-path" && argc == 6) {
        const bool rejected = !manager.enqueue({"task", clsync::SyncDirection::Download, argv[5], "../escape.bin"}, error);
        manager.stop();
        return report(rejected, error);
    } else {
        manager.stop();
        return 2;
    }

    if (!accepted) {
        manager.stop();
        return report(action == "reject-path", error);
    }
    const bool idle = manager.wait_for_idle(std::chrono::seconds(10));
    const auto results = manager.results();
    const bool complete = idle && !results.empty() && std::all_of(results.begin(), results.end(),
        [](const clsync::SyncResult& result) { return result.state == clsync::TaskState::Completed; });
    std::string details = "tasks=" + std::to_string(results.size());
    for (const auto& result : results)
        details += " " + result.task.id + ":attempts=" + std::to_string(result.attempts);
    manager.stop();
    return report(complete, details);
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) return 2;
    const std::string area(argv[1]);
    if (area == "network") return run_network(argc, argv);
    if (area == "file") return run_file(argc, argv);
    if (area == "ntfs") return run_ntfs(argc, argv);
    if (area == "ipc") return run_ipc(argc, argv);
    if (area == "workflow") return run_workflow(argc, argv);
    if (area == "sync") return run_sync(argc, argv);
    if (area == "log" && argc == 5) {
        output(argv[2], std::stoi(argv[3]), argv[4]);
        return 0;
    }
    return 2;
}
