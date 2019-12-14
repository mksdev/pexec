//
// Created by michalks on 12/9/17.
//

#ifndef MINER_ARGUMENT_PARSER_H
#define MINER_ARGUMENT_PARSER_H

#include <string>
#include <vector>

namespace util {

std::vector<std::string> str2arg(const std::string &str);
std::string arg2str(const std::vector<std::string>& args);

class ArgumentParser {
    std::vector<std::string> str_args_;
    std::vector<std::string> input_args_;
    std::vector<const char *> prog_args_;

public:
    explicit ArgumentParser(const std::string& str);
    const char** args() const noexcept;
    const std::vector<std::string>& args_s() const noexcept;
    const std::vector<std::string>& input() const noexcept;

};

}


#endif //MINER_ARGUMENT_PARSER_H
