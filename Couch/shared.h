#ifndef CPPCOUCH_SHARED_H
#define CPPCOUCH_SHARED_H

#include "../String/string_tools.h"
#include <json.h>
#include <sstream>
#include <iostream>
#include <map>
#include <memory>

namespace couchdb
{
    /* auth_type - Specifies how to provide credentials to the server, if any */
    enum auth_type
    {
        auth_none,
        auth_basic,
        auth_cookie
    };

    /* http_client_base class - Provides a base class for the network implementation.
     * All overloads must implement the specified API.
     */
    template<typename http_url_type_t, typename http_client_timeout_duration_t = int, typename http_client_timeout_mode_t = int, typename http_client_response_handle_t = int>
    struct http_client_base
    {
        // These typedefs MUST be set in a derived class of http_client_base
        typedef http_url_type_t url_type; // The URL class used with this client. The class must inherit `http_url_base`
        typedef http_client_timeout_duration_t duration_type; // The timeout duration type class
        typedef http_client_timeout_mode_t mode_type; // The timeout mode type class
        typedef http_client_response_handle_t response_handle_type; // The response handle type
        // This should ALWAYS be the type of the class instantiation.
        // The derived class should ALWAYS overload this typedef.
        // For example:
        //     struct derived : http_client_base<type, type>
        //     {
        //         typedef derived type;
        //         ...
        //     };
        typedef http_client_base<duration_type, mode_type> type;
        // Define the following to true if you want caching enabled
        virtual bool allow_cached_responses() const = 0;
        // Define the following to the invalid response handle (i.e. NULL, perhaps)
        virtual response_handle_type invalid_handle() const = 0;
        // Returns true if the passed handle is invalid (i.e. NULL, perhaps)
        virtual bool is_invalid_handle(response_handle_type handle) const = 0;
        // Reset this connection, closing all active connections
        virtual void reset() = 0;

        virtual ~http_client_base() {}

        /*          url       (IN): The URL to visit.
         *      timeout       (IN): The length of time before timeout should occur.
         * timeout_mode       (IN): Implementation-specific choice of how to timeout.
         *      headers   (IN/OUT): The HTTP headers to be used for the request (WARNING: the list may not be all-inclusive, and
         *                          may not contain all required header fields). The keys that will always be set are "Content-Type",
         *                          "Content-Length", and "Accept". NOTE: All keys sent to the client function will be entirely lowercase.
         *
         *                          Output to this parameter MUST specify all keys as lowercase.
         *       method       (IN): What HTTP method to use (e.g. GET, PUT, DELETE, POST, COPY, etc.). Case is not specified.
         *         data       (IN): The payload to send as the body of the request.
         *   response_buffer (OUT): Where to put the body of the response.
         *     network_error (OUT): Must be set to true (1) if an error occured, false (0) otherwise.
         * error_description (OUT): Set to a human-readable description of what error occured.
         *
         * Return value: Must return the HTTP status code, or zero (0) if an error occured before the response arrived.
         * The client is responsible for handling any redirect (3xx) HTTP codes
         */
        virtual int operator()(const std::string &url,
                               http_client_timeout_duration_t timeout,
                               http_client_timeout_mode_t timeout_mode,
                               std::map<std::string, std::string> &headers,
                               const std::string &method,
                               const std::string &data,
                               std::string &response_buffer,
                               bool &network_error,
                               std::string &error_description) = 0;

        /*          url       (IN): The URL to visit.
         *      timeout       (IN): The length of time before timeout should occur.
         * timeout_mode       (IN): Implementation-specific choice of how to timeout.
         *      headers   (IN/OUT): The HTTP headers to be used for the request (WARNING: the list may not be all-inclusive, and
         *                          may not contain all required header fields). The keys that will always be set are "Content-Type",
         *                          "Content-Length", and "Accept". NOTE: All keys sent to the client function will be entirely lowercase.
         *
         *                          Output to this parameter MUST specify all keys as lowercase.
         *       method       (IN): What HTTP method to use (e.g. GET, PUT, DELETE, POST, COPY, etc.). Case is not specified.
         *         data       (IN): The payload to send as the body of the request.
         *   response_buffer (OUT): Where to put the body of the response.
         *     network_error (OUT): Must be set to true (1) if an error occured, false (0) otherwise.
         * error_description (OUT): Set to a human-readable description of what error occured.
         *
         * Return value: Must return the HTTP status code, or zero (0) if an error occured before the response arrived.
         * The client is responsible for handling any redirect (3xx) HTTP codes
         */
        virtual int get_response_handle(const std::string &url,
                                        http_client_timeout_duration_t timeout,
                                        http_client_timeout_mode_t timeout_mode,
                                        std::map<std::string, std::string> &headers,
                                        const std::string &method,
                                        const std::string &data,
                                        response_handle_type &response_buffer,
                                        bool &network_error,
                                        std::string &error_description) = 0;

