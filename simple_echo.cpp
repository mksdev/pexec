//
// Created by Michal Němec on 14/12/2019.
//

#include <istream>
#include <string>
#include <iostream>

int main() {
    std::string line;
    while(std::getline(std::cin, line)) {
        std::cout << "received line: " << line << std::endl;
        if(line == "exit") {
            std::cout << "exitting" << std::endl;
            break;
        }
    }
    return 101;
}