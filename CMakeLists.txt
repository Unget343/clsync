cmake_minimum_required(VERSION 3.16)

project(clsync LANGUAGES CXX)

include(CTest)
include(CPM.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL QUIET)
if(NOT TARGET CURL::libcurl)
    CPMAddPackage(
        NAME curl
        GITHUB_REPOSITORY curl/curl
        GIT_TAG curl-8_21_0
        OPTIONS
            "BUILD_CURL_EXE OFF"
            "BUILD_SHARED_LIBS OFF"
            "BUILD_LIBCURL_DOCS OFF"
            "BUILD_MISC_DOCS OFF"
            "ENABLE_CURL_MANUAL OFF"
            "CURL_USE_LIBPSL OFF"
            "USE_LIBIDN2 OFF"
            "USE_NGHTTP2 OFF"
            "CURL_ZLIB OFF"
            "CURL_BROTLI OFF"
            "CURL_ZSTD OFF"
            "CURL_USE_SCHANNEL ON"
    )
    if(TARGET libcurl AND NOT TARGET CURL::libcurl)
        add_library(CURL::libcurl ALIAS libcurl)
    endif()
endif()

if(NOT TARGET CURL::libcurl)
    message(FATAL_ERROR "libcurl is required to build clsync")
endif()

add_library(clsync_core
    include/reborn/reborn.cpp
    include/rfs/fs_file.cpp
    include/rfs/ntfs/ntfs.cpp
    net/network.cpp
    net/sha256.cpp
    net/sync_manager.cpp
)
target_include_directories(clsync_core PUBLIC include net)
target_link_libraries(clsync_core PUBLIC CURL::libcurl)

add_executable(clsync net/sync.cpp)
target_link_libraries(clsync PRIVATE clsync_core)

if(BUILD_TESTING)
    add_executable(clsync_test_driver tests/helpers/clsync_test_driver.cpp)
    target_link_libraries(clsync_test_driver PRIVATE clsync_core)
endif()
