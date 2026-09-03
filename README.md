# CLsync — Cloud Synchronization

**CLsync** is a modern C++17 utility designed for synchronizing files and file systems between local environments and cloud storage.

The project provides a robust and convenient way to keep local data synchronized with remote cloud-based storage, making it possible to work with files locally while maintaining an up-to-date copy in the cloud.

In addition to cloud synchronization capabilities, **CLsync includes its own NTFS implementation utilizing [WinFsp](https://winfsp.dev/)**, providing the project with a dedicated filesystem layer for intercepting and handling filesystem operations on Windows.

## Overview

CLsync is intended to serve as a lightweight and flexible synchronization utility for applications and environments that require reliable interaction between local file systems and cloud storage. It goes beyond simple file copying by integrating at the filesystem level to monitor and manage synchronization transparently.

### Architecture & Components

The project is modular and structured into several core libraries:
* **`rfs` (Remote File System):** A filesystem layer with built-in NTFS support, leveraging WinFsp on Windows for seamless OS integration.
* **`net`:** An HTTP networking module powered by `libcurl` to handle remote communication and cloud synchronization.
* **`reborn`:** An IPC (Inter-Process Communication) socket abstraction layer.
* **`kuse`:** A task management kernel responsible for scheduling and executing internal background tasks.

### Key Features

* **Cloud synchronization** — synchronize local files and directories with remote cloud storage via HTTP.
* **File system integration** — deep integration with the operating system using WinFsp to provide real-time filesystem tracking.
* **NTFS implementation** — includes an integrated implementation for working with the NTFS file system.
* **Local-to-cloud workflow** — allows locally stored files to be synchronized with their cloud-based counterparts transparently.
* **Modern C++ design** — built with C++17 and managed via CMake with the CPM package manager for automatic dependency resolution.

## Building the Project

CLsync uses CMake (>= 3.14) and CPM for dependency management. During the configuration step, it automatically downloads the required external dependencies (such as `curl` and `winfsp`).

```bash
mkdir build && cd build && cmake .. && cmake --build .
```

## Project Goals

The primary goal of CLsync is to provide a clear and efficient foundation for synchronizing file systems with cloud storage.

The project is focused on bridging the gap between traditional local filesystem operations and cloud-based storage, while keeping the synchronization process transparent and manageable for the end-user.

## Status

CLsync is an actively developed project. Additional functionality, improvements, and documentation may be added over time.
