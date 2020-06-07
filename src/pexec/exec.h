//
// Created by Michal Němec on 06/06/2020.
//

#ifndef PEXEC_EXEC_H
#define PEXEC_EXEC_H

#include "pexec.h"

namespace pexec {

struct perror {
    error pexec_error; // pexec::error
    int error_code; //errno
    perror(error err, int code);
};

std::ostream& operator<<(std::ostream& out, perror err);

struct pexec_status {
    std::string proc_out;
    std::string proc_err;

    std::string args;
    proc_status::state state;
    proc_status proc;
    std::vector<perror> err;

    bool valid() const;
    operator bool() const;

};

pexec_status exec(const std::string& arg, const fd_state_callback& cb = {});

}

#endif //PEXEC_EXEC_H
