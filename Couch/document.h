#ifndef CPPCOUCH_DOCUMENT_H
#define CPPCOUCH_DOCUMENT_H

#include "revision.h"
#include "attachment.h"

namespace couchdb
{
    template<typename http_client> class design_document;

    /* Document class - Operates on a single CouchDB document, either the current revision or
     * a past revision. The document can be updated, copied, or deleted, and attachments are managed here.
     *
     * If the revision of a Document object is empty, the Document object references the latest revision of
     * the CouchDB document, but with some restrictions. Deletion is not supported, and, depending on the document
     * type, other operations may not be.
     *
     * If you need the latest revision of a document, with no restrictions, use the getLatestRevision() function.
     */

    //a callback type for resolving conflicts
    //the first parameter is a list of contents of conflicting documents
    //the second parameter is a pointer to the winning document. set the value of the referenced object to a/the winning document
    typedef void (*DocumentConflictResolver)(const Json::Value & /* conflicting documents */, Json::Value * /* document to write back to couch */);

    template<typename http_client> class database;
    template<typename http_client> class locator;

    template<typename http_client>
    class document
    {
        friend class attachment<http_client>;
        friend class database<http_client>;
        friend class locator<http_client>;

        typedef communication<http_client> base;

        typedef attachment<http_client> attachment_type;

    protected:
        document(std::shared_ptr<base> _comm, const std::string &_db,
                   const std::string &_id, const std::string &_rev)
            : comm_(_comm)
            , db_(_db)
            , id_(_id)
            , revision_(_rev)
        {}

    public:
        enum type {normal, design};

        document(const document &other)
            : comm_(other.comm_)
            , db_(other.db_)
            , id_(other.id_)
            , revision_(other.revision_)
        {}
        virtual ~document() {}

        // Returns whether this document is a design document or not
        virtual type doc_type() const
        {
            return id_.find("_design/") == 0? design: normal;
        }

        bool operator==(const document &other) const {return db_ == other.db_ && id_ == other.id_ && revision_ == other.revision_;}
        bool operator!=(const document &other) const {return !(*this == other);}

        // Returns a document referencing the latest revision of this document, with a valid '_rev' value
        virtual document get_latest_revision() const
        {
            Json::Value response = comm_->get_data("/" + url_encode(db_) + "/_all_docs?key=" + url_encode("\"" + id_ + "\""));
            if (!response.isObject())
                throw error(error::document_unavailable);

            int numRows = response["total_rows"].asInt();
            const Json::Value &rows = response["rows"];

            if (numRows > 0 && rows.isArray() && rows.size())
            {
                const Json::Value &docObj = rows[0];
                if (!docObj.isObject() || !docObj["value"].isObject())
                    throw error(error::document_unavailable);

                return document(comm_, db_, id_, docObj["value"]["rev"].asString());
            }
            else
                throw error(error::document_unavailable);
        }

        // Returns the name of the database this document is in
        virtual std::string get_db_name() const {return db_;}

        // Returns the database object of this document
        virtual database<http_client> get_db() const {return database<http_client>(comm_, db_);}

        // Returns this document's id
        virtual std::string get_doc_id() const {return id_;}

        // Returns the revision of this document
        // (if empty, this document references the latest revision, but has some restrictions)
        virtual std::string get_doc_revision() const {return revision_;}

        // Returns true if the document exists, false if it does not
        virtual bool exists() const
        {
            try {comm_->get_data(get_doc_url_path(true), "HEAD");}
            catch (const error &e)
            {
                if (e.type() == error::content_not_found)
                    return false;
                throw;
            }
            return true;
        }

        // Returns true if the document is active, false if it has been deleted
        virtual bool is_deleted() const
        {
            Json::Value val;

            try {val = get_data();}
            catch (const error &e)
            {
                if (e.type() == error::document_unavailable)
                    return true;
                throw;
            }

            if (val.isObject())
                return val["_deleted"].asBool();
            return true;
        }

