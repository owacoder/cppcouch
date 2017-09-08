#ifndef CPPCOUCH_COMMUNICATION_H
#define CPPCOUCH_COMMUNICATION_H

#include <map>
#include <string>
#include <memory>

#include "shared.h"
#include "user.h"

#define CPPCOUCH_DEFAULT_URL "http://localhost:5984"
#define CPPCOUCH_DEFAULT_NODE_URL "http://localhost:5986"
#define CPPCOUCH_DEFAULT_SSL_URL "https://localhost:6984"

namespace couchdb
{
    /* Communication class - The base class for communicating with CouchDB,
     * not generally used by anything other than the API itself.
     *
     * This class handles the HTTP network requests sent to CouchDB, as well as errors.
     */

    inline std::string local() {return CPPCOUCH_DEFAULT_URL;}
    inline std::string local_ssl() {return CPPCOUCH_DEFAULT_SSL_URL;}
    inline unsigned short local_port() {return 5984;}
    inline unsigned short local_ssl_port() {return 6984;}
    inline unsigned short local_cluster_node_port() {return 5986;}

    template<typename http_client> class changes;

    template<typename http_client>
    class communication
    {
        friend class changes<http_client>;

        communication(const communication &) {}
        communication &operator=(const communication &) {return *this;}

    public:
        typedef typename http_client::url_type http_client_url_t;
        typedef typename http_client::duration_type http_client_timeout_duration_t;
        typedef typename http_client::mode_type http_client_timeout_mode_t;

        typedef std::map<std::string, std::string> header_map;

        class state
        {
            friend class communication;

        public:
            state(http_client_timeout_duration_t timeout_,
                  http_client_timeout_mode_t timeout_mode_,
                  const std::string &url_,
                  const user &user_,
                  auth_type auth_,
                  const std::string &cookie_)
                : timeout_(timeout_)
                , timeout_mode_(timeout_mode_)
                , url_(url_)
                , user_(user_)
                , auth_type_(auth_)
                , cookie_(cookie_)
            {}

            state()
                : timeout_(static_cast<http_client_timeout_duration_t>(-1))
                , timeout_mode_(static_cast<http_client_timeout_mode_t>(0))
                , auth_type_(auth_none)
            {}

        private:
            void set_url(const std::string &url)
            {
                if (url == url_)
                    return;

                cached_responses_.clear();
            }

            http_client_timeout_duration_t timeout_;
            http_client_timeout_mode_t timeout_mode_;
            std::string url_;
            std::string buffer_;

            user user_;
            auth_type auth_type_;
            std::string cookie_;

            std::map<std::string, std::string> cached_responses_; // Map of URL -> raw responses
        };

        communication(http_client _network = http_client(), const std::string &url = std::string(), const user &_user = user(), auth_type auth = auth_none, http_client_timeout_duration_t timeout = static_cast<http_client_timeout_duration_t>(-1))
            : client(_network), d(timeout, static_cast<http_client_timeout_mode_t>(0), url, _user, auth, std::string())
        {}

        http_client &get_client() {return client;}

        // Save and restore the current state
        // State objects are not modifiable except by this class
        const state &get_current_state() const {return d;}
        void set_current_state(const state &_state) {d = _state;}

        json::value get_data(const std::string &url, const std::string &method = "GET",
                           const std::string &data = "", bool cacheable = false)
        {
            return get_data(url, method, data, header_map(), cacheable);
        }

        json::value get_data(const std::string &url, const header_map &headers,
                           const std::string &method = "GET",
                           const std::string &data = "", bool cacheable = false)
        {
            return get_data(url, method, data, headers, cacheable);
        }

        std::string get_raw_data(const std::string &url, const std::string &method = "GET", const header_map &headers = header_map(), const std::string &data = "", bool cacheable = false)
        {
            get_raw_data(url, method, data, headers, cacheable);
            return d.buffer_;
        }

        // Timeout in milliseconds
        http_client_timeout_duration_t get_timeout() const {return d.timeout_;}
        void set_timeout(http_client_timeout_duration_t timeout) {d.timeout_ = timeout;}

        http_client_timeout_mode_t get_timeout_mode() const {return d.timeout_mode_;}
        void set_timeout_mode(http_client_timeout_mode_t mode) {d.timeout_mode_ = mode;}

        // The base URL every request is referring to
        std::string get_server_url() const {return d.url_;}
        void set_server_url(const std::string &url) {d.set_url(url);}

        // Clear the internal response cache
        void clear_cache() {d.cached_responses_.clear();}

        // The credentials used for authentication
        user get_user() const {return d.user_;}
        void set_user(const user &_user)
        {
            d.user_ = _user;
            d.cookie_.clear();
        }

