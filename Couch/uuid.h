#ifndef UUID_H
#define UUID_H

#include "communication.h"
#include "connection.h"

namespace couchdb
{
    /* Uuid class - This class is a simple wrapper for the CouchDB UUID interface. Call
     * generate() to pull uuids from CouchDB, and next() to acquire the next UUID(s).
     */

    template<typename http_client>
    class uuid
    {
        uuid(const uuid &) {}
        void operator=(const uuid &) {}

        typedef connection<http_client> connection_type;

    public:
        uuid(std::shared_ptr<connection_type> c) : conn(c) {}

        size_t available() const {return uuids.size();}

        uuid &generate(size_t count = 1)
        {
            if (count >= uuids.size())
            {
                count -= uuids.size();
                uuids += conn->get_uuids(std::max(count, static_cast<size_t>(10)));
            }

            return *this;
        }

        std::string next()
        {
            std::string n;
            if (uuids.empty())
                return generate().next();
            n = uuids.back();
            uuids.pop_back();
            return n;
        }

        std::vector<std::string> next(size_t count)
        {
            std::vector<std::string> result;
            if (uuids.size() < count)
                return result;

            result.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                result.push_back(uuids.back());
                uuids.pop_back();
            }
            return result;
        }

    private:
        std::shared_ptr<connection_type> conn;
        std::vector<std::string> uuids;
    };
}

#endif // UUID_H

