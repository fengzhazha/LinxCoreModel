#pragma once

#include <string>
#include <unordered_map>

template <typename T>
class BiMap {
public:
    BiMap(const std::initializer_list<std::pair<T, std::string>> &init)
    {
        // for (const auto &[i, s] : init) {
        for (const auto &item : init) {
            type2strDict[item.first] = item.second;
            str2typeDict[item.second] = item.first;
        }
    }

    bool Count(T key) const
    { 
        return type2strDict.count(key);
    }

    bool Count(const std::string &key) const {
        return str2typeDict.count(key);
    }

    const std::string &Find(T key, const std::string &defaultValue = "") const
    {
        if (type2strDict.count(key)) {
            return type2strDict.find(key)->second;
        } else {
            return defaultValue;
        }
    }

    T Find(const std::string &key, T defaultValue) const
    {
        if (str2typeDict.count(key)) {
            return str2typeDict.find(key)->second;
        } else {
            return defaultValue;
        }
    }

private:
    std::unordered_map<T, std::string> type2strDict;
    std::unordered_map<std::string, T> str2typeDict;
};