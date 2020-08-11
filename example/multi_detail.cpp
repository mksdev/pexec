
#include <pexec/pexec.h>

int main() {
    pexec::pexec_multi procs;
    // we can re-run new set of processes after first batch has been executed
    // handle detailed state of the process

    procs.exec("ls -la", [](pexec::pexec_multi_handle& handle){
        handle.set_error_cb([&](pexec::error error){
            // pexec internal error handling
            std::cout << "process error: " << pexec::error2string(error) << std::endl;
        });
        handle.set_stdout_cb([&](const char* data, std::size_t len){
            // direct stdout pipe data
            std::cout << "process stdout(" << len << ")" << std::string(data, len) << std::endl;
        });
        handle.set_stderr_cb([&](const char* data, std::size_t len){
            // direct stderr pipe data
            std::cout << "process stderr(" << len << ")" << std::string(data, len) << std::endl;
        });
        handle.set_state_cb([&](pexec::proc_status::state state, pexec::proc_status& proc) {
            // state::STARTED, SIGNALED, STOPPED, USER_STOPPED, FAIL_STOPPED
            std::cout << "process " << pexec::proc_status::state2str(state) << std::endl;

            // you can save stdin file descriptor and use it to pass additional data with ::write
            std::cout << "stdin_fd: " << proc.stdin_fd << "\n";
        });
    });

    procs.stop(pexec::stop_flag::STOP_WAIT);
    procs.run();

    return 0;
}
