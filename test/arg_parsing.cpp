
#include <pexec/pexec.h>
#include <iostream>

template<typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& vec) {
    for(std::size_t i = 0; i != vec.size(); ++i) {
        out << "[" << i << "] = ";
        out << "\"" << vec[i] << "\"";
        if(i+1 != vec.size()) {
            out << "\n";
        }
    }
    return out;
}


int main() {

    {
        const char* args = "ls -la";
        auto args_arr = pexec::util::str2arg(args);
        std::cout << args << "\n";
        std::cout << args_arr << std::endl;
        assert(args_arr.size() == 2);
        assert(args_arr[0] == "ls");
        assert(args_arr[1] == "-la");
    }

    {
        const char* args = "cp \"./file with spaces.txt\" ./output.txt";
        auto args_arr = pexec::util::str2arg(args);
        std::cout << args << "\n";
        std::cout << args_arr << std::endl;
        assert(args_arr.size() == 3);
        assert(args_arr[0] == "cp");
        assert(args_arr[1] == "./file with spaces.txt");
        assert(args_arr[2] == "./output.txt");
    }

    return 0;
}
