
#include <pexec/pexec.h>

int main() {
    pexec::pexec_multi procs;
    procs.exec("w", [](const pexec::pexec_status& status){
        std::cout << status.proc_out << std::endl;
    });
    procs.exec("whoami", [](const pexec::pexec_status& status){
        std::cout << status.proc_out << std::endl;
    });
    procs.exec("uname", [](const pexec::pexec_status& status){
        std::cout << status.proc_out << std::endl;
    });
    procs.stop(pexec::stop_flag::STOP_WAIT);
    procs.run();
    return 0;
}
