#ifndef CPPCOUCH_REVISION_H
#define CPPCOUCH_REVISION_H

#include <string>
#include <vector>

namespace couchdb
{
    /* Revision class - This class stores a single revision/status pair for a document
     */

    class revision
    {
    public:
        revision(const std::string &version, const std::string &status) : version_(version), status_(status) {}

        std::string version() const {return version_;}
        std::string status() const {return status_;}

    private:
        std::string version_;
        std::string status_;
    };

    typedef std::vector<revision> revisions;
}

#endif // CPPCOUCH_REVISION_H
