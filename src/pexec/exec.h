//
// Created by Michal Němec on 06/06/2020.
//

#ifndef PEXEC_EXEC_H
#define PEXEC_EXEC_H

#include "pexec.h"

namespace pexec {

struct exec_status {
    std::string proc_out;
    std::string proc_err;

    proc_status proc;
    std::vector<error> err;
};

exec_status exec(const std::string& arg, const fd_state_callback& cb = {});

}

#endif //PEXEC_EXEC_H
