//
// Created by Michal Němec on 05/06/2020.
//

#ifndef PEXEC_PROC_STATUS_H
#define PEXEC_PROC_STATUS_H

#include <iostream>
#include <array>
#include <cstdio>
#include <unistd.h>

#include <zconf.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <csignal>

namespace pexec {

struct proc_status {

    enum class state {
        STARTED, SIGNALED, STOPPED, USER_STOPPED, FAIL_STOPPED
    };

    static std::string state2str(state state) noexcept;

    int user_stop_fd = -1;

    pid_t pid = 0;
    int stdin_fd = -1;
    bool running = false;

    int wstatus = 0;

    bool exited = false;
    int return_code = 0;
    bool signaled = false;
    int signaled_signal = 0;
    bool generated_core_dump = false;
    bool stopped = false;
    bool stopped_signal = false;
    bool continued = false;

    void user_stop() const noexcept;
    void update_status(int status) noexcept;

};

std::ostream& operator<<(std::ostream& out, const proc_status& proc);

}



#endif //PEXEC_PROC_STATUS_H