        // The type of authentication
        auth_type get_auth_type() const {return d.auth_type_;}
        std::string get_auth_type_readable() const
        {
            switch (d.auth_type_)
            {
                case auth_basic: return "Basic";
                case auth_cookie: return "Cookie";
                case auth_none:
                default: return "None";
            }
        }
        void set_auth_type(auth_type type) {d.auth_type_ = type;}
        void set_auth_type(const std::string &type)
        {
            std::string lower(ascii_string_tools::to_lower_copy(type));
            if (lower == "none")
                set_auth_type(auth_none);
            else if (lower == "basic")
                set_auth_type(auth_basic);
            else if (lower == "cookie")
                set_auth_type(auth_cookie);
        }

    private:
        json::value get_data(const std::string &url, const std::string &method,
                           const std::string &data, const header_map &headers, bool cacheable)
        {
            get_raw_data(url, method, data, headers, cacheable);
            return string_to_json(d.buffer_);
        }

        void get_raw_data(const std::string &url_, std::string method,
                        const std::string &data, const header_map &headers, bool cacheable)
        {
            std::string url = d.url_ + url_;
            header_map new_headers;

            if (d.cached_responses_.find(url_) != d.cached_responses_.end())
            {
                d.buffer_ = d.cached_responses_[url_];
                return;
            }

            for (auto it: headers)
                new_headers[ascii_string_tools::to_lower_copy(it.first)] = it.second;

#ifdef CPPCOUCH_DEBUG
            std::cout << "Getting data: " << url << " [" << method << "]" << std::endl;
#endif

            d.buffer_.clear();
            bool statusCodeError = false;
            std::string errorDescription;
            int statusCode = 200;

            if (new_headers.find("content-type") == new_headers.end())
                new_headers["content-type"] = "application/json";
            if (new_headers.find("accept") == new_headers.end())
                new_headers["accept"] = "application/json";
            if (new_headers.find("content-length") == new_headers.end())
                new_headers["content-length"] = std::to_string(data.size());

            switch (d.auth_type_)
            {
                case auth_basic:
                    new_headers["authorization"] = d.user_.to_basic_auth();
                    break;
                case auth_cookie:
                    new_headers["cookie"] = d.cookie_;
                    break;
                default:
                    break;
            }

            statusCode = client(url, d.timeout_, d.timeout_mode_, new_headers, method, data, d.buffer_, statusCodeError, errorDescription);

            if (statusCodeError && statusCode == 0)
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << method << " " << url << " failed with error: " << errorDescription << std::endl;
                std::cout << method << " " << url << " status code: 400" << std::endl;
#endif
                throw error(error::communication_error, errorDescription, method + ' ' + url, 400, d.buffer_);
            }
            else if (statusCodeError)
            {
                bool throw_error = true;
                error::error_type err = error::communication_error;

                switch (statusCode)
                {
                    case E_Unauthorized:
                    case E_Forbidden:
                        err = error::forbidden;
                        break;
                    case E_Conflict:
                        err = error::document_conflict;
                        break;
                    case E_Gone:
                    case E_NotFound:
                        err = error::content_not_found;
                        break;
                    default:
                        if (statusCode / 100 == 4)
                            throw_error = false;
                        break;
                }

#ifdef CPPCOUCH_DEBUG
                std::cout << method << " " << url << " failed with error: " << errorDescription << std::endl;
                std::cout << method << " " << url << " status code: " << statusCode << std::endl;
#endif
                if (throw_error)
                    throw error(err, errorDescription, method + ' ' + url, statusCode, d.buffer_);
            }

            if (new_headers.find("set-cookie") != new_headers.end()) // Parse out cookie
            {
                std::vector<std::string> split;
                bool found = false;

                d.cookie_ = new_headers["set-cookie"];
                split = ascii_string_tools::split(d.cookie_, ';');

                for (std::string attr: split)
                {
                    ascii_string_tools::trim(attr);
                    if (attr.find("AuthSession") == 0)
                    {
                        d.cookie_ = attr;
                        found = true;
                        break;
                    }
                }

                if (!found)
                    d.cookie_.clear();
            }

            if (cacheable) // Cache response if possible
                d.cached_responses_[url_] = d.buffer_;

#ifdef CPPCOUCH_DEBUG
            std::cout << method << " " << url << " response: " << statusCode << std::endl;
            //for (Network::Http::Headers::const_iterator i = response.headers().begin(); i != response.headers().end(); ++i)
            //    std::cout << i->first << ": " << i->second << std::endl;
#endif
#ifdef CPPCOUCH_FULL_DEBUG
            std::cout << "Raw buffer: " << d.buffer_ << std::endl;
#endif
        }

        http_client client;
        state d;
    };
}

#endif // CPPCOUCH_COMMUNICATION_H
