#ifndef CPPCOUCH_ATTACHMENT_H
#define CPPCOUCH_ATTACHMENT_H

#include "shared.h"
#include "communication.h"

namespace couchdb
{
    /* Attachment class - This class handles various operations regarding a single
     * attachment of a document or design document.
     */

    template<typename http_client> class document;
    template<typename http_client> class locator;

    template<typename http_client>
    class attachment
    {
        friend class document<http_client>;
        friend class locator<http_client>;

        typedef communication<http_client> base;

    protected:
        attachment(std::shared_ptr<base> _comm, const std::string &_db, const std::string &_document,
                     const std::string &_id, const std::string &_revision, const std::string &_contentType, int64_t _size)
            : comm_(_comm)
            , db_(_db)
            , document_(_document)
            , id_(_id)
            , revision_(_revision)
            , content_type_(_contentType)
            , size_(_size)
        {}

    public:
        virtual ~attachment() {}

        // Returns the document this attachment is part of
        virtual document<http_client> get_doc() const {return document<http_client>(comm_, db_, document_, revision_);}

        // Returns the name of the database containing this attachment
        virtual std::string get_db_name() const {return db_;}

        // Returns the id of the document containing this attachment
        virtual std::string get_doc_id() const {return document_;}

        // Returns the id of this attachment
        virtual std::string get_attachment_id() const {return id_;}

        // Returns the revision of the document containing this attachment
        // (if empty, this object refers to the latest revision)
        virtual std::string get_doc_revision() const {return revision_;}

        // Returns the content-type of this attachment
        virtual std::string get_content_type() const {return content_type_;}

        // Returns the size of this attachment in bytes
        // -1 signifies the size is unknown
        virtual int64_t get_size() const {return size_;}

        // Returns the data contained in this attachment
        virtual std::string get_data() const
        {
            std::string data;
            std::string url = "/" + url_encode(db_) + "/" + url_encode_doc_id(document_) + "/" + url_encode(id_);

            if (revision_.size() > 0)
                url += "?rev=" + url_encode(revision_);

            data = comm_->get_raw_data(url);

            if (data.size() > 0 && data[0] == '{')
            {
                // check to make sure we did not receive an error
                Json::Value doc = string_to_json(data);

                if (doc.isObject() && doc.isMember("error") && doc.isMember("reason"))
                {
#ifdef CPPCOUCH_DEBUG
                    std::cout << "Could not retrieve data for attachment \"" + id_ + "\": " + doc["reason"].asString();
#endif
                    throw error(error::attachment_unavailable, doc["reason"].asString());
                }
            }

            return data;
        }

        // Sets the data contained in this attachment
        // and updates this attachment to point to the new revision of the document
        // IMPORTANT: No other attachments will be updated
        virtual attachment &set_data(const std::string &data, const std::string &_contentType = "")
        {
            typename base::header_map headers;
            if (_contentType.empty())
                headers["Content-Type"] = content_type_;
            else
                headers["Content-Type"] = _contentType;

            Json::Value obj = comm_->get_data(getURL(true), headers, "PUT", data);
            if (!obj.isObject())
                throw error(error::attachment_unavailable);

            if (obj.isMember("error") && obj.isMember("reason"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Could not update attachment \"" + id_ + "\": " + obj["reason"].asString();
#endif
                throw error(error::attachment_unavailable, obj["reason"].asString());
            }

            if (!obj["ok"].asBool())
                throw error(error::attachment_unavailable);

            revision_ = obj["rev"].asString();
            size_ = data.size();

            return *this;
        }

        // Returns the URL of the CouchDB server
        virtual std::string get_server_url() const {return comm_->get_server_url();}

        // Returns the URL of the database containing this attachment
        virtual std::string get_db_url() const {return comm_->get_server_url() + "/" + url_encode(db_);}

        // Returns the URL of the document containing this attachment, with optional revision
        virtual std::string get_doc_url(bool withRevision = true) const {return get_db_url() + "/" + get_doc_id_and_revision_as_url(withRevision);}

        // Returns the URL of the document containing this attachment,
        // starting from the document id, with optional revision
        virtual std::string get_doc_id_and_revision_as_url(bool withRevision = true) const
        {
            std::string url = url_encode_doc_id(document_);
            if (revision_.size() > 0 && withRevision)
                url += "?rev=" + url_encode(revision_);
            return url;
        }

        // Returns the URL of this attachment, with optional document revision
        virtual std::string get_attachment_url(bool withRevision = true) const {return get_db_url() + "/" + get_doc_id_and_revision_and_attachment_as_url(withRevision);}

        // Returns the URL of this attachment, starting from the document id,
        // with optional revision
        virtual std::string get_doc_id_and_revision_and_attachment_as_url(bool withRevision = true) const
        {
            std::string url = url_encode_doc_id(document_) + "/" + url_encode_attachment_id(id_);
            if (revision_.size() > 0 && withRevision)
                url += "?rev=" + url_encode(revision_);
            return url;
        }

    protected:
        std::string getURL(bool withRevision) const
        {
            std::string url = "/" + url_encode(db_) + "/" + url_encode_doc_id(document_) + "/" + url_encode_attachment_id(id_);
            if (withRevision && revision_.size() > 0)
                url += "?rev=" + url_encode(revision_);
            return url;
        }

    private:
        std::shared_ptr<base> comm_;
        std::string db_;
        std::string document_;
        std::string id_;
        std::string revision_;
        std::string content_type_;
        int64_t size_;
    };
}

#endif // CPPCOUCH_ATTACHMENT_H
