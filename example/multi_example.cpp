
#include <pexec/pexec.h>

int main() {
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
    procs.run();

    // we can re-run new set of processes after first batch has been executed
    procs.exec("ls -la", [](const pexec::pexec_status& status){
        std::cout << status.proc << std::endl;
    });
    procs.stop(pexec::stop_flag::STOP_KILL, SIGKILL);
    procs.run();

    return 0;
}
