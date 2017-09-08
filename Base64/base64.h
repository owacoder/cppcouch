#ifndef BASE64_H
#define BASE64_H

#include <string>
#include <stdint.h>
#include <string.h>
#include <memory.h>

namespace base64
{
    inline std::string encode_copy(const std::string &str, bool insert_newlines = false)
    {
        const char alpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        uint32_t temp;
        size_t result_size = 0;
        const size_t line_length = 76;

        for (size_t i = 0; i < str.size();)
        {
            temp = (uint32_t) (str[i++] & 0xFF) << 16;
            if (i + 2 <= str.size())
            {
                temp |= (uint32_t) (str[i++] & 0xFF) << 8;
                temp |= (uint32_t) (str[i++] & 0xFF);
                result.push_back(alpha[(temp >> 18) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back(alpha[(temp >> 12) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back(alpha[(temp >>  6) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back(alpha[ temp        & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0 && i < str.size()) result.push_back('\n');
            }
            else if (i + 1 == str.size())
            {
                temp |= (uint32_t) (str[i++] & 0xFF) << 8;
                result.push_back(alpha[(temp >> 18) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back(alpha[(temp >> 12) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back(alpha[(temp >>  6) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back('=');
            }
            else if (i == str.size())
            {
                result.push_back(alpha[(temp >> 18) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back(alpha[(temp >> 12) & 0x3F]);
                if (insert_newlines && ++result_size % line_length == 0) result.push_back('\n');
                result.push_back('=');
                result.push_back('=');
            }
        }

        return result;
    }

    inline std::string decode_copy(const std::string &str)
    {
        const std::string alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        size_t input = 0;
        uint32_t temp = 0;

        for (size_t i = 0; i < str.size(); ++i)
        {
            size_t pos = alpha.find(str[i]);
            if (pos == std::string::npos)
                continue;
            temp |= (uint32_t) pos << (18 - 6 * input++);
            if (input == 4)
            {
                result.push_back((temp >> 16) & 0xFF);
                result.push_back((temp >>  8) & 0xFF);
                result.push_back( temp        & 0xFF);
                input = 0;
                temp = 0;
            }
        }

        if (input > 1) result.push_back((temp >> 16) & 0xFF);
        if (input > 2) result.push_back((temp >>  8) & 0xFF);

        return result;
    }

    inline void encode(std::string &str, bool insert_newlines = false) {str = encode_copy(str, insert_newlines);}
    inline void decode(std::string &str) {str = decode_copy(str);}
}

#endif // BASE64_H
