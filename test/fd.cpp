
#include <pexec/exec.h>
#include <chrono>

/*
 * Test that pexec does not leave any opened filedescriptors
 * after pexec call there should be only 0, 1, 2 file descriptors opened
 * that should result in pipe() returning successive filedescriptors from 3+
 */
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
        assert(p[0] == prev_fd+1);
        assert(p[1] == p[0]+1);
        prev_fd = p[1];
    }
    for(auto&& fd : fds) {
        close(fd);
    }
}

int main() {
    test_fd();
    for(int i = 0; i != 10; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto ret = pexec::exec("ls -la");
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << ret.proc_out;
        std::cout << "pexec took: " << dur << " ms\n";
        test_fd();
    }
    test_fd();
    return 0;
}
