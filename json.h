#ifndef JSON_H
#define JSON_H

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <codecvt>
#include <locale>
#include <type_traits>
#include <math.h>

namespace json
{
    enum type
    {
        null,
        boolean,
        integer,
        real,
        string,
        array,
        object
    };

    class value;

    typedef bool bool_t;
    typedef int64_t int_t;
    typedef double real_t;
    typedef const char *cstring_t;
    typedef std::string string_t;
    typedef std::vector<value> array_t;
    typedef std::map<string_t, value> object_t;

    struct error
    {
        error(cstring_t reason) : what_(reason) {}

        cstring_t what() const {return what_;}

    private:
        cstring_t what_;
    };

    class value
    {
    public:
        value() : type_(null) {}
        value(bool_t v) : type_(boolean), bool_(v) {}
        value(int_t v) : type_(integer), int_(v) {}
        value(real_t v) : type_(real), real_(v) {}
        value(cstring_t v) : type_(string), str_(v) {}
        value(const string_t &v) : type_(string), str_(v) {}
        value(const array_t &v) : type_(array), arr_(v) {}
        value(const object_t &v) : type_(object), obj_(v) {}
        template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        value(T v) : type_(integer), int_(v) {}
        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        value(T v) : type_(real), real_(v) {}

        type get_type() const {return type_;}
        size_t size() const {return type_ == array? arr_.size(): type_ == object? obj_.size(): 0;}

        bool_t is_null() const {return type_ == null;}
        bool_t is_bool() const {return type_ == boolean;}
        bool_t is_int() const {return type_ == integer;}
        bool_t is_real() const {return type_ == real || type_ == integer;}
        bool_t is_string() const {return type_ == string;}
        bool_t is_array() const {return type_ == array;}
        bool_t is_object() const {return type_ == object;}

        bool_t get_bool() const {return bool_;}
        int_t get_int() const {return int_;}
        real_t get_real() const {return type_ == integer? int_: real_;}
        cstring_t get_cstring() const {return str_.c_str();}
        const string_t &get_string() const {return str_;}
        const array_t &get_array() const {return arr_;}
        const object_t &get_object() const {return obj_;}

        bool_t &get_bool() {clear(boolean); return bool_;}
        int_t &get_int() {clear(integer); return int_;}
        real_t &get_real() {clear(real); return real_;}
        string_t &get_string() {clear(string); return str_;}
        array_t &get_array() {clear(array); return arr_;}
        object_t &get_object() {clear(object); return obj_;}

        void set_null() {clear(null);}
        void set_bool(bool_t v) {clear(boolean); bool_ = v;}
        void set_int(int_t v) {clear(integer); int_ = v;}
        void set_real(real_t v) {clear(real); real_ = v;}
        void set_string(cstring_t v) {clear(string); str_ = v;}
        void set_string(const string_t &v) {clear(string); str_ = v;}
        void set_array(const array_t &v) {clear(array); arr_ = v;}
        void set_object(const object_t &v) {clear(object); obj_ = v;}

        value operator[](const string_t &key) const
        {
            auto it = obj_.find(key);
            if (it != obj_.end())
                return it->second;
            return value();
        }
        value &operator[](const string_t &key) {clear(object); return obj_[key];}
        bool_t is_member(cstring_t key) const {return obj_.find(key) != obj_.end();}
        bool_t is_member(const string_t &key) const {return obj_.find(key) != obj_.end();}
        void erase(const string_t &key) {obj_.erase(key);}

        void push_back(const value &v) {clear(array); arr_.push_back(v);}
        void push_back(value &&v) {clear(array); arr_.push_back(v);}
        const value &operator[](size_t pos) const {return arr_[pos];}
        value &operator[](size_t pos) {return arr_[pos];}
        void erase(int_t pos) {arr_.erase(arr_.begin() + pos);}

