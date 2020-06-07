//
// Created by Michal Němec on 07/06/2020.
//

#ifndef PEXEC_PEXEC_STATUS_H
#define PEXEC_PEXEC_STATUS_H

#include <string>
#include <vector>

#include "proc_status.h"
#include "error.h"

namespace pexec {

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

}


#endif //PEXEC_PEXEC_STATUS_H