        /* Read a line from a response handle.
         * Either blocks until a line is available, or returns an empty line if no lines are available
         * (It doesn't matter which, it just helps the managing thread to shut the feed down sooner
         * if this function does not block)
         */
        virtual std::string read_line_from_response_handle(response_handle_type handle) = 0;
    };

    // This class must be used as the base class of a URL implementation
    struct http_url_base
    {
        virtual ~http_url_base() {}

        virtual std::string to_string() const = 0;
        virtual void from_string(const std::string &url) = 0;

        virtual std::string get_scheme() const = 0;
        virtual void set_scheme(const std::string &scheme) = 0;

        virtual std::string get_username() const = 0;
        virtual void set_username(const std::string &username) = 0;

        virtual std::string get_password() const = 0;
        virtual void set_password(const std::string &password) = 0;

        virtual std::string get_host() const = 0;
        virtual void set_host(const std::string &host) = 0;

        virtual unsigned short get_port() const = 0;
        virtual void set_port(unsigned short port) = 0;

        virtual std::string get_path() const = 0;
        virtual void set_path(const std::string &path) = 0;

        virtual std::string get_query() const = 0;
        virtual void set_query(const std::string &query) = 0;

        virtual std::string get_fragment() const = 0;
        virtual void set_fragment(const std::string &fragment) = 0;

        virtual std::string get_authority() const = 0;
        virtual void set_authority(const std::string &authority) = 0;
    };

    enum http_status_code
    {
        Invalid,

        Informational = 1,
        Success = 2,
        Redirect = 3,
        ClientError = 4,
        ServerError = 5,

        // Informational
        I_Continue = 100,
        I_SwitchingProtocols = 101,
        I_Processing = 102,

        // Success
        S_Ok = 200,
        S_Created = 201,
        S_Accepted = 202,
        S_NonAuthoritativeInfo = 203,
        S_NoContent = 204,
        S_ResetContent = 205,
        S_PartialContent = 206,
        S_MultiStatus = 207,
        S_AlreadyReported = 208,
        S_ImUsed = 209,

        // Redirect
        R_MultipleChoices = 300,
        R_MovedPermanently = 301,
        R_Found = 302,
        R_SeeOther = 303,
        R_NotModified = 304,
        R_UseProxy = 305,
        R_SwitchProxy = 306,
        R_TemporaryRedirect = 307,
        R_PermanentRedirect = 308,

        // Client error
        E_BadRequest = 400,
        E_Unauthorized = 401,
        E_PaymentRequired = 402,
        E_Forbidden = 403,
        E_NotFound = 404,
        E_MethodNotAllowed = 405,
        E_NotAcceptable = 406,
        E_ProxyAuthenticationRequired = 407,
        E_RequestTimeout = 408,
        E_Conflict = 409,
        E_Gone = 410,
        E_LengthRequired = 411,
        E_PreconditionFailed = 412,
        E_PayloadTooLarge = 413,
        E_UriTooLong = 414,
        E_UnsupportedMediaType = 415,
        E_RangeNotSatisfiable = 416,
        E_ExpectationFailed = 417,
        E_ImATeapot = 418,
        E_MisdirectedRequest = 421,
        E_UnprocessableEntity = 422,
        E_Locked = 423,
        E_FailedDependency = 424,
        E_UpgradeRequired = 426,
        E_PreconditionRequired = 428,
        E_TooManyRequests = 429,
        E_RequestHeaderFieldsTooLarge = 431,
        E_UnavailableForLegalReasons = 451,

        // Server error
        V_InternalServerError = 500,
        V_NotImplemented = 501,
        V_BadGateway = 502,
        V_ServiceUnavailable = 503,
        V_GatewayTimeout = 504,
        V_HttpVersionNotSupported = 505,
        V_VariantAlsoNegotiates = 506,
        V_InsufficientStorage = 507,
        V_LoopDetected = 508,
        V_NotExtended = 510,
        V_NetworkAuthenticationRequired = 511
    };

    /* Error class - Stores errors and descriptions thrown from functions.
     */

    class error
    {
    public:
        enum error_type
        {
            invalid_argument,
            unknown_error,

            forbidden,
            bad_response,
            request_failed,

            // With either of the following two errors, the network request, network response code,
            // and network response should always contain valid information
            communication_error,
            content_not_found,

            view_unavailable,

            attachment_unavailable,
            attachment_not_creatable,
            attachment_not_deletable,

            document_conflict,
            document_unavailable,
            document_not_creatable,
            document_not_deletable,

            database_unavailable,
            database_not_creatable,
            database_not_deletable
        };

