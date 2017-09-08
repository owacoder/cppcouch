#ifndef CPPCOUCH_DATABASE_H
#define CPPCOUCH_DATABASE_H

#include "document.h"
#include "replication.h"
#include "Design/designdocument.h"

namespace couchdb
{
    /* Database class - All operations pertaining to a CouchDB database are encapsulated by this class.
     *
     * A database may only be created by a Connection, ensuring that the same Communication class is used
     * for all subclasses.
     */

    template<typename http_client> class connection;

    template<typename http_client>
    class database
    {
        friend class document<http_client>;
        friend class connection<http_client>;
        friend class locator<http_client>;

        typedef communication<http_client> base;
        typedef document<http_client> document_type;
        typedef replication_document<http_client> replication_document_type;
        typedef design_document<http_client> design_document_type;
        typedef attachment<http_client> attachment_type;

    protected:
        database(std::shared_ptr<base> _comm, const std::string &name)
            : comm_(_comm)
            , name_(name)
        {}

    public:
        // Returns true if the given name is a valid database name
        static bool is_valid_name(const std::string &b)
        {
            if (b.empty() || !islower(static_cast<unsigned char>(b[0]))) // Empty name or does not start with lowercase letter
                return false;

            for (size_t i = 1; i < b.size(); ++i)
            {
                char chr = b[i];
                if (!islower(static_cast<unsigned char>(chr)) &&
                        !isdigit(static_cast<unsigned char>(chr)) &&
                        chr != '_' &&
                        chr != '$' &&
                        chr != '(' &&
                        chr != ')' &&
                        chr != '+' &&
                        chr != '-' &&
                        chr != '/')
                    return false;
            }

            return true;
        }
        virtual ~database() {}

        // Returns Error::NoError if the database exists,
        // otherwise Error::ContentNotAvailable will be returned
        virtual bool exists() const
        {
            try {comm_->get_data("/" + url_encode(name_), "HEAD");}
            catch (const error &e)
            {
                if (e.type() == error::content_not_found)
                    return false;
                throw;
            }
            return true;
        }

        // Returns the connection object of this database object
        virtual connection<http_client> get_connection() {return connection<http_client>(comm_);}

        // Replicates this database to the target with given options, and returns CouchDB's response
        // Target must be an unencoded URL
        virtual json::value replicate_to(const std::string &targetURL, const json::value &options = json::object_t() /* Object */) const
        {
            return replicate_remote(comm_->get_server_url() + "/" + url_encode(name_), targetURL, options);
        }

        // Replicates source to this database with given options, and returns CouchDB's response
        // Source must be an unencoded URL
        virtual json::value replicate_from(const std::string &sourceURL, const json::value &options = json::object_t() /* Object */) const
        {
            return replicate_remote(sourceURL, comm_->get_server_url() + "/" + url_encode(name_), options);
        }

        // Replicates source to target with given options, and returns CouchDB's response
        // Source or target must be unencoded URLs
        virtual json::value replicate_remote(const std::string &sourceURL, const std::string &targetURL, const json::value &options = json::object_t() /* Object */) const
        {
            json::value mOptions(options);
            std::string source = sourceURL;
            std::string target = targetURL;

            if (!mOptions.is_object())
                mOptions = json::value();

            mOptions["source"] = source;
            mOptions["target"] = target;

            std::string data = json_to_string(mOptions);
#ifdef CPPCOUCH_DEBUG
            std::cout << "replication data: " << data;
#endif
            return comm_->get_data("/_replicate", "POST", data);
        }

        // Cancels a replication (using the '_replicate' endpoint) and returns CouchDB's response
        // The JSON object parameter should be the response from one of the 'replicateX' functions above
        virtual json::value cancel_replication(const json::value &replication /* Object */)
        {
            json::value rep(replication);

            if (!rep.is_object())
                rep = json::value();

            rep["cancel"] = true;
            return comm_->get_data("/_replicate", "POST", json_to_string(rep));
        }

        // Replicates this database target with given options, and returns CouchDB's response
        // Target must be an unencoded URL
        // If id is empty, an automatically generated one will be given to the document
        virtual replication_document_type create_replication_to(const std::string &targetURL, const std::string &replicateDocId = "", const json::value &options = json::object_t() /* Object */) const
        {
            return create_replication_remote(comm_->get_server_url() + "/" + url_encode(name_), targetURL, replicateDocId, options);
        }

