cmake_minimum_required(VERSION 3.10)

project(clsync CXX)

include(CPM.cmake)
option(CPM_ENABLE on)

if (cpm)
    message("-- adding extenstions. Downloading Curl...")
    CPMGetPackage("gh:curl/curl@curl-8_21_0")
    message("-- Downloading WinFsp...")
    CPMGetPackage("gh:winfsp/winfsp@v2.1")

    target_link_libraries(clsync PRIVATE curl::curl)
    add_executable(clsync src/main.cpp)
endif()