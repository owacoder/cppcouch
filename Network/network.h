#ifndef NETWORK_H
#define NETWORK_H

#include "../Couch/shared.h"
#include "../Couch/communication.h"
#include <memory>

#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/NetException.h>

#include <Poco/URI.h>

namespace couchdb
{
    struct http_url_impl : public http_url_base, public Poco::URI
    {
        http_url_impl(const std::string &url = std::string()) : Poco::URI(url) {}
        virtual ~http_url_impl() {}

        std::string to_string() const {return toString();}
        void from_string(const std::string &url)
        {
            Poco::URI uri(url);
            uri.swap(*this);
        }

        std::string get_scheme() const {return getScheme();}
        void set_scheme(const std::string &scheme) {setScheme(scheme);}

        std::string get_username() const {return ascii_string_tools::split(getUserInfo(), ':', true).front();}
        void set_username(const std::string &username)
        {
            std::string userInfo = getUserInfo();
            size_t idx = userInfo.find(':');
            if (idx != std::string::npos)
            {
                userInfo.erase(0, idx);
                userInfo.insert(0, username);
            }
            else
                userInfo = username;
            setUserInfo(userInfo);
        }

        std::string get_password() const
        {
            auto split = ascii_string_tools::split(getUserInfo(), ':', true);
            if (split.size() > 1)
                return split.back();
            return {};
        }
        void set_password(const std::string &password)
        {
            std::string userInfo = getUserInfo();
            size_t idx = userInfo.rfind(':');
            if (idx != std::string::npos)
            {
                userInfo.erase(idx+1);
                userInfo.append(password);
            }
            else
                userInfo = ":" + password;
            setUserInfo(userInfo);
        }

        std::string get_host() const {return getHost();}
        void set_host(const std::string &host) {setHost(host);}

        unsigned short get_port() const {return getPort();}
        void set_port(unsigned short port) {setPort(port);}

        std::string get_path() const {return getPath();}
        void set_path(const std::string &path) {setPath(path);}

        std::string get_query() const {return getQuery();}
        void set_query(const std::string &query) {setQuery(query);}

        std::string get_fragment() const {return getFragment();}
        void set_fragment(const std::string &fragment) {setFragment(fragment);}

        std::string get_authority() const {return getAuthority();}
        void set_authority(const std::string &authority) {setAuthority(authority);}
    };

    template<bool allow_caching = true>
    struct http_impl : public http_client_base<http_url_impl>
    {
        http_impl(std::shared_ptr<Poco::Net::HTTPClientSession> c = std::make_shared<Poco::Net::HTTPClientSession>(),
                  std::shared_ptr<Poco::Net::HTTPSClientSession> s = std::make_shared<Poco::Net::HTTPSClientSession>()) : client(c), sclient(s) {}

        typedef http_impl type;

        bool allow_cached_responses() const {return allow_caching;}

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
        virtual int operator()(std::string &url,
                               duration_type &timeout,
                               mode_type &timeout_mode,
                               std::map<std::string, std::string> &headers,
                               std::string &method,
                               const std::string &data,
                               std::string &response_buffer,
                               bool &network_error,
                               std::string &error_description)
        {
            Poco::URI uri(url);

            (void) timeout;
            (void) timeout_mode;

            try
            {
                // HTTPS connection
                if (uri.getScheme() == "https")
                {
                    if (uri.getHost() != sclient->getHost() ||
                        (uri.getPort() != sclient->getPort() && uri.getPort() != 0))
                    {
                        sclient->reset();
                        sclient->setHost(uri.getHost());
                        sclient->setPort(uri.getPort());
                        sclient->setKeepAlive(true);
                    }

                    Poco::Net::HTTPRequest request(method, url, "HTTP/1.1");
                    for (auto it = headers.begin(); it != headers.end(); ++it)
                        request.add(it->first, it->second);

                    sclient->sendRequest(request) << data;

#ifdef CPPCOUCH_FULL_DEBUG
                    std::cout << method << " " << url << std::endl;
                    for (auto it = request.begin(); it != request.end(); ++it)
                        std::cout << it->first << ": " << it->second << std::endl;
                    std::cout << data << std::endl;
#endif

                    Poco::Net::HTTPResponse response;
                    std::ostringstream response_stream;
                    sclient->receiveResponse(response) >> response_stream.rdbuf();
                    response_buffer = response_stream.str();

                    int status = static_cast<int>(response.getStatus());
                    network_error = status / 100 != 2;
                    error_description = response.getReason();

                    headers.clear();
                    for (auto it = response.begin(); it != response.end(); ++it)
                        headers[ascii_string_tools::to_lower_copy(it->first)] = it->second;

#ifdef CPPCOUCH_FULL_DEBUG
                    std::cout << method << " " << url << std::endl;
                    for (auto it = response.begin(); it != response.end(); ++it)
                        std::cout << it->first << ": " << it->second << std::endl;
                    std::cout << response_buffer << std::endl;
#endif

                    return status;
                }
                // Normal HTTP connection
                else
                {
                    if (uri.getHost() != client->getHost() ||
                        (uri.getPort() != client->getPort() && uri.getPort() != 0))
                    {
                        client->reset();
                        client->setHost(uri.getHost());
                        client->setPort(uri.getPort());
                        client->setKeepAlive(true);
                    }

                    Poco::Net::HTTPRequest request(method, url, "HTTP/1.1");
                    for (auto it = headers.begin(); it != headers.end(); ++it)
                        request.add(it->first, it->second);
                    client->sendRequest(request) << data;

                    Poco::Net::HTTPResponse response;
                    std::ostringstream response_stream;
                    client->receiveResponse(response) >> response_stream.rdbuf();
                    response_buffer = response_stream.str();

                    int status = static_cast<int>(response.getStatus());
                    network_error = status / 100 != 2;
                    error_description = response.getReason();

                    headers.clear();
                    for (auto it = response.begin(); it != response.end(); ++it)
                        headers[ascii_string_tools::to_lower_copy(it->first)] = it->second;

                    return status;
                }
            }
            catch (const Poco::Net::NetException &e)
            {
                sclient.reset();
                client.reset();
                network_error = true;
                error_description = e.what();
                return 0;
            }
        }

    private:
        std::shared_ptr<Poco::Net::HTTPClientSession> client;
        std::shared_ptr<Poco::Net::HTTPSClientSession> sclient;
    };
}

#endif // NETWORK_H
