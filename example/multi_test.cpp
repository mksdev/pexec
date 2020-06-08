
#include <pexec/pexec.h>
#include <thread>

void test_fd() {
    std::vector<int> fds;
    int prev_fd = 2;
    for(int i = 0; i<sysconf(_SC_OPEN_MAX); i++) {
        int p[2] = {-1, -1};
        if(pipe(p) < 0) {
            break;
        }

        fds.emplace_back(p[0]);
        fds.emplace_back(p[1]);
        auto diff = p[0] - prev_fd;
        //std::cout << "iteration: " << i << ", pipe: {" << p[0] << ", " << p[1] << "} prev-diff: " << diff << std::endl;
        assert(p[1] - p[0] == 1);
        assert(p[0] == prev_fd+1);
        assert(p[1] == p[0]+1);
        prev_fd = p[1];
    }
    for(auto&& fd : fds) {
        close(fd);
    }
}

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

    using exec_handle = std::function<void(pexec::pexec_multi&, const std::string&)>;
    exec_handle exec = [](pexec::pexec_multi& procs, const std::string& args){
        procs.exec(args, [&](const pexec::pexec_status& ret){
            std::cout << ret.proc;
            std::cout << "args: " << ret.args << "\n";
            std::cout << "state: " << pexec::proc_status::state2str(ret.state) << "\n";
            if(!ret) {
                // print all errors
                std::cout << "error: " << ret.err << "\n";
            }
            // print stdout/stderr of the process
            if(!ret.proc_out.empty()) {
                std::cout << "stdout(size=" << ret.proc_out.size() << "): \n" << ret.proc_out;
                std::cout << "\n";
            }
            if(!ret.proc_err.empty()) {
                std::cout << "stderr(size=" << ret.proc_err.size() << "): \n" << ret.proc_err;
                std::cout << "\n";
            }
            if(ret.proc_err.empty() || ret.proc_err.empty()) {
                std::cout << "\n";
            }
        });
    };

    {
        pexec::pexec_multi procs;
        procs.on_error([](pexec::error err){
            std::cout << "pexec_multi error: " << pexec::error2string(err) << ", (" << errno << ") " << strerror(errno) << "\n";
        });
        exec(procs, "ls -la");
        exec(procs, "ls -la");
        exec(procs, "ls -la");
        procs.stop(pexec::stop_flag::STOP_WAIT);
        procs.run();
        std::cout << "done" << "\n";
    }
    test_fd();
    return 0;
}
