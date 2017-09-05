#ifndef STRING_TOOLS_H
#define STRING_TOOLS_H

#include <string.h>
#include <string>
#include <vector>

namespace ascii_string_tools
{
    std::string to_lower_copy(const std::string &ascii_string);
    std::string to_upper_copy(const std::string &ascii_string);

    void to_lower(std::string &ascii_string);
    void to_upper(std::string &ascii_string);

    std::string to_percent_encoded_copy(const std::string &str);
    std::string to_percent_decoded_copy(const std::string &str);

    void to_percent_encoded(std::string &str);
    void to_percent_decoded(std::string &str);

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

    std::vector<std::string> split(const std::string &str, char delim, bool keep_empty = false);

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
