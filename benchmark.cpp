
#include <pexec/exec.h>
#include <chrono>

int main() {
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto ret = pexec::exec("ls -la");
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "pexec took: " << dur << " ms\n";
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        ::system("ls -la");
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "system took: " << dur << " ms\n";
    }

    return 0;
}