        // The following are convenience conversion functions
        bool_t get_bool(bool_t default_) const {return is_bool()? bool_: default_;}
        int_t get_int(int_t default_) const {return is_int()? int_: default_;}
        real_t get_real(real_t default_) const {return is_real()? real_: default_;}
        cstring_t get_string(cstring_t default_) const {return is_string()? str_.c_str(): default_;}
        string_t get_string(const string_t &default_) const {return is_string()? str_: default_;}
        array_t get_array(const array_t &default_) const {return is_array()? arr_: default_;}
        object_t get_object(const object_t &default_) const {return is_object()? obj_: default_;}

        bool_t as_bool(bool_t default_ = false) const {return value(*this).convert_to(boolean, default_).bool_;}
        int_t as_int(int_t default_ = 0) const {return value(*this).convert_to(integer, default_).int_;}
        real_t as_real(real_t default_ = 0.0) const {return value(*this).convert_to(real, default_).real_;}
        string_t as_string(const string_t &default_ = string_t()) const {return value(*this).convert_to(string, default_).str_;}
        array_t as_array(const array_t &default_ = array_t()) const {return value(*this).convert_to(array, default_).arr_;}
        object_t as_object(const object_t &default_ = object_t()) const {return value(*this).convert_to(object, default_).obj_;}

        bool_t &convert_to_bool(bool_t default_ = false) {return convert_to(boolean, default_).bool_;}
        int_t &convert_to_int(int_t default_ = 0) {return convert_to(integer, default_).int_;}
        real_t &convert_to_real(real_t default_ = 0.0) {return convert_to(real, default_).real_;}
        string_t &convert_to_string(const string_t &default_ = string_t()) {return convert_to(string, default_).str_;}
        array_t &convert_to_array(const array_t &default_ = array_t()) {return convert_to(array, default_).arr_;}
        object_t &convert_to_object(const object_t &default_ = object_t()) {return convert_to(object, default_).obj_;}

    private:
        void clear(type new_type)
        {
            if (type_ == new_type)
                return;

            str_.clear(); str_.shrink_to_fit();
            arr_.clear(); arr_.shrink_to_fit();
            obj_.clear();
            type_ = new_type;
        }

        value &convert_to(type new_type, value default_value)
        {
            if (type_ == new_type)
                return *this;

            switch (type_)
            {
                case null: *this = default_value; break;
                case boolean:
                {
                    clear(new_type);
                    switch (new_type)
                    {
                        case integer: int_ = bool_; break;
                        case real: real_ = bool_; break;
                        case string: str_ = bool_? "true": "false"; break;
                        default: *this = default_value; break;
                    }
                    break;
                }
                case integer:
                {
                    clear(new_type);
                    switch (new_type)
                    {
                        case boolean: bool_ = int_ != 0; break;
                        case real: real_ = int_; break;
                        case string: str_ = std::to_string(int_); break;
                        default: *this = default_value; break;
                    }
                    break;
                }
                case real:
                {
                    clear(new_type);
                    switch (new_type)
                    {
                        case boolean: bool_ = real_ != 0.0; break;
                        case integer: int_ = (real_ >= INT64_MIN && real_ <= INT64_MAX)? trunc(real_): 0; break;
                        case string: str_ = std::to_string(real_); break;
                        default: *this = default_value; break;
                    }
                    break;
                }
                case string:
                {
                    switch (new_type)
                    {
                        case boolean: bool_ = str_ == "true"; break;
                        case integer:
                        {
                            std::istringstream str(str_);
                            str >> int_;
                            if (!str)
                                int_ = 0;
                            break;
                        }
                        case real:
                        {
                            std::istringstream str(str_);
                            str >> real_;
                            if (!str)
                                real_ = 0.0;
                            break;
                        }
                        default: *this = default_value; break;
                    }
                    clear(new_type);
                    break;
                }
                default: break;
            }

            return *this;
        }

        type type_;
        union
        {
            bool_t bool_;
            int_t int_;
            real_t real_;
        };
        string_t str_;
        array_t arr_;
        object_t obj_;
    };

