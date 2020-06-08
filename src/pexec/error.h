//
// Created by Michal Němec on 06/06/2020.
//

#ifndef PEXEC_ERROR_H
#define PEXEC_ERROR_H

#include <ostream>
#include <string>
#include <functional>

namespace pexec {

enum class error : int {
    NO_ERROR,
    SIGACTION_SET_ERROR,
    SIGACTION_RESET_ERROR,
    STDIN_PIPE_ERROR,
    STDERR_PIPE_ERROR,
    STDOUT_PIPE_ERROR,
    FORK_ERROR,
    ARG_PARSE_ERROR,
    ARG_PARSE_C_ERROR,
    SIGNAL_PIPE_ERROR,
    WATCH_PIPE_ERROR,
    SIG_BLOCK_ERROR,
    SIG_UNBLOCK_ERROR,
    SELECT_ERROR,
    STDOUT_PIPE_READ_ERROR,
    STDERR_PIPE_READ_ERROR,
    STDOUT_PIPE_READ_REST_ERROR,
    STDERR_PIPE_READ_REST_ERROR,
    SIGNAL_PIPE_READ_ERROR,
    WATCH_PIPE_READ_ERROR,
    WAITPID_ERROR,
    FORK_EXEC_ERROR, //100
    FORK_STDIN_NONBLOCK_ERROR, //101
    FORK_STDOUT_NONBLOCK_ERROR, //102
    FORK_STDERR_NONBLOCK_ERROR, //103
    FORK_DUP2_STDIN_ERROR, //104
    FORK_DUP2_STDOUT_ERROR, //105
    FORK_DUP2_STDERR_ERROR, //106
    EXEC_ALLOCATION_ERROR
};

struct perror {
    error pexec_error; // pexec::error
    int error_code; //errno
    perror(error err, int code);
};

using error_status_cb = std::function<void(error)>;
std::string error2string(enum error err);
std::ostream& operator<<(std::ostream& out, perror err);

}

#endif //PEXEC_ERROR_H