        // Replicates source to this database with given options, and returns CouchDB's response
        // Source must be an unencoded URL
        // If id is empty, an automatically generated one will be given to the document
        virtual replication_document_type create_replication_from(const std::string &sourceURL, const std::string &replicateDocId = "", const json::value &options = json::object_t() /* Object */) const
        {
            return create_replication_remote(sourceURL, comm_->get_server_url() + "/" + url_encode(name_), replicateDocId, options);
        }

        // Replicates source to target with given options, and returns CouchDB's response
        // Source or target must be unencoded URLs
        // If id is empty, an automatically generated one will be given to the document
        virtual replication_document_type create_replication_remote(const std::string &sourceURL, const std::string &targetURL, const std::string &replicateDocId = "", const json::value &options = json::object_t() /* Object */) const
        {
            std::string url("/_replicator/");
            std::string method("POST");
            json::value mOptions(options);

            std::string source = sourceURL;
            std::string target = targetURL;

            if (!mOptions.is_object())
                mOptions = json::value();

            mOptions["source"] = source;
            mOptions["target"] = target;

            if (replicateDocId.size() > 0)
            {
                url += "/" + url_encode(replicateDocId);
                method = "PUT";
            }

            json::value response = comm_->get_data(url, method, json_to_string(mOptions));
            if (!response.is_object())
                throw error(error::document_not_creatable);

            if (response.is_member("error"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Replication document could not be created: " + response["reason"].get_string();
#endif
                throw error(error::document_not_creatable, response["reason"].get_string());
            }

            return replication_document_type(comm_, response["id"].get_string(), response["rev"].get_string());
        }

        // A raw '/_bulk_docs' api of the current database
        // Returns the response from CouchDB (which should be an array)
        virtual json::value bulk_update_raw(const json::value &docs /* Array */, const json::value &request = json::object_t() /* Object */)
        {
            json::value obj(request);

            if (!obj.is_object())
                obj = json::object_t();
            obj["docs"] = docs;

            std::string doc_data = json_to_string(obj);

            json::value response = comm_->get_data("/" + url_encode(get_db_name()) + "/_bulk_docs", "POST", doc_data);
            if (!response.is_array())
                return response;

            for (auto item: response.get_array())
            {
                if (item.is_object() && !item["ok"].get_bool())
                    throw error(item["error"] == "conflict"? error::document_not_creatable: error::forbidden);
            }

            return response;
        }

        // Inserts several documents at one time in the current database
        // Returns the response from CouchDB (which should be an array)
        virtual json::value bulk_insert(json::value docs /* Array */, const json::value &request = json::object_t() /* Object */)
        {
            if (docs.is_array())
                for (auto it = docs.get_array().begin(); it != docs.get_array().end(); ++it)
                    if (it->is_object())
                        it->erase("_rev");

            return bulk_update_raw(docs, request);
        }

        // Deletes several documents at one time in the current database
        // Returns the response from CouchDB (which should be an array)
        virtual json::value bulk_delete(const std::vector<document_type> &docs, const json::value &request = json::object_t() /* Object */)
        {
            json::value arr;

            for (auto d: docs)
            {
                json::value obj;
                obj["_id"] = d.get_doc_id();
                obj["_rev"] = d.get_doc_revision();
                obj["_deleted"] = true;
                arr.push_back(obj);
            }

            return bulk_update_raw(arr, request);
        }

        // Compacts the database manually if possible
        virtual database &compact()
        {
            json::value response = comm_->get_data("/" + url_encode(name_) + "/_compact", "POST");
            if (!response.is_object())
                throw error(error::database_unavailable);

            if (response.is_member("error"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Compaction of database " << name_ << " failed: " << response["reason"].get_string();
#endif
                throw error(error::database_unavailable, response["reason"].get_string());
            }

            if (!response["ok"].get_bool())
                throw error(error::database_unavailable);

            return *this;
        }

        // Returns the unencoded name of the database
        virtual std::string get_db_name() const {return name_;}

        // Returns CouchDB's information about the database
        virtual json::value get_info() const
        {
            json::value response = comm_->get_data("/" + url_encode(name_));
            if (!response.is_object())
                throw error(error::database_unavailable);

            if (!response.is_member("db_name"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Database \"" + name_ + "\" not found.";
#endif
                throw error(error::database_unavailable);
            }

            return response;
        }

        // Lists all normal documents (excludes design documents)
        virtual std::vector<document_type> list_docs()
        {
            json::value response = comm_->get_data("/" + url_encode(name_) + "/_all_docs");
            if (!response.is_object())
                throw error(error::database_unavailable);

            std::vector<document_type> docs;

            if (response["total_rows"].get_int() > 0)
            {
                const json::value &rows = response["rows"];
                if (!rows.is_array())
                    throw error(error::database_unavailable);

                for (auto row: rows.get_array())
                {
                    if (!row.is_object())
                        throw error(error::database_unavailable);

                    if (row["id"].get_string().find("_design/") != 0) // Ignore design documents
                    {
                        const json::value &value = row["value"];
                        if (!value.is_object())
                            throw error(error::database_unavailable);

                        docs.push_back(document_type(comm_, name_, row["id"].get_string(), value["rev"].get_string()));
                    }
                }
            }

            return docs;
        }

        // Lists all documents, normal or design
        virtual std::vector<document_type> list_all_docs()
        {
            json::value response = comm_->get_data("/" + url_encode(name_) + "/_all_docs");
            if (!response.is_object())
                throw error(error::database_unavailable);

            std::vector<document_type> docs;

            if (response["total_rows"].get_int() > 0)
            {
                const json::value &rows = response["rows"];
                if (!rows.is_array())
                    throw error(error::database_unavailable);

                for (auto row: rows.get_array())
                {
                    if (!row.is_object())
                        throw error(error::database_unavailable);

                    const json::value &value = row["value"];
                    if (!value.is_object())
                        throw error(error::database_unavailable);

                    docs.push_back(document_type(comm_, name_, row["id"].get_string(), value["rev"].get_string()));
                }
            }

            return docs;
        }

        // Lists all design documents
        virtual std::vector<design_document_type> list_design_docs()
        {
            json::value response = comm_->get_data("/" + url_encode(name_) + "/_all_docs");
            if (!response.is_object())
                throw error(error::database_unavailable);

            std::vector<design_document_type> docs;

            if (response["total_rows"].get_int() > 0)
            {
                const json::value &rows = response["rows"];
                if (!rows.is_array())
                    throw error(error::database_unavailable);

                for (auto row: rows.get_array())
                {
                    if (!row.is_object())
                        throw error(error::database_unavailable);

                    if (row["id"].get_string().find("_design/") == 0) // Only allow design documents
                    {
                        const json::value &value = row["value"];
                        if (!value.is_object())
                            throw error(error::database_unavailable);

                        docs.push_back(design_document_type(comm_, name_, row["id"].get_string(), value["rev"].get_string()));
                    }
                }
            }

            return docs;
        }

        // Returns a document with given id, and optional revision
        virtual document_type get_doc(const std::string &id, const std::string &rev = "")
        {
            std::string url = "/" + url_encode(name_) + "/" + url_encode_doc_id(id);
            if (rev.size() > 0)
                url += "?rev=" + url_encode(rev);

            json::value response = comm_->get_data(url);
            if (!response.is_object())
                throw error(error::document_unavailable);

            if (!response.is_member("_id"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Document " + id + " (v" + rev + ") not found: " + response["reason"].get_string();
#endif
                throw error(error::database_unavailable, response["reason"].get_string());
            }

            return document_type(comm_, name_, response["_id"].get_string(), response["_rev"].get_string());
        }

        // Creates a document with given body
        // If id is empty, an automatically generated id will be given to the document
        virtual document_type create_doc(const json::value &data /* Object */, const std::string &id = "")
        {
            return create_doc(data, std::vector<attachment_type>(), id);
        }

        // Creates a document with given body and attachments
        // If id is empty, an automatically generated id will be given to the document
        virtual document_type create_doc(json::value data /* Object */, const std::vector<attachment_type> &attachments,
                                const std::string &id = "")
        {
            if (attachments.size() > 0)
            {
                json::value attachmentObj;

                for (attachment_type item: attachments)
                {
                    json::value attachmentData;

                    attachmentData["data"] = item.get_data();
                    attachmentData["content_type"] = item.get_content_type();
                    attachmentObj[item.get_doc_id()] = attachmentData;
                }

                data["_attachments"] = attachmentObj;
            }

            std::string method, url;

            if (id.size() > 0)
            {
                url = "/" + url_encode(name_) + "/" + url_encode(id);
                method = "PUT";
            }
            else
            {
                url = "/" + url_encode(name_) + "/";
                method = "POST";
            }

            json::value response = comm_->get_data(url, method, json_to_string(data));
            if (!response.is_object())
                throw error(error::document_not_creatable);

            if (!response.is_member("id"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Document could not be created: " + response["reason"].get_string();
#endif
                throw error(error::document_not_creatable, response["reason"].get_string());
            }

            return document_type(comm_, name_, response["id"].get_string(), response["rev"].get_string());
        }

        // Ensures a document exists and returns it
        virtual document_type ensure_doc_exists(const std::string &id)
        {
            // This is specifically ordered to attempt to get the document first because
            // trying to create the document with its ID already in existence will not cause an
            // error in CouchDB v1.x.x
            try {return get_doc(id);}
            catch (const error &e) {if (e.type() != error::content_not_found && e.type() != error::document_unavailable) throw;}

            return create_doc(json::object_t(), std::vector<attachment_type>(), id);
        }

        // Creates a document with given body and attachments
        virtual document_type ensure_doc_exists(const std::string &id, json::value data /* Object */)
        {
            // This is specifically ordered to attempt to get the document first because
            // trying to create the document with its ID already in existence will not cause an
            // error in CouchDB v1.x.x
            try {return get_doc(id).set_data(data);}
            catch (const error &e) {if (e.type() != error::content_not_found && e.type() != error::document_unavailable) throw;}

            return create_doc(json::object_t(), std::vector<attachment_type>(), id);
        }

        // Ensures the document with given name is deleted
        virtual database &ensure_doc_is_deleted(const std::string &docId, const std::string &revision = std::string())
        {
            try {get_doc(docId, revision).remove();}
            catch (error &e) {if (e.type() != error::content_not_found && e.type() != error::document_not_deletable) throw;}

            return *this;
        }

        // Returns a design document with given id, and optional revision
        virtual design_document_type get_design_doc(const std::string &id, const std::string &rev = "")
        {
            std::string url = "/" + url_encode(name_) + "/" + url_encode_doc_id(id);
            if (rev.size() > 0)
                url += "?rev=" + url_encode(rev);

            json::value response = comm_->get_data(url);
            if (!response.is_object())
                throw error(error::document_unavailable);

            if (!response.is_member("_id"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Design document " + id + " (v" + rev + ") not found: " + response["error"].get_string();
#endif
                throw error(error::document_unavailable, response["reason"].get_string());
            }

            return design_document_type(comm_, name_, response["_id"].get_string(), response["_rev"].get_string());
        }

        // Creates a design document with given body
        // If id is empty, an automatically generated id will be given to the document
        virtual design_document_type create_design_doc(const json::value &data /* Object */, const std::string &id = "")
        {
            return create_design_doc(data, std::vector<attachment_type>(), id);
        }

        // Creates a design document with given body and attachments
        // If id is empty, an automatically generated id will be given to the document
        virtual design_document_type create_design_doc(json::value data /* Object */, const std::vector<attachment_type> &attachments,
                                const std::string &id = "")
        {
            if (attachments.size() > 0)
            {
                json::value attachmentObj;

                for (attachment_type item: attachments)
                {
                    json::value attachmentData;

                    attachmentData["data"] = item.get_data();
                    attachmentData["content_type"] = item.get_content_type();
                    attachmentObj[item.get_doc_id()] = attachmentData;
                }

                data["_attachments"] = attachmentObj;
            }

            std::string method, url;

            if (id.size() > 0)
            {
                url = "/" + url_encode(name_) + "/" + url_encode(id);
                method = "PUT";
            }
            else
            {
                url = "/" + url_encode(name_) + "/";
                method = "POST";
            }

            json::value response = comm_->get_data(url, method, json_to_string(data));
            if (!response.is_object())
                throw error(error::document_not_creatable);

            if (!response.is_member("id"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Design document could not be created: " + response["reason"].get_string();
#endif
                throw error(error::document_not_creatable, response["reason"].get_string());
            }

            return design_document_type(comm_, name_, response["id"].get_string(), response["rev"].get_string());
        }

        // Deletes this database
        // NOTE: this is an IRREVERSABLE operation!
        virtual database &remove()
        {
            json::value response = comm_->get_data("/" + url_encode(name_), "DELETE");
            if (!response.is_object())
                throw error(error::database_not_deletable);

            if (response.is_member("error"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Unable to delete database \"" + name_ + "\": " + response["reason"].get_string();
#endif
                throw error(error::database_not_deletable, response["reason"].get_string());
            }

            if (!response["ok"].get_bool())
                throw error(error::database_not_deletable);

            return *this;
        }

        // Returns the URL of the CouchDB server
        virtual std::string get_server_url() const {return comm_->get_server_url();}

        // Returns the URL of this database
        virtual std::string get_db_url() const {return comm_->get_server_url() + "/" + url_encode(name_);}

    protected:
        std::shared_ptr<base> comm_;
        std::string name_;
    };
}

#endif // CPPCOUCH_DATABASE_H
