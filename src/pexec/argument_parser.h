//
// Created by michalks on 12/9/17.
//

#ifndef MINER_ARGUMENT_PARSER_H
#define MINER_ARGUMENT_PARSER_H

#include <string>
#include <vector>

namespace pexec {
namespace util {

std::vector<std::string> str2arg(const std::string &str);
std::string arg2str(const std::vector<std::string>& args);

}
}


#endif //MINER_ARGUMENT_PARSER_H
