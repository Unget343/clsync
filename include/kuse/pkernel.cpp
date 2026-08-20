#include "pkernel.h"
using namespace std;

[[noreturn]] int PKernel::kernel_critical_free()
{
    if (this->_isExec(this) || this->is_init)
    {
        // We iterate only through the rows of the matrix
        for (size_t i = 0; i < k_argc.size(); ++i)
        {
            // Validation: We check that the string actually contains at least 2 elements
            if (k_argc[i].size() >= 2)
            {
                // k_argc[i][0] is always the argument, and k_argc[i][1] is its value
                if (k_argc[i][0] == "__no_send_request" &&
                    k_argc[i][1] == "true")
                {
                    output("Kuse", -153, "Critical task completion... Unknown cause");
                }
                else if (k_argc[i][0] == "__no_send_request" &&
                        k_argc[i][1] == "false")
                {
                    socket->sendRequest("SIGCRIT");
                    this->is_init = false;
                    output("Kuse", -154, "Critical completion");
                }
            }
        }
    }
}

[[noreturn]] int PKernel::kernel_safe_free(void* __arg)
    if (this->_isExec(this) || this->is_init)
    {
        // We iterate only through the rows of the matrix
        for (size_t i = 0; i < k_argc.size(); ++i)
        {
            // Validation: We check that the string actually contains at least 2 elements
            if (k_argc[i].size() >= 2)
            {
                if (k_argc[i][0] == "__no_send_request" &&
                    k_argc[i][1] == "true")
                    {
                        socket->sendRequest("SIGNULL");
                        output("Kuse", 0, "process ending...");
                    } 
            }
        }
    }
    this->is_init = false;
}