# cppcouch
## C++11 library for interacting with CouchDB
### Overview

This is a header-only library for interacting with CouchDB synchronously in C++. To include cppcouch in your project, just copy the directory structure to your project and `#include <Couch/cppcouch.h>` in your code. However, cppcouch is *not* complete: it still needs an HTTP interface to work!

An API to build an HTTP interface on top of is outlined below, as well as in `Couch/shared.h`. Two classes, `http_url_base` and `http_client_base`, need to be inherited to provide network support. The template parameter for most of the classes in cppcouch is for an `http_client_base`-inherited type. An example network implementation based on the Poco C++ library is available in `Network/network.h`, but does not implement network timeouts. Feel free to use it if you wish.

### Implementing a URL interface

Before implementing an HTTP interface, a URL interface must be implemented. A URL implementation must inherit `http_url_base`, and overload the following members:

```c++
// Convert URL to a standard string representation of the entire URL
virtual std::string to_string() const = 0;
// Parse URL from a standard string representation
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

// Get username, password, host and port in standard URL representation
virtual std::string get_authority() const = 0; 
// Set username, password, host and port in standard URL representation
// Only the host is required to be present
virtual void set_authority(const std::string &authority) = 0;
```
    
The unmarked functions should be relatively self-explanatory.

### Implementing an HTTP interface

An HTTP interface must inherit `http_client_base`, providing as template parameters the relevant URL implementation class, the timeout duration type, and the timeout mode type. If your HTTP implementation does not require (or support) timeouts, the latter two types need not be present in the template parameter list.

In the new HTTP interface, the following `typedef` must be defined:

  - `type`: This type MUST be set to the identity type of the new HTTP interface
  
and the following two members must be overloaded:

```c++
// Allows the connection to cache responses to things that SHOULD never change for a given server, like the welcome message.
virtual bool allow_cached_responses() const = 0;

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
                        std::string &error_description) = 0;
```
                            
Note that the URLs received may be for either HTTP or HTTPS connections, and an interface should be able to handle both.

### Notes

  - The `_changes` feed interface is currently broken and needs work.
  - cppcouch is not designed to be used in an asynchronous manner. The easiest way to make it asynchronous is to operate it in a separate thread from the main program, and use cross-thread communication.

### Usage

The following is a simple interface to get the current CouchDB version from the localhost:

```c++
#include <Couch/cppcouch.h>
#include <Network/network.h>

int main()
{
    auto connection = couchdb::make_connection(couchdb::http_impl<>(), // The included HTTP interface class using Poco libraries
                                               couchdb::local(), // The local couchdb interface
                                               couchdb::user("username", "password"), // The user to authenticate with
                                               couchdb::auth_basic); // How to authenticate, in this case using HTTP Basic authentication
    
    try
    {
        std::cout << connection->get_couchdb_version() << std::endl;
    }
    catch (const couchdb::error &e)
    {
        std::cerr << "ERROR: " << e.reason() << std::endl;
    }
}
```

Please check back for more usage examples.
