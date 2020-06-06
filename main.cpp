
#include <pexec/exec.h>

int main() {

    auto ret = pexec::exec("ls -la");

    std::cout << "process \n" << ret.proc;
    if(!ret.err.empty()) {
        std::cout << "error: ";
        for(auto&& e: ret.err) {
            std::cout << pexec::status2string(e) << " ";
        }
        std::cout << "\n";
    }
    std::cout << "output(size=" << ret.proc_out.size() << "): \n" << ret.proc_out << std::endl;
    std::cout << "error(size=" << ret.proc_err.size() << "): \n" << ret.proc_err << std::endl;
    return 0;
}
