#pragma once
#define __TEST__

#ifdef __TEST__
#   define __REB_TEST__ 1
#   include <cstring>
#   include <string>
#   include <unistd.h>
#   include <sys/socket.h>
#   include <sys/stat.h>
#   include <sys/un.h>
#   include <vector>
using namespace std;
#else 
// include windows api
#endif

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
            int server_fd = -1;
            int client_fd = -1;
            ssize_t bytes = 0;
            SocketStat last_status = UNDEFINED;
            const char* answer = nullptr;

            SocketStat enqueueRequest(const string& request);

            bool forwardTo(const Request& request);

            SocketStat processNextRequest();
            
            Socket()
            {
                buffer.reserve(kMaxBufferRequests);
                server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (server_fd == -1)
                    perror("[-500]: Reborn: cannot starting IPC Socket");
            }

        public:
            Socket(const char* _path) : socket_path(_path)
            {
                buffer.reserve(kMaxBufferRequests);
                server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (server_fd == -1)
                    perror("[-500] Reborn: cannot starting IPC Socket");
            }

            SocketStat setSocketPath(const char* _path);

            SocketStat sendRequest(const char* request);

            SocketStat processRequests();

            SocketStat getLastStatus() const
            { return last_status; }

            const char* getAnswer() const
            { return answer; }

            explicit operator bool() const 
            { return server_fd != -1; }

            bool operator!() const 
            { return server_fd == -1; }

            ~Socket()
            {
                if (server_fd != -1)
                    ::close(server_fd);
                if (client_fd != -1)
                    ::close(client_fd);

                if (socket_path)
                    unlink(socket_path);
            }
        };
} // reborn