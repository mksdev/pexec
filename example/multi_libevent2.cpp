
#include <pexec/pexec.h>
#include <event2/event.h>
#include <event2/event_struct.h>

int main() {
    std::unordered_map<int, struct event> events;
    event_base* evbase = event_base_new();

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

    procs.set_type(pexec::loop_type::EXTERNAL);
    procs.register_event([&](int fd, pexec::fd_action act, pexec::fd_what what) {
        auto& ev = events[fd];
        switch (act) {
            case pexec::fd_action::ADD_EVENT: {
                event_assign( &ev, evbase, fd, EV_READ | EV_PERSIST,
                        [](int fd, short event, void *arg) {
                            ((pexec::pexec_multi*)arg)->do_action(fd);
                }, &procs);
                event_add(&ev, nullptr);
                break;
            }
            case pexec::fd_action::REMOVE_EVENT: {
                event_del(&ev);
                events.erase(events.find(fd));
                break;
            }
        }
    });

    procs.run();

    event_base_dispatch(evbase);
    event_base_free(evbase);

    return 0;
}
