#include "../include/reborn/reborn.h"
#include "network.h"
#include "../include/rfs/ntfs/ntfs.h"
#include "../include/log/log.h"
using namespace reborn;
using namespace rfs;

int main(int argc, char* argv[])
{
    Socket<Request> sock("/tmp/clsync.sock");
    if (!sock) 
    {
        output("sync", -4, "Failed to bind socket");
        return -4;
    }
    sock.sendRequest("INITED");
    output("sync", 0, "Send init signal");

    ntfs::NtfsFile volume;
    output("sync", 0, "create volume...");
    if (!volume.is_open()) 
    {
        output("sync", -5, "Failed to create volume");
        return -5;
    }
    sock.sendRequest("VOLUME:OK");
    output("sync", 0, "NTFS volume has created");

    Network netsock;
    output("sync", 0, "create net socket...");
    string response;
    if (netsock.http_get("http://[IP_ADDRESS]", response) == true) 
        sock.sendRequest("HTTP:OK");
    else
    {
        sock.sendRequest("HTTP:FAIL");
        output("sync", -6, "HTTP timeout");
        return -6;
    }
    
    return 0;
}