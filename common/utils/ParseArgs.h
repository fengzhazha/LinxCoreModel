#ifndef PARSEARGS_H
#define PARSEARGS_H

#pragma once

#include <iomanip>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <functional>

#include "json.hpp"

namespace JCore {
using Json = nlohmann::json;
class ParseArgs {
public:
    ParseArgs() = default;
    ParseArgs(std::string name) : name_(name) {}
    void RegisterParam(const std::string &key, int &value, const std::string &description)
    {
        params_[key] = [this, &value](const std::string &str) {
            std::istringstream iss(str);
            iss >> value;
        };
        registeredArgs.push_back(key);
        descriptions_[key] = description;
    }

    void RegisterParam(const std::string &key, uint64_t &value, const std::string &description)
    {
        params_[key] = [this, &value](const std::string &str) {
            std::istringstream iss(str);
            iss >> value;
        };
        registeredArgs.push_back(key);
        descriptions_[key] = description;
    }

    void RegisterParam(const std::string &key, bool &value, const std::string &description)
    {
        params_[key] = [this, &value](const std::string &str) {
            value = (str == "true" || str == "1");
        };
        registeredArgs.push_back(key);
        descriptions_[key] = description;
    }

    void RegisterParam(const std::string &key, std::string &value, const std::string &description)
    {
        params_[key] = [this, &value](const std::string &str) {
            value = str;
        };
        registeredArgs.push_back(key);
        descriptions_[key] = description;
    }

    void RegisterParam(const std::string &key, std::vector<std::string> &values, const std::string &description)
    {
        paramArrays_[key] = &values;
        registeredArgs.push_back(key);
        descriptions_[key] = description;
    }

    void ParseSingleArgs(const std::vector<std::string> &args, size_t &currentIndex)
    {
        auto &index = args[currentIndex];
        if (currentIndex + 1 < args.size()) {
            params_[index](args[currentIndex + 1]);
            ++currentIndex;  // 跳过下一个参数
        } else {
            std::cerr << "Missing argument for " << args[currentIndex] << std::endl;
        }
    }
    
    void ParseArrays(const std::vector<std::string> &args, size_t &currentIndex)
    {
        auto arrayIt = paramArrays_.find(args[currentIndex]);
        if (arrayIt != paramArrays_.end()) {
            while (currentIndex + 1 < args.size() && args[currentIndex + 1][0] != '-') {
                (*arrayIt->second).push_back(args[currentIndex + 1]);
                ++currentIndex;  // 跳过下一个参数
            }
        } else {
            std::cerr << "Unknown parameter: " << args[currentIndex] << std::endl;
        }
    }

    // 解析参数
    void Parse(const std::vector<std::string> &args)
    {
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "-h") {
                PrintHelp();
                return;
            }
            if (args[i][0] == '-') {
                auto it = params_.find(args[i]);
                if (it != params_.end()) {
                    ParseSingleArgs(args, i);
                } else {
                    ParseArrays(args, i);
                }
            }
        }
    }

    void Parse(int argc, char** argv)
    {
        std::vector<std::string> args;
        args.reserve(argc);
        
        for (int i = 0; i < argc; ++i) {
            args.emplace_back(argv[i]);
        }
        
        Parse(args); // 调用原有的vector版本
    }

    void ParseJsonConfig(const std::string &path, std::vector<std::string> &cfg) const
    {
        std::cout << "Config Path:" << path << std::endl;
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Error: fail to open file:" << path << std::endl;
        }
        Json j;
        file >> j;
        std::cout << j.dump(1) << std::endl;
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::string c = it.key() + "=" + it.value().dump();
            cfg.emplace_back(c);
        }
        file.close();
    }

    void ParseConfig(const std::string &path, std::vector<std::string> &cfg) const
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cout << "Error: fail to open file:" << path << std::endl;
            assert(false);
        }
        std::string line;
        while (std::getline(file, line)) {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                cfg.emplace_back(line);
            } else {
                std::cout << "Parse Config File:" << line << std::endl;
            }
        }
        file.close();
    }

private:
    std::map<std::string, std::function<void(const std::string &)>> params_;
    std::map<std::string, std::vector<std::string> *> paramArrays_;
    std::map<std::string, std::string> descriptions_;
    std::vector<std::string> registeredArgs;
    std::string name_;
    int argWidth = 30;
    void PrintHelp() const
    {
        std::cout << "Help for " << name_ << ":" << std::endl;
        std::cout << "Usage: " << std::endl;
        std::cout << std::setw(argWidth) << std::left << "Argument" << ": Description" << std::endl;
        for (const auto &arg : registeredArgs) {
            std::cout << std::setw(argWidth) << std::left << arg << ": " << descriptions_.at(arg) << std::endl;
        }
    }
};
}
#endif