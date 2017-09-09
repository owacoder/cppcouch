# cppcouch
## C++11 wrapper library for CouchDB
### Overview

This is a header-only library for interacting with CouchDB synchronously in C++. To include cppcouch in your project, just copy the directory structure to your project and `#include <Couch/cppcouch.h>` in your code. However, cppcouch is *not* complete: it still needs an HTTP interface to work!

An API to build an HTTP interface on top of is outlined in `shared.h`. Two classes, `http_url_base` and `http_client_base`, need to be inherited to provide network support. The template parameter for most of the classes in cppcouch is for a `http_client_base`-inherited type. An example network implementation based on the Poco C++ library is available in `Network/network.h`, but does not implement network timeouts. Feel free to use it if you wish.

### Notes

  - The `_changes` feed interface is currently broken and needs work.
  - cppcouch is not designed to be used in an asynchronous manner. The easiest way to make it asynchronous is to operate it in a separate thread from the main program, and use cross-thread communication.

### Usage

Please check back for usage examples.
