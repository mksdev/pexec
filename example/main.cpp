
#include <pexec/exec.h>

int main() {
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
}
