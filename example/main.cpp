
#include <pexec/exec.h>

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& vec) {
    for(std::size_t i = 0; i != vec.size(); ++i) {
        out << vec[i];
        if(i+1 != vec.size()) {
            out << " ";
        }
    }
    return out;
}

int main() {
    auto ret = pexec::exec("ls -la");
    if(!ret) {
        // print all errors
        std::cout << "error: " << ret.err << "\n";
    }
    // print stdout/stderr of the process
    std::cout << "stdout(size=" << ret.proc_out.size() << "): \n" << ret.proc_out << "\n";
    std::cout << "stderr(size=" << ret.proc_err.size() << "): \n" << ret.proc_err << "\n";
    return 0;
}
