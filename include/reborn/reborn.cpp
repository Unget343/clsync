#include "reborn.h"
using namespace reborn;

/* Processing requests and storing them in a buffer of a maximum size,
 then forwarding them to other components for processing */

template<>
SocketStat Socket<Request>::enqueueRequest(const string& request)
{
    if (request.empty())
        return ERR;

    if (buffer.size() >= kMaxBufferRequests)
        return ERR;

    buffer.emplace_back(request);
    return OK;
}

/* passing a request to another component */
template<>
bool Socket<Request>::forwardTo(const Request& request)
{ return !request.payload.empty(); }

/* Processing the following requests from the buffer */
template<>
 SocketStat Socket<Request>::processNextRequest()
{
    if (buffer.empty())
        return UNDEFINED;

    Request& next = buffer.front();
    if (next.status != WAITING)
        return ERR;

    bool processed = forwardTo(next);
    next.status = processed ? OK : ERR;
    last_status = next.status;
    answer = processed ? "OK" : "ERR";

    buffer.erase(buffer.begin());
    return last_status;
}

template<>
SocketStat Socket<Request>::close() noexcept
{
#   ifdef __REB_TEST__
        if (server_fd != -1)
        {
            ::close(server_fd);
            server_fd = -1;
        }
        if (client_fd != -1)
        {
            ::close(client_fd);
            client_fd = -1;
        }
#   else
        if (server_fd != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            ::closesocket(server_fd);
#else
            ::close(server_fd);
#endif
            server_fd = INVALID_SOCKET_VALUE;
        }
        if (client_fd != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            ::closesocket(client_fd);
#else
            ::close(client_fd);
#endif
            client_fd = INVALID_SOCKET_VALUE;
        }
#   endif
    return OK;
}

template<>
SocketStat Socket<Request>::getLastStatus() const
{ return last_status; }

template<>
SocketStat Socket<Request>::setSocketPath(const char* _path)
{
    socket_path = _path;
    return OK;
}

template<>
SocketStat Socket<Request>::sendRequest(const char* request)
{
    if (!request ||
        *request == '\0') return ERR;
        
    return enqueueRequest(string(request));
}

template<>
SocketStat Socket<Request>::processRequests()
{
    SocketStat status = ERR;
    while (!buffer.empty())
        status = processNextRequest();

    return status;
}