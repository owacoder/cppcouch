#ifndef STRING_TOOLS_H
#define STRING_TOOLS_H

#include <string.h>
#include <string>
#include <vector>

namespace ascii_string_tools
{
    inline void to_lower(std::string &ascii_string)
    {
        for (size_t i = 0; i < ascii_string.size(); ++i)
            ascii_string[i] = tolower(static_cast<unsigned char>(ascii_string[i]));
    }

    inline void to_upper(std::string &ascii_string)
    {
        for (size_t i = 0; i < ascii_string.size(); ++i)
            ascii_string[i] = toupper(static_cast<unsigned char>(ascii_string[i]));
    }

    inline std::string to_lower_copy(const std::string &ascii_string)
    {
        std::string s(ascii_string);
        to_lower(s);
        return s;
    }

    inline std::string to_upper_copy(const std::string &ascii_string)
    {
        std::string s(ascii_string);
        to_upper(s);
        return s;
    }

    inline std::string to_percent_encoded_copy(const std::string &str)
    {
        const std::string hex = "0123456789ABCDEF";
        const std::string unreserved = "-._~";
        std::string result;

        for (size_t i = 0; i < str.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(str[i]);
            if (!isalnum(c) && unreserved.find(c) == std::string::npos)
            {
                result.push_back('%');
                result.push_back(hex[c >> 4]);
                result.push_back(hex[c & 0xF]);
            }
            else
                result.push_back(c);
        }

        return result;
    }

    inline std::string to_percent_decoded_copy(const std::string &str)
    {
        const std::string hex = "0123456789ABCDEF";
        std::string result;

        for (size_t i = 0; i < str.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(str[i]);
            if (c == '%' && i + 2 < str.size() &&
                isxdigit(static_cast<unsigned char>(str[i+1])) &&
                isxdigit(static_cast<unsigned char>(str[i+2])))
            {
                result.push_back((hex.find(toupper(static_cast<unsigned char>(str[i+1]))) << 4) +
                                  hex.find(toupper(static_cast<unsigned char>(str[i+2]))));
                i += 2;
            }
            else
                result.push_back(c);
        }

        return result;
    }

    inline void to_percent_encoded(std::string &str) {str = to_percent_encoded_copy(str);}
    inline void to_percent_decoded(std::string &str) {str = to_percent_decoded_copy(str);}

    struct isspace {int operator()(int val) const {return ::isspace(val);}};

    template<class Predicate = isspace>
    void trim(std::string &str, Predicate p = isspace())
    {
        size_t start = 0, end = str.size();

        for (; start < str.size() && p(static_cast<unsigned char>(str[start])); ++start);
        for (; end > 0 && p(static_cast<unsigned char>(str[end-1])); --end);

        str.erase(end);
        str.erase(0, start);
    }

    template<class Predicate = isspace>
    std::string trim_copy(const std::string &str, Predicate p = isspace())
    {
        std::string s(str);
        trim(s, p);
        return s;
    }

    inline std::vector<std::string> split(const std::string &str, char delim, bool keep_empty = false)
    {
        std::vector<std::string> result;
        size_t start = 0, pos;

        if (!keep_empty && str.empty())
            return result;

        while (true)
        {
            pos = str.find(delim, start);
            if (pos == std::string::npos)
            {
                if (start != str.size() || keep_empty)
                    result.push_back(str.substr(start));
                break;
            }
            if (start != pos || keep_empty) // Don't add unless it is not empty or we want empty entries
                result.push_back(str.substr(start, pos-start));
            start = pos+1;
        }

        return result;
    }

    template<class Container = std::vector<std::string>>
    std::string join(const Container &container, const std::string &delim, bool keep_empty = false)
    {
        std::string result;
        for (auto it = container.begin(); it != container.end(); ++it)
        {
            if (!it->empty() || keep_empty)
            {
                if (!result.empty())
                    result += delim;

                result += *it;
            }
        }
        return result;
    }
}

#endif // STRING_TOOLS_H