        // Returns all the revisions for this document
        virtual revisions get_all_revisions() const
        {
            couchdb::revisions revisions;

            Json::Value value = comm_->get_data(get_doc_url_path(false) + "?revs_info=true");
            if (!value.isObject())
                throw error(error::document_unavailable);

            const Json::Value &array = value["_revs_info"];
            if (!array.isArray())
                throw error(error::document_unavailable);

            for (auto rev: array)
            {
                if (rev.isObject())
                    revisions.push_back(revision(rev["rev"].asString(), rev["status"].asString()));
            }

            return revisions;
        }

        // Returns the body of the document with given queries
        virtual Json::Value get_data(const queries &_queries = queries()) const
        {
            Json::Value obj = comm_->get_data(add_url_queries(get_doc_url_path(true), _queries));
            if (!obj.isObject())
                throw error(error::document_unavailable);

            if (!obj.isMember("_id") && !obj.isMember("_rev") &&
                    obj.isMember("error") && obj.isMember("reason"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Document \"" + id_ + "\" not found: " + obj["reason"].asString();
#endif
                throw error(error::document_unavailable, obj["reason"].asString());
            }

            return obj;
        }

        // Returns the body of the document with given queries
        // If include_revision_in_request is false, the most up-to-date revision is used
        virtual Json::Value get_data(bool include_revision_in_request, const queries &queries = queries()) const
        {
            Json::Value obj = comm_->get_data(add_url_queries(get_doc_url_path(include_revision_in_request), queries));
            if (!obj.isObject())
                throw error(error::document_unavailable);

            if (!obj.isMember("_id") && !obj.isMember("_rev") &&
                    obj.isMember("error") && obj.isMember("reason"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Document \"" + id_ + "\" not found: " + obj["reason"].asString();
#endif
                throw error(error::document_unavailable, obj["reason"].asString());
            }

            return obj;
        }

        // Returns the body of the document with conflict resolution
        virtual Json::Value get_data_with_conflict_resolver(DocumentConflictResolver callback, const queries &_queries = queries())
        {
            queries new_queries(_queries);
            Json::Value data, docs(Json::arrayValue);
            bool found = false;

            for (query q: _queries)
                if (q.first == "conflicts")
                {
                    found = true;
                    break;
                }

            if (!found)
                new_queries.push_back(query("conflicts", "true"));

            // Get initial document, listing conflicts
            data = get_data(false, new_queries);
            if (!data.isObject())
                throw error(error::document_unavailable);

            Json::Value conflicts = data["_conflicts"];
            if (!conflicts.isArray())
                throw error(error::document_unavailable);

            conflicts.append(data["_rev"].asString());

            // Get the content of each conflict
            for (auto conflict: conflicts)
                docs.append(document(comm_, db_, id_, conflict.asString()).get_data(_queries));

            //if there are conflicts
            if (docs.size() > 1)
            {
                Json::Value result = data;
                result.removeMember("_conflicts");

                // Attempt to resolve
                callback(docs, &result);

                // Update the current document
                Json::Value request(Json::objectValue);

                /* TODO: all_or_nothing option does not exist in CouchDB 2.0.0 */
                request["all_or_nothing"] = true;

                // Update the first revision (the actual revision to save really doesn't matter,
                // so we just use the first one) and mark all others deleted
                result["_id"] = docs[0]["_id"]; // Overwrite immutable fields to correct data
                result["_rev"] = docs[0]["_rev"]; // Ditto
                docs[0] = result;
                for (auto it = docs.begin(); it != docs.end(); ++it)
                    (*it)["_deleted"] = true;
                docs[0].removeMember("_deleted");

                database<http_client>(comm_, db_).bulk_update_raw(docs, request);

                return result;
            }

            // If there are no conflicts, just return the document
            data.removeMember("_conflicts");
            return data;
        }

        // Sets the body of the document with given fields,
        // and updates this document to point to the new revision
        // IMPORTANT: No other document objects will be updated to the new revision
        virtual document &set_data(Json::Value data)
        {
            Json::Value response = comm_->get_data(get_doc_url_path(true));
            if (!response.isObject())
                throw error(error::document_unavailable);

            if (!data.isObject())
                data = Json::Value(Json::objectValue);

            for (auto it = response.begin(); it != response.end(); ++it)
            {
                std::string key = it.key().asString();
                if ((key == "_id" || key == "_rev") || // Reserved field? These cannot be modified, so we need to make sure they don't change
                    (key.find('_') == 0 && !data.isMember(key))) // Non-included reserved field, we should include it (reserved fields are those beginning with an underscore '_')
                    data[key] = response[key];
            }

            response = comm_->get_data(get_doc_url_path(false), "PUT", json_to_string(data));
            if (!response.isObject())
                throw error(error::document_unavailable);

            if (!response.isMember("id"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Document could not be created: " + response["reason"].asString();
#endif
                throw error(error::document_unavailable, response["reason"].asString());
            }

            revision_ = response["rev"].asString();

            return *this;
        }

        // Adds an attachment with given attachment id, content-type, and data
        // The attachment id must not be empty
        virtual attachment_type create_attachment(const std::string &attachmentId, const std::string &contentType, const std::string &data)
        {
            if (attachmentId.empty())
                throw error(error::attachment_not_creatable, "No attachment identifier specified");

            std::string url = get_doc_url_path(false) + "/" + url_encode_attachment_id(attachmentId);
            if (revision_.size() > 0)
                url += "?rev=" + url_encode(revision_);

            typename base::header_map headers;
            headers["Content-Type"] = contentType;

            Json::Value response = comm_->get_data(url, headers, "PUT", data);
            if (!response.isObject())
                throw error(error::document_unavailable);

            if (response.isMember("error") && response.isMember("reason"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Could not create attachment \"" + attachmentId + "\": " + response["reason"].asString();
#endif
                throw error(error::attachment_not_creatable, response["reason"].asString());
            }

            revision_ = response["rev"].asString();

            if (!response["ok"].asBool())
                throw error(error::attachment_not_creatable);

            return attachment_type(comm_, db_, id_, attachmentId, revision_, contentType, data.size());
        }

        // Ensures an attachment exists and returns it
        virtual attachment_type ensure_attachment_exists(const std::string &attachmentId)
        {
            try {return get_attachment(attachmentId);}
            catch (const error &e) {if (e.type() != error::content_not_found && e.type() != error::attachment_unavailable) throw;}

            return this->create_attachment(attachmentId, "text/plain", "");
        }

        // Creates an attachment with given body and content type
        virtual attachment_type ensure_attachment_exists(const std::string &attachmentId,
                                                         const std::string &contentType,
                                                         const std::string &data)
        {
            try {return get_attachment(attachmentId).set_data(data, contentType);}
            catch (const error &e) {if (e.type() != error::content_not_found && e.type() != error::attachment_unavailable) throw;}

            return this->create_attachment(attachmentId, contentType, data);
        }

        // Returns the attachment with the given id if possible
        virtual attachment_type get_attachment(const std::string &attachmentId) const
        {
            Json::Value response = get_data();
            if (!response.isObject())
                throw error(error::attachment_unavailable);

            if (!response.isMember("_attachments"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "No Attachments";
#endif
                throw error(error::attachment_unavailable, "The document has no attachments");
            }

            response = response["_attachments"];
            if (!response.isObject() || !response.isMember(attachmentId))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "No attachment found with id \"" + attachmentId + "\"";
#endif
                throw error(error::attachment_unavailable);
            }

            response = response[attachmentId];
            if (!response.isObject())
                throw error(error::attachment_unavailable);

            return attachment_type(comm_,
                                   db_,
                                   id_,
                                   attachmentId,
                                   revision_,
                                   response["content_type"].asString(),
                                   response.get("length", -1).asInt64());
        }

        // Returns a list of all attachments for this document
        virtual std::vector<attachment_type> list_all_attachments() const
        {
            Json::Value response = get_data();
            if (!response.isObject())
                throw error(error::document_unavailable);

            std::vector<attachment_type> vAttachments;
            if (!response.isMember("_attachments")) // No attachments with document?
                return vAttachments;

            const Json::Value &attachments = response["_attachments"];
            if (!response.isObject())
                throw error(error::attachment_unavailable);

            for (auto it = attachments.begin(); it != attachments.end(); ++it)
            {
                if (it->isObject())
                    vAttachments.push_back(attachment_type(comm_, db_, id_, it.key().asString(), revision_,
                                                   (*it)["content_type"].asString(),
                                                   it->get("length", -1).asInt64()));
            }

            return vAttachments;
        }

        // Deletes the attachment with the given id if possible
        virtual document &remove_attachment(const std::string &attachmentId)
        {
            if (attachmentId.empty())
                throw error(error::attachment_not_deletable);

            std::string url = get_doc_url_path(false) + "/" + url_encode_attachment_id(attachmentId);
            if (revision_.size() > 0)
                url += "?rev=" + url_encode(revision_);

            Json::Value response = comm_->get_data(url, "DELETE");
            if (!response.isObject())
                throw error(error::attachment_not_deletable);

            if (response.isMember("error") && response.isMember("reason"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Could not delete attachment \"" + attachmentId + "\": " + response["reason"].asString();
#endif
                throw error(error::attachment_not_deletable, response["reason"].asString());
            }

            revision_ = response["rev"].asString();

            if (!response["ok"].asBool())
                throw error(error::attachment_not_deletable);

            return *this;
        }

        // Ensures the attachment with given name is deleted
        virtual document &ensure_attachment_is_deleted(const std::string &attachmentId)
        {
            try {return remove_attachment(attachmentId);}
            catch (error &e) {if (e.type() != error::content_not_found && e.type() != error::attachment_not_deletable) throw;}

            return *this;
        }

        // Copies this document to another id and optional revision in the same database
        // and returns the new document
        virtual document copy(const std::string &targetId, const std::string &targetRev = "")
        {
            typename base::header_map headers;
            if (targetRev.size() > 0)
                headers["Destination"] = url_encode_doc_id(targetId) + "?rev=" + url_encode(targetRev);
            else
                headers["Destination"] = url_encode_doc_id(targetId);

            Json::Value response = comm_->get_data(get_doc_url_path(true), headers, "COPY");
            if (!response.isObject())
                throw error(error::document_not_creatable);

            if (response.isMember("error") && response.isMember("reason"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Could not copy document \"" + id_ + "\" to \"" + targetId + "\": " + response["reason"].asString();
#endif
                throw error(error::document_not_creatable, response["reason"].asString());
            }

            std::string newId = url_encode_doc_id(targetId);
            if (response.isMember("id"))
                newId = response["id"].asString();

            return document(comm_, db_, newId, response["rev"].asString());
        }

        // Deletes this document
        virtual document &remove()
        {
            Json::Value response = comm_->get_data(get_doc_url_path(true), "DELETE");
            if (!response.isObject() || !response["ok"].asBool())
                throw error(error::document_not_deletable);

            return *this;
        }

        // Returns the URL of the CouchDB server
        virtual std::string get_server_url() const {return comm_->get_server_url();}

        // Returns the URL of the database this document is in
        virtual std::string get_db_url() const {return comm_->get_server_url() + "/" + url_encode(db_);}

        // Returns the URL of this document, optionally including the revision
        virtual std::string get_doc_url(bool withRevision = true) const {return get_db_url() + "/" + get_doc_id_and_revision_as_url(withRevision);}

        // Returns the URL of this document, starting from the id, optionally including the revision
        virtual std::string get_doc_id_and_revision_as_url(bool withRevision = true) const
        {
            std::string url = url_encode_doc_id(id_);
            if (revision_.size() > 0 && withRevision)
                url += "?rev=" + url_encode(revision_);
            return url;
        }

    protected:
        // Returns the path to the document, not including the scheme, host, and port
        virtual std::string get_doc_url_path(bool withRevision) const
        {
            std::string url = "/" + url_encode(db_) + "/" + url_encode_doc_id(id_);
            if (withRevision && revision_.size() > 0)
                url += "?rev=" + url_encode(revision_);
            return url;
        }

        std::shared_ptr<base> comm_;
        std::string db_;
        std::string id_;
        std::string revision_;
    };
}

#endif // CPPCOUCH_DOCUMENT_H
