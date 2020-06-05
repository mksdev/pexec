//
// Created by michalks on 12/9/17.
//

#include <sstream>
#include <iostream>
#include "argument_parser.h"

namespace pexec {
namespace util {

template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}

std::vector<std::string> str2arg(const std::string &ss) {
    if(ss.empty()) {
        return {};
    }
    std::vector<std::string> ret;
    std::vector<std::string> subs = split(ss, '\"');
    for(int i=0, sz_i = (int)subs.size(); i<sz_i; i++) {
        if(i%2 == 0) {
            std::vector<std::string> subs_s = split(subs[i], ' ');
            for(int j=0, sz_j = (int)subs_s.size(); j<sz_j; j++) {
                auto proc = subs_s[j];
                if(proc.empty()) continue;

                ret.push_back(subs_s[j]);
            }
        }else{
            auto proc = subs[i];
            if(proc.empty()) continue;
            ret.push_back(subs[i]);
        }
    }
    return ret;
}

std::string arg2str(const std::vector<std::string>& args)
{
    std::ostringstream ss;
    for(size_t i = 0, sz = args.size(); i<sz; i++) {
        const auto& p = args[i];
        bool contains_space = false;
        for (char c : p) {
            if(c == ' ') {
                contains_space = true;
                break;
            }
        }

        if(contains_space) ss << "\"";
        ss << p;
        if(contains_space) ss << "\"";

        if(i+1 != sz) {
            ss << " ";
        }
    }
    return ss.str();
}

}
}