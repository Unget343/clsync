// This library is part of the Kuse project (a component of VoidR)
//  and is designed for trissering and interacting with system capabilities.
#pragma once

#include <cstddef>
#include <string>
#include <vector>
#include <cstring>
#include <thread>
#include "pkernel.h"
#include "../driver/reborn/reborn.h"
#include "../module/log.h"
#include <chrono>
using namespace std;

class ProcExec : private PKernel
{
    reborn::Socket<reborn::Request>* _socket = nullptr;
    protected:
        // A simple monitoring function that allows you to track partition mounts in RFS 
        void* __stmount(char* _path, char _argc) __attribute__((section(".kernel")));
        vector<vector<string>> proc_buffer; // buffer for userspace 

        vector<vector<string>> kernel_buffer;

        bool waitForLPE()
        {
            while (strcmp(_socket->getAnswer(), "ACCEPT_OK") != 0)
            {
                this_thread::sleep_for(chrono::seconds(10));

                output("ProcExec", -8530, "LPE waiting timeout");
                break;
            }

            return strcmp(_socket->getAnswer(), "ACCEPT_OK") == 0;
        }

        bool provide(ProcExec* _proc, bool kernelspace, string param)
        {
            if (!_proc) return false;

            if (kernelspace == false)
            {
                // 12345;0;root
                string request = to_string(_proc->_pid) + ";"
                + to_string(_proc->_uid) + ";"
                + param + ";";

                _socket->sendRequest((char*)request.c_str());
                waitForLPE();
            }


            string request = to_string(_proc->_pid) + to_string(_proc->_uid) + param;
            if (request.size() == CHAR_MAX) return false;

            _socket->sendRequest((char*)request.c_str());

            waitForLPE();
            return true;
        }
    
    private:
        int _uid;
        int _pid;
        char _stat;
        
    public:
        ProcExec(int uid, int pid, char stat, string name) 
        : _uid(uid), _pid(pid), _stat(stat),
          _socket(new reborn::Socket<reborn::Request>("/tmp/proc-block.sock"))
        {
            switch(pid)
            {
                case 10000:
                case 1000:
                case 100:
                    provide(this, false, "user_srv");

                case 10:
                    if(provide(this, true, "srv"))
                    {
                        kernel_buffer.push_back({to_string(uid),
                                    to_string(pid),
                                    string(1, stat),
                                    name});
                    }
                    
                case 0:
                    if (provide(this, true, "root") == true)
                    {
                        kernel_buffer.push_back({to_string(uid),
                                    to_string(pid),
                                    string(1, stat),
                                    name});
                    }
                
                default:
                    output("ProcExec", 8230, "Unknown range of PID values");
            }

            proc_buffer.push_back({to_string(uid),
                                    to_string(pid),
                                    string(1, stat),
                                    name});
        }

        vector<vector<string>> getProcBuffer() const noexcept
        { return proc_buffer; }

        vector<vector<string>> getKernelBuffer() const noexcept __attribute__((section(".kernel")))
        {
            _socket->sendRequest("ROOTBUF");

            return _socket->getLastStatus() == SocketStat::OK ? kernel_buffer 
                                                              : vector<vector<string>>{};
        }

        virtual int requestPrivilege() = 0;
        virtual ~ProcExec() = default;
};