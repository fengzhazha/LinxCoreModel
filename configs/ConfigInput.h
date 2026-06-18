#pragma once

#ifndef CONFIG_INPUT_H
#define CONFIG_INPUT_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

namespace JCore {
struct ConfigInput {
    std::unordered_map<std::string, int> cfgMap;
    ConfigInput() = default;

    bool Init(std::vector<std::string> &cfgs)
    {
        bool repeat = false;
        for (auto &cfg : cfgs) {
            if (cfgMap.find(cfg) != cfgMap.end()) {
                repeat = true;
                std::cerr << "Repeated config Input:" << cfg << std::endl;
            }
            cfgMap[cfg] = 0;
        }
        return !repeat;
    }
    bool UnuseCheck()
    {
        bool hasUnuse = false;
        for (auto &entry : cfgMap) {
            if (entry.second == 0) {
                hasUnuse = true;
                std::cerr << "Unused config input:" << entry.first << std::endl;
            }
        }
        return hasUnuse;
    }
};
}
#endif