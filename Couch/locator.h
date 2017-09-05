#ifndef CPPCOUCH_LOCATOR_H
#define CPPCOUCH_LOCATOR_H

#include "connection.h"

namespace couchdb
{
    /* Locator class - This class parses a URL to find what object it points to. The resulting object
     * can then be accessed without the need for a Database object, then a Document object, etc.
     *
     * Each referenced object can be retrieved with one function call:
     *     getXXX(), where XXX is the name of the object being pointed to.
     *
     * Also, any object above the referenced object may be safely referenced with
     * the respective getXXX() call. (i.e. if a document is referenced, the database that contains it
     * may be retrieved as well)
     *
     * Note: Specialized views such as {db}/_all_docs, etc. will not and cannot be referenced properly.
     * The only CouchDB objects which may be referenced are a Database, a Document, a DesignDocument,
     * an Attachment of a Document or DesignDocument, or a View within a DesignDocument.
     */

    template<typename http_client>
    class locator
    {
        typedef connection<http_client> base;
        typedef couchdb::database<http_client> database_type;
        typedef couchdb::document<http_client> document_type;
        typedef couchdb::design_document<http_client> design_document_type;
        typedef couchdb::replication_document<http_client> replication_document_type;
        typedef couchdb::attachment<http_client> attachment_type;
        typedef couchdb::view<http_client> view_type;

        locator(std::shared_ptr<base> connection,
                const std::string &baseURL,
                const std::string &db,
                const std::string &docId = "",
                const std::string &doc = "",
                const std::string &rev = "")
            : c(connection)
            , baseURL(baseURL)
            , db(db)
            , docId(docId)
            , id(doc)
            , revision(rev)
        {
            if (db == "_replicator")
                type_ = replication_document;
            else if (doc.size() > 0)
            {
                if (doc.find("_view/") == 0 && docId.find("_design/") == 0)
                    type_ = view;
                else
                    type_ = attachment;
            }
            else if (docId.size() > 0)
            {
                if (docId.find("_design/") == 0)
                    type_ = design_document;
                else
                    type_ = document;
            }
            else
                type_ = database;
        }

    public:
        enum type
        {
            database,
            document,
            design_document,
            replication_document,
            attachment,
            view
        };

        static locator fromURL(std::shared_ptr<base> connection, const std::string &url)
        {
#ifdef CPPCOUCH_DEBUG
            std::cout << "original URL:" << url;
#endif

            std::string base, db, docId, id, rev;

            if (url.empty())
                return locator(connection, base, db, docId, id, rev);

            typename communication<http_client>::http_client_url_t http_client_url;
            http_client_url.from_string(url);

            base = http_client_url.get_scheme() + "://" + http_client_url.get_authority();
            std::vector<std::string> split = ascii_string_tools::split(http_client_url.get_path(), '/');

            if (http_client_url.get_query().size() > 0)
            {
                // The only thing in a query should be the revision
                rev = http_client_url.get_query();
                if (rev.find("rev=") != std::string::npos)
                {
                    rev = rev.substr(rev.find("rev=") + 4);
                    if (rev.find('&') != std::string::npos)
                        rev = rev.substr(0, rev.find('&'));
                }
                else
                    rev.clear();
            }

            for (auto i: split)
                std::cout << i << std::endl;

            if (split.size() >= 3 && split[0] == "shards") // Shards database
            {
                db = split[0] + "/" + split[1] + "/" + split[2];
                split.erase(split.begin(), split.begin() + 3);
            }

            if (split.size() == 1) // Database
                db = split[0];
            else if (split.size() == 2) // Document
            {
                db = split[0];
                docId = split[1];
            }
            else if (split.size() > 2) // Attachment, view, or design document
            {
                db = split[0];
                if (split[1] == "_design")
                {
                    docId = split[1] + "/" + split[2];

                    for (size_t i = 3; i < split.size() - 1; ++i)
                        id += split[i] + "/";

                    if (split.size() > 3)
                        id += split.back();
                }
                else
                {
                    docId = split[1];
                    for (size_t i = 2; i < split.size() - 1; ++i)
                        id += split[i] + "/";
                    id += split.back();
                }
            }

            return locator(connection, url_decode(base), url_decode(db), url_decode(docId), url_decode(id), url_decode(rev));
        }

