#ifndef BASE64_H
#define BASE64_H

#include <string>

namespace base64
{
    std::string encode_copy(const std::string &str, bool insert_newlines = false);
    std::string decode_copy(const std::string &str);

    void encode(std::string &str, bool insert_newlines = false);
    void decode(std::string &str);
}

#endif // BASE64_H
