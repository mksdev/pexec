//
// Created by Michal Němec on 06/06/2020.
//

#include "exec.h"

namespace pexec {

bool
pexec_status::valid() const
{
    return err.empty();
}

pexec_status::operator bool() const {
    return valid();
}

pexec_status
exec(const std::string& arg, const fd_state_callback& cb)
{
    pexec_status ret{};

    ret.args = arg;

    std::ostringstream stdout_oss;
    std::ostringstream stderr_oss;

    pexec<> proc;
    proc.set_error_cb([&](error err){
        ret.err.emplace_back(err, errno);
    });
    proc.set_stdout_cb([&](const char* data, std::size_t len){
        stdout_oss << data;
    });
    proc.set_stderr_cb([&](const char* data, std::size_t len){
        stderr_oss << data;
    });
    proc.set_state_cb([&](proc_status::state state, proc_status& proc) {
        ret.proc = proc;
        ret.state = state;
        if(cb) cb(state, proc);
    });
    proc.exec(arg);

    ret.proc_out = stdout_oss.str();
    ret.proc_err = stderr_oss.str();

    return ret;
}

}