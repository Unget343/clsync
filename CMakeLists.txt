cmake_minimum_required(VERSION 3.14)

project(clsync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── CPM Package Manager ────────────────────────────────────────────
include(CPM.cmake)
option(CPM_ENABLE "Download external dependencies via CPM" ON)

if(CPM_ENABLE)
    message(STATUS "[clsync] Downloading external dependencies via CPM...")

    CPMAddPackage(
        NAME curl
        GITHUB_REPOSITORY curl/curl
        GIT_TAG curl-8_21_0
        OPTIONS
            "BUILD_CURL_EXE OFF"
            "BUILD_SHARED_LIBS OFF"
            "CURL_USE_OPENSSL OFF"
            "CURL_USE_SCHANNEL ON"
    )

    CPMAddPackage(
        NAME winfsp
        GITHUB_REPOSITORY winfsp/winfsp
        GIT_TAG v2.1
        DOWNLOAD_ONLY YES
    )

    if(winfsp_ADDED)
        set(WINFSP_INCLUDE_DIR "${winfsp_SOURCE_DIR}/inc" CACHE PATH "WinFsp include directory")
    endif()
endif()

# ── Library: reborn (IPC socket abstraction) ───────────────────────
add_library(reborn STATIC
    include/reborn/reborn.cpp
    include/reborn/reborn.h
)
target_include_directories(reborn PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if(WIN32)
    target_link_libraries(reborn PUBLIC ws2_32)
endif()

# ── Library: rfs (filesystem layer + NTFS) ─────────────────────────
add_library(rfs STATIC
    include/rfs/fs_file.cpp
    include/rfs/fs_file.h
    include/rfs/ntfs/ntfs.cpp
    include/rfs/ntfs/ntfs.h
)
target_include_directories(rfs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if(WIN32 AND DEFINED WINFSP_INCLUDE_DIR)
    target_include_directories(rfs PUBLIC ${WINFSP_INCLUDE_DIR})
endif()

# ── Library: net (HTTP networking via curl) ─────────────────────────
add_library(net STATIC
    net/network.cpp
    net/network.h
    net/sync.cpp
)
target_include_directories(net PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/net
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
if(TARGET CURL::libcurl)
    target_link_libraries(net PUBLIC CURL::libcurl)
elseif(TARGET libcurl)
    target_link_libraries(net PUBLIC libcurl)
endif()

# ── Executable: clsync ─────────────────────────────────────────────
add_executable(clsync
    src/main.cpp
)
target_include_directories(clsync PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/net
)
target_link_libraries(clsync PRIVATE
    reborn
    rfs
    net
)