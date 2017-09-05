#ifndef CPPCOUCH_USER_H
#define CPPCOUCH_USER_H

#include "../String/string_tools.h"
#include "../Base64/base64.h"

namespace couchdb
{
    class user
    {
    public:
        static user from_basic_auth(std::string auth)
        {
            size_t idx;
            if (ascii_string_tools::to_lower_copy(auth).find("basic ") == 0)
                auth.erase(0, 6);
            base64::decode(auth);
            idx = auth.find(':');
            if (idx == std::string::npos)
                return user(auth);
            return user(auth.substr(0, idx), auth.substr(idx+1));
        }

        user(const std::string &user = "", const std::string &pass = "")
            : u(user)
            , p(pass)
        {}

        std::string username() const {return u;}
        void set_username(const std::string &name) {u = name;}

        std::string password() const {return p;}
        void set_password(const std::string &password) {p = password;}

        std::string serialize() const {return u + ":" + p;}
        std::string to_basic_auth() const {return "Basic " + base64::encode_copy(serialize());}
        std::string to_xwww_form_url_encoded() const
        {
            return "name=" +
                    ascii_string_tools::to_percent_encoded_copy(u) +
                    "&password=" +
                    ascii_string_tools::to_percent_encoded_copy(p);
        }

    private:
        std::string u, p;
    };
}

#endif // CPPCOUCH_USER_H
