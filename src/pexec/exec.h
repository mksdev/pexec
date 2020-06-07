//
// Created by Michal Němec on 06/06/2020.
//

#ifndef PEXEC_EXEC_H
#define PEXEC_EXEC_H

#include "util.h"
#include "pexec_status.h"

namespace pexec {

pexec_status exec(const std::string& arg, const fd_state_callback& cb = {});

}

#endif //PEXEC_EXEC_H
