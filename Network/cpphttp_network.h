#ifndef CPPHTTP_NETWORK_H
#define CPPHTTP_NETWORK_H

#include "../Couch/shared.h"
#include "../Couch/communication.h"

#include "CppHttp/cpphttp.h"

#include <thread>

namespace couchdb
{
    struct asio_url_impl : public http_url_base, public CppHttp::Uri
    {
        asio_url_impl(const std::string &url = std::string()) : CppHttp::Uri(url) {}
        virtual ~asio_url_impl() {}

        std::string to_string() const {return toString();}
        void from_string(const std::string &url) {fromString(url).swap(*this);}

        std::string get_scheme() const {return scheme();}
        void set_scheme(const std::string &scheme) {setScheme(scheme);}

        std::string get_username() const {return username();}
        void set_username(const std::string &username) {setUsername(username);}

        std::string get_password() const {return password();}
        void set_password(const std::string &password) {setPassword(password);}

        std::string get_host() const {return host();}
        void set_host(const std::string &host) {setHost(host);}

        unsigned short get_port() const {return port();}
        void set_port(unsigned short port) {setPort(port);}

        std::string get_path() const {return path();}
        void set_path(const std::string &path) {setPath(path);}

        std::string get_query() const {return query();}
        void set_query(const std::string &query) {setQuery(query);}

        std::string get_fragment() const {return fragment();}
        void set_fragment(const std::string &fragment) {setFragment(fragment);}

        std::string get_authority() const {return authority();}
        void set_authority(const std::string &authority)
        {
            CppHttp::Uri uri(authority);
            setHost(uri.host());
            setPort(uri.port());
            setUserInfo(uri.userInfo());
        }
    };

    struct asio_http_response_handle
    {
        void add_response(CppHttp::Http::Connection &,
                          const CppHttp::Http::Response &response,
                          const boost::system::error_code &ec)
        {
            if (!ec)
                responses.push_back(response.body());
        }

        std::shared_ptr<CppHttp::Http::Connection> connection;
        std::deque<std::string> responses;
    };