        static inline std::string errorToString(error_type err)
        {
            switch (err)
            {
                case invalid_argument:
                    return "An invalid argument was passed to a function internally";
                case communication_error:
                    return "There was an error communicating with CouchDB";
                case forbidden:
                    return "The requested operation is forbidden by CouchDB";
                case bad_response:
                    return "The server returned a malformed response";
                case content_not_found:
                    return "The requested content was not found";
                case view_unavailable:
                    return "The requested view was not found";
                case attachment_unavailable:
                    return "The attachment requested could not be retrieved";
                case attachment_not_creatable:
                    return "The attachment could not be created";
                case attachment_not_deletable:
                    return "The attachment could not be deleted";
                case document_unavailable:
                    return "The document requested could not be retrieved";
                case document_not_creatable:
                    return "The document could not be created";
                case document_not_deletable:
                    return "The document could not be deleted";
                case database_unavailable:
                    return "The database requested could not be retrieved";
                case database_not_creatable:
                    return "The database could not be created";
                case database_not_deletable:
                    return "The database could not be deleted";
                case unknown_error:
                default:
                    return "An unknown error occured";
            }
        }

        error(error_type err, const std::string &str = std::string(),
              const std::string &request = std::string(),
              int network_response_code = 200,
              const std::string &network_response = std::string())
            : err(err)
            , response_code_(network_response_code)
            , request_(request)
            , response_(network_response)
            , str(str)
        {}

        error_type type() const {return err;}

        int network_response_code() const {return response_code_;}
        const std::string &network_request() const {return request_;}
        const std::string &network_response() const {return response_;}

        std::string reason() const
        {
            if (str.empty())
                return errorToString(err);
            else
                return str;
        }

    private:
        error_type err;

        int response_code_;
        std::string request_, response_;

        std::string str;
    };

    //general function to encode a URL
    inline std::string url_encode(const std::string &url)
    {
        return ascii_string_tools::to_percent_encoded_copy(url);
    }

    //encodes a document id for use in a URL
    inline std::string url_encode_doc_id(const std::string &url)
    {
        if (url.find("_design/") == 0)
            return "_design/" + url_encode(url.substr(8));
        else
            return url_encode(url);
    }

    //encodes a view id for use in a URL
    inline std::string url_encode_view_id(const std::string &url)
    {
        if (url.find("_view/") == 0)
            return "_view/" + url_encode(url.substr(6));
        else
            return url_encode(url);
    }

    //encodes an attachment id for use in a URL
    inline std::string url_encode_attachment_id(const std::string &url)
    {
        std::string str = url_encode(url);
        size_t idx;
        //find uppercase
        while ((idx = str.find("%2F")) != std::string::npos)
        {
            str.erase(idx+1, 2);
            str[idx] = '/';
        }
        //and lowercase
        while ((idx = str.find("%2f")) != std::string::npos)
        {
            str.erase(idx+1, 2);
            str[idx] = '/';
        }
        return str;
    }

    //generic function to decode a URL
    inline std::string url_decode(const std::string &url)
    {
        return ascii_string_tools::to_percent_decoded_copy(url);
    }

    //returns true if the given document id is reserved by CouchDB
    inline bool is_special_doc_id(const std::string &id)
    {
        return id.find('_') == 0;
    }

    typedef std::pair<std::string, std::string> query;
    typedef std::vector<query> queries;

    //adds the given query to the given URL
    inline std::string add_url_query(const std::string &url, const std::string &query)
    {
        if (url.find('?') == std::string::npos)
            return url + "?" + query;
        else
            return url + "&" + query;
    }

    //adds the given query to the given URL
    inline std::string add_url_query(const std::string &url, const query &query)
    {
        if (url.find('?') == std::string::npos)
            return url + "?" + query.first + "=" + query.second;
        else
            return url + "&" + query.first + "=" + query.second;
    }

    //adds the given key/value pair to the given URL
    inline std::string add_url_query(const std::string &url, const std::string &key, const std::string &value)
    {
        if (url.find('?') == std::string::npos)
            return url + "?" + key + "=" + value;
        else
            return url + "&" + key + "=" + value;
    }

    //adds the given queries to the given URL
    inline std::string add_url_queries(const std::string &url, const std::vector<std::string> &queries)
    {
        std::string ret(url);

        if (queries.empty())
            return ret;

        if (url.find('?') == std::string::npos)
            ret += "?" + queries[0];
        else
            ret += "&" + queries[0];

        for (size_t i = 1; i < queries.size(); ++i)
            ret += "&" + queries[i];

        return ret;
    }

    //adds the given queries to the given URL
    inline std::string add_url_queries(const std::string &url, const queries &queries)
    {
        std::string ret(url);

        if (queries.empty())
            return ret;

        if (url.find('?') == std::string::npos)
            ret += "?" + queries[0].first + "=" + queries[0].second;
        else
            ret += "&" + queries[0].first + "=" + queries[0].second;

        for (size_t i = 1; i < queries.size(); ++i)
            ret += "&" + queries[i].first + "=" + queries[i].second;

        return ret;
    }

    // Converts string to JSON value
    inline json::value string_to_json(const std::string &str)
    {
        try {return json::from_json(str);}
        catch (json::error) {return json::value();}
    }

    // Converts JSON value to string
    inline std::string json_to_string(const json::value &val)
    {
        return json::to_json(val);
    }
}

#endif // CPPCOUCH_SHARED_H

