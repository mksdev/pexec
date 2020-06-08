# pexec 
* pexec is process spawning library, replacement for `::system` or `::popen` functions that provides process stdout/err outputs and various state callbacks.

## Limitations
* library internally registers signal handler for SIGCHLD and then waits in ::select loop for process to end.
* only one pexec/pexec_multi::run() call should be active at given time, otherwise signal handlers will be overridden

## Supported platforms
* macOS
* Linux

## CMake
* add repository as submodule to your project
* add project in cmake
```
add_subdirectory(<path to repo>/pexec)
```
* link pexec to your cmake target
```
target_link_library(<target> pexec)
```

## Examples

#### Blocking example with processing after the process ends
```
#include <pexec/pexec.h>

auto ret = pexec::exec("lss -la");
if(!ret) {
    // print all errors
    std::cout << "error: ";
    for(auto&& e: ret.err) {
        std::cout << pexec::error2string(e) << " ";
    }
    std::cout << "\n";
    return 1;
}

// print stdout/stderr of the process
std::cout << "stdout(size=" << ret.proc_out.size() << "): \n" << ret.proc_out << std::endl;
std::cout << "stderr(size=" << ret.proc_err.size() << "): \n" << ret.proc_err << std::endl;
return 0;
```
* library provides array of errors for handling purposes, some errors cannot lead to direct cancellation of the process and must be saved in bulk.
#### Blocking example with direct callbacks when process is running
```
#include <pexec/pexec.h>

pexec<> proc;
proc.set_error_cb([&](error error){
    // pexec internal error handling
});
proc.set_stdout_cb([&](const char* data, std::size_t len){
    // direct stdout pipe data
});
proc.set_stderr_cb([&](const char* data, std::size_t len){
    // direct stderr pipe data
});
proc.set_state_cb([&](pexec::proc_status::state state, proc_status& proc) {
    // state::STARTED, SIGNALED, STOPPED, USER_STOPPED
});
proc.exec("ls -la");
```

* when `::fork` is called, `::select` loop handles all stdout/err file descriptors
* library provides a way to stop the loop using additional pipe that can be used break the loop
* stop method `proc_status::user_stop()` can be called on `proc` object returned from state callback
* data processing or stopping the loop can be done in different threads

#### Multi process example with processing after the process ends
```
pexec::pexec_multi procs;
// calling .exec() or .stop() is thread safe,
// library internally uses thread safe buffer that is used to sequentially process all jobs
procs.exec("w", [](const pexec::pexec_status& status){
    std::cout << status.proc_out << std::endl;
});
procs.exec("whoami", [](const pexec::pexec_status& status){
    std::cout << status.proc_out << std::endl;
});
procs.exec("uname", [](const pexec::pexec_status& status){
    std::cout << status.proc_out << std::endl;
});
// when executing on the same thread we must specify stopping criterion
// otherwise .run() will wait indefinitely
procs.stop(pexec::stop_flag::STOP_WAIT);

// until now .exec() and .stop() are queue in the internal buffer and will be processed in the same order as called
// run will block until all processes has stopped based on .stop() call
procs.run();
```

#### Multi process example with direct callbacks when process is running
```
pexec::pexec_multi procs;
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
```

#### Multi process example with different thread
```
pexec::pexec_multi procs;
std::thread th([&](){
    // run event-loop
    procs.run();
});

... on different thread ...
procs.exec("w", [](const pexec::pexec_status& status){
    std::cout << status.proc_out << std::endl;
});
procs.exec("whoami", [](const pexec::pexec_status& status){
    std::cout << status.proc_out << std::endl;
});
procs.exec("uname", [](const pexec::pexec_status& status){
    std::cout << status.proc_out << std::endl;
});
// can be called later or never based on requiements

... later in code ...
procs.stop(pexec::stop_flag::STOP_WAIT);
th.join();
```

##### handling `stdin`:
* `proc` object returned from state callback contains duplicated `stdin` file descriptor on which `::write` syscall can be called
```
proc.set_state_cb([&](proc_status::state state, proc_status& proc) {
    // signal different thread that process is active or handle locally
    if(state == pexec::proc_status::state::STARTED) {
        const char* msg = "Hello world\n";
        assert(proc.stdin_fd != -1);
        ::write(proc.stdin_fd, msg, strlen(msg));
    }
});
```