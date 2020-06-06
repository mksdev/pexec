//
// Created by Michal Němec on 06/06/2020.
//

#include "error.h"

namespace pexec {

std::string error2string(enum error err) {
    switch (err) {
        case error::NO_ERROR: return "NO_ERROR";
        case error::SIGACTION_SET_ERROR: return "SIGACTION_SET_ERROR";
        case error::SIGACTION_RESET_ERROR: return "SIGACTION_RESET_ERROR";
        case error::STDIN_PIPE_ERROR: return "STDIN_PIPE_ERROR";
        case error::STDERR_PIPE_ERROR: return "STDERR_PIPE_ERROR";
        case error::STDOUT_PIPE_ERROR: return "STDOUT_PIPE_ERROR";
        case error::FORK_ERROR: return "FORK_ERROR";
        case error::ARG_PARSE_ERROR: return "ARG_PARSE_ERROR";
        case error::ARG_PARSE_C_ERROR: return "ARG_PARSE_C_ERROR";
        case error::SIGNAL_PIPE_ERROR: return "SIGNAL_PIPE_ERROR";
        case error::WATCH_PIPE_ERROR: return "WATCH_PIPE_ERROR";
        case error::SIG_BLOCK_ERROR: return "SIG_BLOCK_ERROR";
        case error::SIG_UNBLOCK_ERROR: return "SIG_UNBLOCK_ERROR";
        case error::SELECT_ERROR: return "SELECT_ERROR";
        case error::STDOUT_PIPE_READ_ERROR: return "STDOUT_PIPE_READ_ERROR";
        case error::STDERR_PIPE_READ_ERROR: return "STDERR_PIPE_READ_ERROR";
        case error::SIGNAL_PIPE_READ_ERROR: return "SIGNAL_PIPE_READ_ERROR";
        case error::WATCH_PIPE_READ_ERROR: return "WATCH_PIPE_READ_ERROR";
        case error::WAITPID_ERROR: return "WAITPID_ERROR";
        case error::FORK_EXEC_ERROR: return "FORK_EXEC_ERROR";
        case error::FORK_STDIN_NONBLOCK_ERROR: return "FORK_STDIN_NONBLOCK_ERROR";
        case error::FORK_STDOUT_NONBLOCK_ERROR: return "FORK_STDOUT_NONBLOCK_ERROR";
        case error::FORK_STDERR_NONBLOCK_ERROR: return "FORK_STDERR_NONBLOCK_ERROR";
        case error::FORK_DUP2_STDIN_ERROR: return "FORK_DUP2_STDIN_ERROR";
        case error::FORK_DUP2_STDOUT_ERROR: return "FORK_DUP2_STDOUT_ERROR";
        case error::FORK_DUP2_STDERR_ERROR: return "FORK_DUP2_STDERR_ERROR";
    }
}

}