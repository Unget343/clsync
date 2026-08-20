#pragma once

#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <afunix.h>
#   include <windows.h>
#   pragma comment(lib, "ws2_32.lib")
    typedef SOCKET SOCKET_FD;
#   define INVALID_SOCKET_VALUE INVALID_SOCKET
#else 
#   include <unistd.h>
#   include <sys/socket.h>
#   include <sys/stat.h>
#   include <sys/un.h>
    typedef int SOCKET_FD;
#   define INVALID_SOCKET_VALUE (-1)
#endif

using namespace std;

enum SocketStat
{
    OK = 0,
    ERR = -1,
    WAITING = 1,
    UNDEFINED = -2
};

namespace reborn
{

    struct Request
    {
        string payload;
        SocketStat status;

        Request(const string& data)
            : payload(data)
            , status(WAITING)
        {
        }
    };

    template<typename _SOCKET>
    class Socket
    {
        private:
            const char* socket_path;
            _SOCKET _request;

            vector<Request> buffer;
            static constexpr size_t kMaxBufferRequests = 10;

        protected:
            SOCKET_FD server_fd = INVALID_SOCKET_VALUE;
            SOCKET_FD client_fd = INVALID_SOCKET_VALUE;
            ssize_t bytes = 0;
            SocketStat last_status = UNDEFINED;
            const char* answer = nullptr;

            SocketStat enqueueRequest(const string& request);

            bool forwardTo(const Request& request);

            SocketStat processNextRequest();
            
            Socket()
            {
                buffer.reserve(kMaxBufferRequests);
#ifdef _WIN32
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                    perror("[-500]: Reborn: WSAStartup failed");
                }
#endif
                server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (server_fd == INVALID_SOCKET_VALUE)
                    perror("[-500]: Reborn: cannot starting IPC Socket");
            }

        public:
            Socket(const char* _path) : socket_path(_path)
            {
                buffer.reserve(kMaxBufferRequests);
#ifdef _WIN32
                WSADATA wsaData;
                if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                    perror("[-500] Reborn: WSAStartup failed");
                }
#endif
                server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (server_fd == INVALID_SOCKET_VALUE)
                    perror("[-500] Reborn: cannot starting IPC Socket");
            }

            SocketStat setSocketPath(const char* _path);

            SocketStat sendRequest(const char* request);

            SocketStat processRequests();

            SocketStat getLastStatus() const
            { return last_status; }

            const char* getAnswer() const
            { return answer; }

            SocketStat close() noexcept;

            explicit operator bool() const 
            { return server_fd != INVALID_SOCKET_VALUE; }

            bool operator!() const 
            { return server_fd == INVALID_SOCKET_VALUE; }

            ~Socket()
            {
                if (server_fd != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
                    ::closesocket(server_fd);
#else
                    ::close(server_fd);
#endif
                }
                if (client_fd != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
                    ::closesocket(client_fd);
#else
                    ::close(client_fd);
#endif
                }

                if (socket_path) {
#ifdef _WIN32
                    ::_unlink(socket_path);
#else
                    ::unlink(socket_path);
#endif
                }

#ifdef _WIN32
                WSACleanup();
#endif
            }
        };
} // reborn