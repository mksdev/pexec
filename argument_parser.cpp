//
// Created by michalks on 12/9/17.
//

#include <sstream>
#include <iostream>
#include "argument_parser.h"

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

ArgumentParser::ArgumentParser(const std::string& str)
{
    {
        std::vector<std::string> subs_n = split(str, '|');
        std::string process_args = str;
        std::string process_inputs;
        if(subs_n.size() == 2) {
            process_args = subs_n[1];
            process_inputs = subs_n[0];
        }
        str_args_ = str2arg(process_args);
        input_args_ = str2arg(process_inputs);
    }
    for(const auto& ss : str_args_) {
        prog_args_.push_back(ss.c_str());
    }
    prog_args_.push_back(nullptr);
}

const char**
ArgumentParser::args() const noexcept
{
    return (const char**)prog_args_.data();
}

const std::vector<std::string>&
ArgumentParser::args_s() const noexcept
{
    return str_args_;
}

const std::vector<std::string>&
ArgumentParser::input() const noexcept
{
    return input_args_;
}

}
