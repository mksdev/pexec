//
// Created by Michal Němec on 05/06/2020.
//

#include <cassert>
#include <memory.h>
#include "proc_status.h"

using namespace pexec;

std::string
proc_status::state2str(state state) noexcept
{
    switch(state) {
        case state::STARTED: return "STARTED";
        case state::SIGNALED: return "SIGNALED";
        case state::STOPPED: return "STOPPED";
        case state::USER_STOPPED: return "USER_STOPPED";
        case state::FAIL_STOPPED: return "FAIL_STOPPED";
    }
}


void
proc_status::user_stop() const noexcept
{
    const char c = '\0';
    assert(user_stop_fd != -1);
    write(user_stop_fd, &c, sizeof(c));
}

void
proc_status::update_status(int status) noexcept
{
    wstatus = status;
    /**
    WIFEXITED(status)
        returns true if the child terminated normally, that is, by calling exit(3) or _exit(2), or by returning from main().
    WEXITSTATUS(status)
        returns the exit status of the child. This consists of the least significant 8 bits of the status argument that the child specified in a call to exit(3) or _exit(2) or as the argument for a return statement in main(). This macro should only be employed if WIFEXITED returned true.
    WIFSIGNALED(status)
        returns true if the child process was terminated by a signal.
    WTERMSIG(status)
        returns the number of the signal that caused the child process to terminate. This macro should only be employed if WIFSIGNALED returned true.
    WCOREDUMP(status)
        returns true if the child produced a core dump. This macro should only be employed if WIFSIGNALED returned true. This macro is not specified in POSIX.1-2001 and is not available on some UNIX implementations (e.g., AIX, SunOS). Only use this enclosed in #ifdef WCOREDUMP ... #endif.
    WIFSTOPPED(status)
        returns true if the child process was stopped by delivery of a signal; this is only possible if the call was done using WUNTRACED or when the child is being traced (see ptrace(2)).
    WSTOPSIG(status)
        returns the number of the signal which caused the child to stop. This macro should only be employed if WIFSTOPPED returned true.
    WIFCONTINUED(status)
        (since Linux 2.6.10) returns true if the child process was resumed by delivery of SIGCONT.
    @return
    */
    exited = WIFEXITED(status);
    if(exited) {
        return_code = WEXITSTATUS(status);
    }
    signaled = WIFSIGNALED(status);
    if(signaled) {
        signaled_signal = WTERMSIG(status);
        generated_core_dump = WCOREDUMP(status);
    }
    stopped = WIFSTOPPED(status);
    if(stopped) {
        stopped_signal = WSTOPSIG(status);
    }
    continued = WIFCONTINUED(status);

    running = !(signaled || exited);
}

namespace pexec {

std::ostream& operator<<(std::ostream& out, const proc_status& proc) {
    out << "pid: " << proc.pid << "\n";
    out << "running: " << proc.running << "\n";
    out << "status: " << proc.wstatus << "\n";
    if(proc.running) {
        out << "stdin_fd: " << proc.stdin_fd << "\n";
    }
    if(proc.exited) {
        out << "exited: " << proc.exited << "\n"
            << "return_code: " << proc.return_code << "\n";
    }
    if(proc.signaled) {
        out << "signaled: " << proc.signaled << "\n"
            << "signaled_signal: " << proc.signaled_signal << " (" << strsignal(proc.signaled_signal) << ")" << "\n"
            << "generated_core_dump: " << proc.generated_core_dump << "\n";
    }
    if(proc.stopped) {
        out << "stopped: " << proc.stopped << "\n"
            << "stopped_signal: " << proc.stopped_signal << " (" << ::strsignal(proc.stopped_signal) << ")" << "\n";
    }
    if(proc.continued) {
        out << "continued: " << proc.continued << "\n";
    }
    return out;
}

}