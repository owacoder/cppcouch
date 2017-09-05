#ifndef CPPCOUCH_REPLICATIONDOCUMENT_H
#define CPPCOUCH_REPLICATIONDOCUMENT_H

#include "document.h"

namespace couchdb
{
    /* Replication class - All operations pertaining to a CouchDB '/_replicator' based persistent replication
     * are encapsulated by this class.
     *
     * A replication may only be created by a Database, ensuring that the same Communication class is used
     * for all subclasses.
     *
     * UNTESTED
     */

    template<typename http_client> class database;
    template<typename http_client> class locator;

    template<typename http_client>
    class replication_document : public document<http_client>
    {
        friend class database<http_client>;
        friend class locator<http_client>;

    protected:
        replication_document(std::shared_ptr<communication<http_client>> _comm, const std::string &_id, const std::string &_rev)
            : document<http_client>(_comm, "_replicator", _id, _rev)
        {}

    public:
        virtual ~replication_document() {}

        bool operator==(const replication_document &other) const {return document<http_client>::operator==(other);}
        bool operator!=(const replication_document &other) const {return !(*this == other);}

        // Deletes this document, cancelling this replication
        virtual replication_document &remove()
        {
            this->get_latest_revision().remove();
            return *this;
        }
    };
}

#endif // CPPCOUCH_REPLICATIONDOCUMENT_H
