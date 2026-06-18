#pragma once
#ifndef __CONF_PARSER_H__
#define __CONF_PARSER_H__

#include <cstdio>
#include <string>
#include <vector>

namespace JCore {

class ConfParser
{
    static constexpr size_t BUFFER_SIZE1 = 0x1000;

    FILE *fptr{nullptr};
    char line[BUFFER_SIZE1]{0};
    char buf[BUFFER_SIZE1]{0};
    std::string header;

public:
    ConfParser(ConfParser const &) = delete;
    ConfParser(ConfParser &&) = delete;

    ConfParser(std::string const &path, std::vector<std::string> &cfgv)
    {
        if ((fptr = fopen(path.c_str(), "r")) == nullptr)
            return;

        char buf[BUFFER_SIZE1];
        while (fgets(line, BUFFER_SIZE1, fptr) != nullptr)
        {
            size_t pos = 0;
            if (line[0] == '[')
            {
                size_t i = 1;
                for (; line[i] && line[i] != ']'; ++i)
                    buf[pos++] = line[i];
                if (line[i] != ']')
                    return;
                buf[pos++] = '.';
                buf[pos] = 0;
                header = buf;
                continue;
            }

            size_t eqp = 0;
            for (size_t i = 0; line[i]; ++i)
            {
                if (line[i] == '=')
                {
                    eqp = i;
                    break;
                }
            }

            if (eqp == 0)
                continue;

            for (size_t i = 0; i < eqp; ++i)
            {
                if (line[i] == '#')
                    return;
                if (!isspace(line[i]))
                    buf[pos++] = line[i];
            }
            buf[pos++] = '=';
            for (size_t i = eqp + 1; line[i]; ++i)
            {
                if (line[i] == '#')
                    break;
                if (!isspace(line[i]))
                    buf[pos++] = line[i];
            }
            buf[pos] = 0;

            auto s = header + buf;
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (isupper(s[i]))
                    s[i] ^= 32;
            }
            cfgv.emplace_back(std::move(s));
        }
    }

    ~ConfParser()
    {
        fclose(fptr);
    }
};

} // namespace JCore

#endif // __CONF_PARSER_H__
