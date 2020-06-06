# pexec 
* pexec is process spawning library, replacement for `::system` or `::popen` functions that providess process stdout/err outputs and various state callbacks.

## Limitations
* library internally registers signal handler for SIGCHLD and then waits in ::select loop for process to end.
* only one pexec call can be made since multiple calls would override signal handler and break all other calls
* do not use this library if you need to spawn and handle multiple processes at the same time.
* if you require multi-process handling, contact me

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
#include <pexec/exec.h>

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