    inline bool operator==(const value &lhs, const value &rhs)
    {
        if (lhs.get_type() != rhs.get_type())
            return false;

        switch (lhs.get_type())
        {
            case null: return true;
            case boolean: return lhs.get_bool() == rhs.get_bool();
            case integer: return lhs.get_int() == rhs.get_int();
            case real: return lhs.get_real() == rhs.get_real();
            case string: return lhs.get_string() == rhs.get_string();
            case array: return lhs.get_array() == rhs.get_array();
            case object: return lhs.get_object() == rhs.get_object();
            default: return false;
        }
    }

    inline bool operator!=(const value &lhs, const value &rhs)
    {
        return !(lhs == rhs);
    }

    inline bool stream_starts_with(std::istream &stream, const char *str)
    {
        int c;
        while (*str)
        {
            c = stream.get();
            if ((*str++ & 0xff) != c) return false;
        }
        return true;
    }

    inline std::istream &read_string(std::istream &stream, std::string &str)
    {
        static const std::string hex = "0123456789ABCDEF";
        int c;

        c = stream.get();
        if (c != '"') throw error("expected string");

        str.clear();
        while (c = stream.get(), c != '"')
        {
            if (c == EOF) throw error("unexpected end of string");

            if (c == '\\')
            {
                c = stream.get();
                if (c == EOF) throw error("unexpected end of string");

                switch (c)
                {
                    case 'b': str.push_back('\b'); break;
                    case 'f': str.push_back('\f'); break;
                    case 'n': str.push_back('\n'); break;
                    case 'r': str.push_back('\r'); break;
                    case 't': str.push_back('\t'); break;
                    case 'u':
                    {
                        uint32_t code = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            c = stream.get();
                            if (c == EOF) throw error("unexpected end of string");
                            size_t pos = hex.find(toupper(c));
                            if (pos == std::string::npos) throw error("invalid character escape sequence");
                            code = (code << 4) | pos;
                        }

                        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> utf8;
                        str += utf8.to_bytes(code);

                        break;
                    }
                    default:
                        str.push_back(c); break;
                }
            }
            else
                str.push_back(c);
        }

