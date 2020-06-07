
#include <pexec/exec.h>
#include <pexec/pexec_multi.h>
#include <chrono>
#include <thread>

int main() {
    {
        auto start = std::chrono::high_resolution_clock::now();
        pexec::pexec_multi pmulti;
        pmulti.exec("ls -la", [](const pexec::pexec_status& stat){
            std::cout << stat.proc_out;
        });
        pmulti.exec("ls -la /", [](const pexec::pexec_status& stat){
            std::cout << stat.proc_out;
        });
        pmulti.stop(pexec::stop_flag::STOP_WAIT);
        pmulti.run();
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "pexec-multi took: " << dur << " ms\n";
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        pexec::pexec_multi pmulti;
        std::thread th([&](){
            pmulti.run();
        });
        pmulti.exec("ls -la", [](const pexec::pexec_status& stat){
            std::cout << stat.proc_out;
        });
        pmulti.exec("ls -la /", [](const pexec::pexec_status& stat){
            std::cout << stat.proc_out;
        });
        pmulti.stop(pexec::stop_flag::STOP_WAIT);
        th.join();
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "pexec-multi thread took: " << dur << " ms\n";
    }

    {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << pexec::exec("ls -la").proc_out;
        std::cout << pexec::exec("ls -la /").proc_out;
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "pexec took: " << dur << " ms\n";
    }
    {
        auto start = std::chrono::high_resolution_clock::now();
        ::system("ls -la");
        ::system("ls -la /");
        auto end = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "system took: " << dur << " ms\n";
    }

    return 0;
}
