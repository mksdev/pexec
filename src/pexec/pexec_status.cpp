//
// Created by Michal Němec on 07/06/2020.
//

#include "pexec_status.h"

namespace pexec {

bool
pexec_status::valid() const
{
    return err.empty();
}

pexec_status::operator bool() const {
    return valid();
}

}