        return stream;
    }

    inline std::ostream &write_string(std::ostream &stream, const std::string &str)
    {
        static const char hex[] = "0123456789ABCDEF";
        stream << '"';
        for (size_t i = 0; i < str.size(); ++i)
        {
            int c = str[i] & 0xff;

            if (c == '"' || c == '\\')
                stream << '\\' << static_cast<char>(c);
            else
            {
                switch (c)
                {
                    case '"':
                    case '\\': stream << '\\' << static_cast<char>(c); break;
                    case '\b': stream << "\\b"; break;
                    case '\f': stream << "\\f"; break;
                    case '\n': stream << "\\n"; break;
                    case '\r': stream << "\\r"; break;
                    case '\t': stream << "\\t"; break;
                    default:
                        if (iscntrl(c))
                            stream << "\\u00" << hex[c >> 4] << hex[c & 0xf];
                        else
                            stream << str[i];
                        break;
                }
            }
        }

        return stream << '"';
    }

    inline std::istream &operator>>(std::istream &stream, value &v)
    {
        char chr;

        stream >> std::skipws >> chr, stream.unget(); // Peek at next character past whitespace

        if (stream.good())
        {
            switch (chr)
            {
                case 'n':
                    if (!stream_starts_with(stream, "null")) throw error("expected 'null' value");
                    v.set_null();
                    return stream;
                case 't':
                    if (!stream_starts_with(stream, "true")) throw error("expected 'true' value");
                    v.set_bool(true);
                    return stream;
                case 'f':
                    if (!stream_starts_with(stream, "false")) throw error("expected 'false' value");
                    v.set_bool(false);
                    return stream;
                case '"':
                {
                    std::string str;
                    read_string(stream, str);
                    v.set_string(str);
                    return stream;
                }
                case '[':
                    stream >> chr; // Eat '['
                    v.set_array(array_t());

                    // Peek at next character past whitespace
                    stream >> chr;
                    if (!stream || chr == ']') return stream;

                    stream.unget(); // Replace character we peeked at
                    do
                    {
                        value item;
                        stream >> item;
                        v.push_back(item);

                        stream >> chr;
                        if (!stream || (chr != ',' && chr != ']'))
                            throw error("expected ',' separating array elements or ']' ending array");
                    } while (stream && chr != ']');

                    return stream;
                case '{':
                    stream >> chr; // Eat '{'
                    v.set_object(object_t());

                    // Peek at next character past whitespace
                    stream >> chr;
                    if (!stream || chr == '}') return stream;

                    stream.unget(); // Replace character we peeked at
                    do
                    {
                        std::string key;
                        value item;

                        read_string(stream, key);
                        stream >> chr;
                        if (chr != ':') throw error("expected ':' separating key and value in object");
                        stream >> item;
                        v[key] = item;

                        stream >> chr;
                        if (!stream || (chr != ',' && chr != '}'))
                            throw error("expected ',' separating key value pairs or '}' ending object");
                    } while (stream && chr != '}');

                    return stream;
                default:
                    if (isdigit(chr) || chr == '-')
                    {
                        real_t r;
                        stream >> r;
                        if (!stream) throw error("invalid number");

                        if (r == trunc(r) && r >= INT64_MIN && r <= INT64_MAX)
                            v.set_int(static_cast<int_t>(r));
                        else
                            v.set_real(r);

                        return stream;
                    }
                    break;
            }
        }

        throw error("expected JSON value");
    }

    inline std::ostream &operator<<(std::ostream &stream, const value &v)
    {
        switch (v.get_type())
        {
            case null: return stream << "null";
            case boolean: return stream << (v.get_bool()? "true": "false");
            case integer: return stream << v.get_int();
            case real: return stream << v.get_real();
            case string: return write_string(stream, v.get_string());
            case array:
            {
                stream << '[';
                for (auto it = v.get_array().begin(); it != v.get_array().end(); ++it)
                {
                    if (it != v.get_array().begin())
                        stream << ',';
                    stream << *it;
                }
                return stream << ']';
            }
            case object:
            {
                stream << '{';
                for (auto it = v.get_object().begin(); it != v.get_object().end(); ++it)
                {
                    if (it != v.get_object().begin())
                        stream << ',';
                    write_string(stream, it->first) << ':' << it->second;
                }
                return stream << '}';
            }
        }

        // Control will never get here
        return stream;
    }

    inline std::ostream &pretty_print(std::ostream &stream, const value &v, size_t indent_width, size_t start_indent = 0)
    {
        switch (v.get_type())
        {
            case null: return stream << "null";
            case boolean: return stream << (v.get_bool()? "true": "false");
            case integer: return stream << v.get_int();
            case real: return stream << v.get_real();
            case string: return write_string(stream, v.get_string());
            case array:
            {
                if (v.get_array().empty())
                    return stream << "[]";

                stream << "[\n";
                for (auto it = v.get_array().begin(); it != v.get_array().end(); ++it)
                {
                    if (it != v.get_array().begin())
                        stream << ",\n";
                    stream << std::string(indent_width * (start_indent + 1), ' ');
                    pretty_print(stream, *it, indent_width, start_indent + 1);
                }
                return stream << '\n' << std::string(indent_width * start_indent, ' ') << "]";
            }
            case object:
            {
                if (v.get_object().empty())
                    return stream << "{}";

                stream << "{\n";
                for (auto it = v.get_object().begin(); it != v.get_object().end(); ++it)
                {
                    if (it != v.get_object().begin())
                        stream << ",\n";
                    stream << std::string(indent_width * (start_indent + 1), ' ');
                    pretty_print(write_string(stream, it->first) << ": ", it->second, indent_width, start_indent + 1);
                }
                return stream << '\n' << std::string(indent_width * start_indent, ' ') << "}";
            }
        }

        // Control will never get here
        return stream;
    }

    inline value from_json(const std::string &json)
    {
        std::istringstream stream(json);
        value v;
        stream >> v;
        return v;
    }

    inline std::string to_json(const value &v)
    {
        std::ostringstream stream;
        stream << v;
        return stream.str();
    }

    inline std::string to_pretty_json(const value &v, size_t indent_width)
    {
        std::ostringstream stream;
        pretty_print(stream, v, indent_width);
        return stream.str();
    }
}

#endif // JSON_H
