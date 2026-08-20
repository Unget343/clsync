// This library is part of the Kuse project (a component of VoidR)
//  and is designed for trissering and interacting with system capabilities

#pragma once
 
#include <vector>
#include <string>
#include "../reborn/reborn.h"
#include "../log/log.h"
using namespace std;

class PKernel
{
    // pointer to the Reborn socket for IPC communication
    reborn::Socket<reborn::Request>* socket = nullptr;
    socket = new reborn::Socket<reborn::Request>("/tmp/kuse-block.sock");
    protected:
        // space for kernel modules
        vector<void*> kmodules;
        
        // kernel arguments 
        vector<vector<string>> k_argc;
        // stored argc count
        char argc = 0;

        string stop_reason;
    private:
        // pointer to the execution process
        vector<void*> m_processes_userspace; 
        vector<void*> m_processes_kernelspace; 
        
        inline static bool is_init = false;

    public:
        // constructor without startup arguments; arguments can be added later via init
        PKernel(string __reason)
        : stop_reason(__reason)
        {
            if (!socket) 
                socket = new reborn::Socket<reborn::Request>("/tmp/voidr.sock");

            socket->sendRequest("INIT");

            if (socket->getAnswer() == string("ERR"))
                socket->sendRequest("END");
        }   

        // static method to free the kernel process and clear the process lists
        [[noreturn]] virtual int kernel_critical_free() __attribute__((section(".kernel"))) = 0;

        // static method to free the kernel process and clear the process lists safely
        [[noreturn]] virtual int kernel_safe_free(void* __arg) __attribute__((section(".kernel"))) = 0;

        ~PKernel() 
        { socket->sendRequest("SIGEND"); }

        // initialization function for the kernel, called with command line arguments
        int initk(int argc, char *argv[])
        {
            // Store the command line arguments in a vector of strings
            (void)argc;

            vector<string> args;
            if (argv != nullptr)
            {
                for (size_t i = 0; argv[i] != nullptr; ++i)
                    args.emplace_back(argv[i]);
            }

            // Store the arguments in the kernel's argument list and set the argc count
            k_argc.push_back(args);
            this->argc = static_cast<char>(args.size());

            if (!args.empty())
                PKernel_Init(args[0].c_str());
            
            // Enter a loop to process requests from the Reborn socket until the kernel is no longer initialized
            while (this->is_init)
            {
                socket->processRequests();
                if (socket->getLastStatus() == SocketStat::ERR)
                {
                    output("Kuse", -16500, "Error in IPC communication with Reborn socket.");
                    break;
                }
            }

            return 0;
        }

        bool _isExec(PKernel *kern) noexcept 
        { return kern == NULL ? this->is_init : kern->is_init; }

        char _getArgc() const noexcept 
        { return this->argc; }

    protected:
       // initialization function for the kernel, called with command line arguments
        int PKernel_Init(const char* __arg) __attribute__((section(".kernel")))
        {
            if (!is_init)
            {            
               // set argc based on whether an argument string was provided
                argc = __arg ? 1 : 0;
               // register the pointer to the user space process in the userspace table
                m_processes_userspace.push_back((void*)__arg);
               // register the pointer to the kernel space process in the kernelspace table
                m_processes_kernelspace.push_back(this);

                is_init = true;
                socket->sendRequest("SIGINIT");
            }
            return 0;
        }
}; // Kernel process execution