    template<bool allow_caching = true>
    struct asio_http_impl : public http_client_base<asio_url_impl, /* URL implementation */
                                               boost::posix_time::time_duration, /* Timeout duration */
                                               CppHttp::Http::Connection::TimeoutMode, /* Timeout mode */
                                               std::shared_ptr<asio_http_response_handle> /* Response handle */>
    {
        asio_http_impl(std::shared_ptr<CppHttp::Http::ConnectionManager> manager = std::make_shared<CppHttp::Http::ConnectionManager>())
            : client(manager) {}

        typedef asio_http_impl type;

        response_handle_type invalid_handle() const {return response_handle_type();}

        bool is_invalid_handle(response_handle_type handle) const
        {
            return !handle || !handle->connection || handle->connection->disconnected();
        }

        bool allow_cached_responses() const {return allow_caching;}

        void reset() {client->stop();}

        /*          url       (IN): The URL to visit.
         *      timeout       (IN): The length of time before timeout should occur.
         * timeout_mode       (IN): Implementation-specific choice of how to timeout.
         *      headers   (IN/OUT): The HTTP headers to be used for the request (WARNING: the list may not be all-inclusive, and
         *                        may not contain all required header fields, e.g. Content-Length). Output to this parameter
         *                        MUST specify all keys as lowercase.
         *       method       (IN): What HTTP method to use (e.g. GET, PUT, DELETE, POST, COPY, etc.). Case is not specified.
         *         data       (IN): The payload to send as the body of the request.
         * response_buffer   (OUT): Where to put the body of the response.
         *   network_error   (OUT): Must be set to true (1) if an error occured, false (0) otherwise.
         * error_description (OUT): Set to a human-readable description of what error occured.
         *
         * Return value: Must return the HTTP status code, or zero if an error occured before the response arrived
         */
        virtual int operator()(const std::string &url,
                               duration_type timeout,
                               mode_type timeout_mode,
                               std::map<std::string, std::string> &headers,
                               const std::string &method,
                               const std::string &data,
                               std::string &response_buffer,
                               bool &network_error,
                               std::string &error_description)
        {
            CppHttp::Http::Request request(url, headers);
            request.setBody(data);

            auto connection = client->createConnection(request);
            connection->setTimeout(timeout);
            connection->setTimeoutMode(timeout_mode);
            connection->setRequest(request, method);
            if (connection->disconnected())
                connection->connect();
            else
                connection->sendRequest();
            connection->wait_for_transaction();
            client->freeConnection(connection);

            CppHttp::Http::Response response = connection->response();
            response_buffer = response.body();

            int status = static_cast<int>(response.code());
            network_error = status / 100 != 2;
            error_description = response.message();

            headers.clear();
            for (auto it = response.headers().begin(); it != response.headers().end(); ++it)
                headers[ascii_string_tools::to_lower_copy(it->first)] = it->second;

#ifdef CPPCOUCH_FULL_DEBUG
            std::cout << method << " " << url << std::endl;
            for (auto it = response.headers().begin(); it != response.headers().end(); ++it)
                std::cout << it->first << ": " << it->second << std::endl;
            std::cout << response_buffer << std::endl;
#endif

            return status;
        }

        /*          url       (IN): The URL to visit.
         *      timeout       (IN): The length of time before timeout should occur.
         * timeout_mode       (IN): Implementation-specific choice of how to timeout.
         *      headers   (IN/OUT): The HTTP headers to be used for the request (WARNING: the list may not be all-inclusive, and
         *                        may not contain all required header fields, e.g. Content-Length). Output to this parameter
         *                        MUST specify all keys as lowercase.
         *       method       (IN): What HTTP method to use (e.g. GET, PUT, DELETE, POST, COPY, etc.). Case is not specified.
         *         data       (IN): The payload to send as the body of the request.
         * response_buffer   (OUT): Where to put the body of the response.
         *   network_error   (OUT): Must be set to true (1) if an error occured, false (0) otherwise.
         * error_description (OUT): Set to a human-readable description of what error occured.
         *
         * Return value: Must return the HTTP status code, or zero if an error occured before the response arrived
         *
         * NOTE: the response_buffer must be completely read before reusing this http_impl object, or reset() must be called
         */
        virtual int get_response_handle(const std::string &url,
                                        duration_type timeout,
                                        mode_type timeout_mode,
                                        std::map<std::string, std::string> &headers,
                                        const std::string &method,
                                        const std::string &data,
                                        response_handle_type &response_buffer,
                                        bool &network_error,
                                        std::string &error_description)
        {
            response_handle_type response_handle = std::make_shared<asio_http_response_handle>();

            CppHttp::Http::Request request(url, headers);
            request.setBody(data);

            auto connection = client->createConnection(request);
            response_handle->connection = connection;
            connection->setTimeout(timeout);
            connection->setTimeoutMode(timeout_mode);
            connection->setPartialResponseHandler(boost::bind(&asio_http_response_handle::add_response, response_handle.get(), _1, _2, _3));
            connection->setPartialResponseType(CppHttp::Http::Connection::ResponseLine);
            connection->setRequest(request, method);
            if (connection->disconnected())
                connection->connect();
            else
                connection->sendRequest();

            while (connection->response().code() == CppHttp::Http::Invalid)
            {
                connection->start();
                std::this_thread::yield();
            }

            CppHttp::Http::Response response = connection->response();

            int status = static_cast<int>(response.code());
            network_error = status / 100 != 2;
            error_description = response.message();

            headers.clear();
            for (auto it = response.headers().begin(); it != response.headers().end(); ++it)
                headers[ascii_string_tools::to_lower_copy(it->first)] = it->second;

#ifdef CPPCOUCH_FULL_DEBUG
            std::cout << method << " " << url << std::endl;
            for (auto it = response.headers().begin(); it != response.headers().end(); ++it)
                std::cout << it->first << ": " << it->second << std::endl;
#endif

            response_buffer = network_error? response_handle_type(): response_handle;
            return status;
        }

        /* Read a line from a response handle.
         * Returns an empty line if no line is available
         */
        virtual std::string read_line_from_response_handle(response_handle_type handle)
        {
            std::string line;
            if (handle && handle->connection)
            {
                if (handle->connection->connected())
                    handle->connection->start();

                if (!handle->responses.empty())
                {
                    line = handle->responses.front();
                    handle->responses.pop_front();
                }
            }

            while (!line.empty() && strchr("\r\n", line.back()))
                line.pop_back();

            return line;
        }

    private:
        std::shared_ptr<CppHttp::Http::ConnectionManager> client;
    };
}

#endif // CPPHTTP_NETWORK_H