        locator(const locator &other)
            : c(other.c)
            , baseURL(other.baseURL)
            , db(other.db)
            , docId(other.docId)
            , id(other.id)
            , revision(other.revision)
        {

        }

        locator &operator=(const locator &other)
        {
            baseURL = other.baseURL;
            db = other.db;
            docId = other.docId;
            id = other.id;
            revision = other.revision;
            return *this;
        }

        // Returns the type of object this locator points to
        type getType() const {return type_;}

        // Returns true if this locator points to a database
        bool isDatabase() const {return type_ == database;}

        // Returns true if this locator points to a document (of any type)
        bool isDocument() const {return type_ == document || type_ == design_document;}

        // Returns true if this locator points to a design document
        bool isDesignDocument() const {return type_ == design_document;}

        // Returns true if this locator points to a replication document
        bool isReplicationDocument() const {return type_ == replication_document;}

        // Returns true if this locator points to an attachment
        bool isAttachment() const {return type_ == attachment;}

        // Returns true if this locator points to a view
        bool isView() const {return type_ == view;}

        // Returns the database referenced, if possible
        database_type getDatabase() const
        {
            return c->get_db(db);
        }

        // Returns the document referenced, if possible
        document_type getDocument() const
        {
            return c->get_db(db).get_doc(docId, revision);
        }

        // Returns the design document referenced, if possible
        design_document_type getDesignDocument() const
        {
            return c->get_db(db).get_design_doc(docId, revision);
        }

        // Returns the replication document referenced, if possible
        replication_document_type getReplicationDocument() const
        {
            return c->get_db(db).get_doc(docId, revision);
        }

        // Returns the attachment referenced, if possible
        attachment_type getAttachment() const
        {
            return c->get_db(db).get_doc(docId, revision).get_attachment(id);
        }

        // Returns the view referenced, if possible
        view_type getView() const
        {
            std::vector<view_type> views = c->get_db(db).get_design_doc(docId, revision).getViews();

            for (view_type v: views)
                if (v.get_view_id() == id)
                    return view_type(c->comm, db, docId, id, revision);

            throw error(error::view_unavailable);
        }

        // Returns the name of the database being referenced
        std::string getDatabaseName() const {return db;}

        // Returns the id of the document being referenced
        std::string getDocumentID() const {return docId;}

        // Returns the id of the attachment being referenced
        std::string getAttachmentID() const {return id;}

        // Returns the revision of the document being referenced
        std::string getDocumentRevision() const {return revision;}

        // Returns the URL of the CouchDB server
        std::string getBaseURL() const {return baseURL;}

        // Returns the URL of the database being referenced
        std::string getDatabaseURL() const {return baseURL + "/" + url_encode(db);}

        // Returns the URL of the document being referenced
        std::string getDocumentURL() const {return getDatabaseURL() + "/" + getDocumentNameURL();}

        // Returns the URL of the attachment being referenced
        std::string getAttachmentURL() const {return getDatabaseURL() + "/" + getAttachmentNameURL();}

        // Returns the URL of the document being referenced, starting from the document id
        std::string getDocumentNameURL() const
        {
            std::string url = url_encode(docId);
            if (revision.size() > 0)
                url += "?rev=" + url_encode(revision);
            return url;
        }

        // Returns the URL of the attachment being referenced, starting from the attachment id
        std::string getAttachmentNameURL() const
        {
            std::string url = url_encode(docId) + "/" + url_encode(id);
            if (revision.size() > 0)
                url += "?rev=" + url_encode(revision);
            return url;
        }

    private:
        std::shared_ptr<base> c;
        std::string baseURL;
        std::string db;
        std::string docId;
        std::string id;
        std::string revision;
        type type_;
    };
}

#endif // CPPCOUCH_LOCATOR_H
