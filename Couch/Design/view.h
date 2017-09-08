#ifndef CPPCOUCH_VIEW_H
#define CPPCOUCH_VIEW_H

#include "../shared.h"
#include "../communication.h"

namespace couchdb
{
    /* ViewResult struct - This stores a single key/value pair returned from a user-defined view.
     *
     * UNTESTED
     */

    template<typename http_client> class locator;

    struct view_result
    {
        view_result() {}
        view_result(const json::value &key, const json::value &value, const std::string &documentName, const std::string &documentURL)
            : key(key)
            , value(value)
            , documentName(documentName)
            , documentURL(documentURL)
        {}

        json::value key;
        json::value value;
        std::string documentName;
        std::string documentURL;
    };

    /* ViewQuery struct - This struct stores a well-defined query parameter to pass to a view
     */

    struct view_query
    {
        view_query() : useLiteralStrings(true) {}

        std::string key;
        json::value value;
        bool useLiteralStrings; //the string of 'value' will be surrounded by quotes if false
    };

    // Convenience typedefs
    typedef std::vector<view_query> view_queries;
    typedef std::vector<view_result> view_results;

    /* View class - This class references a user-defined view stored in a design document.
     */

    template<typename http_client>
    class view
    {
        friend class design_document<http_client>;
        friend class locator<http_client>;

        typedef communication<http_client> base;

    protected:
        view(std::shared_ptr<base> _comm, const std::string &_db, const std::string &_document,
                     const std::string &_id, const std::string &_revision)
            : comm(_comm)
            , db(_db)
            , document(_document)
            , id(_id)
            , revision(_revision)
        {}

    public:
        // Returns the database this view is stored in
        std::string get_db_name() const {return db;}

        // Returns the design document id this view is stored in
        std::string get_doc_id() const {return document;}

        // Returns the id (or name) of this view
        std::string get_view_id() const {return id;}

        // Returns the revision of the design document this view is stored in
        // (if empty, this object refers to the latest revision)
        std::string get_doc_revision() const {return revision;}

        // Runs the view with specified queries
        view_results query(const view_queries &_queries) const
        {
            std::string queryString;
            for (view_query viewQuery: _queries)
            {
                std::string val;
                if (viewQuery.value.is_string())
                {
                    if (viewQuery.useLiteralStrings)
                        val = viewQuery.value.get_string();
                    else
                        val = "\"" + viewQuery.value.get_string() + "\"";
                }
                else
                    val = json_to_string(viewQuery.value);

                if (!queryString.empty())
                    queryString += "&";

                queryString += url_encode(viewQuery.key) + "=" + url_encode(val);
            }

            return query(queryString);
        }

        // Runs the view with the specified single query
        view_results query(const view_query &viewQuery) const
        {
            std::string val;
            if (viewQuery.value.is_string())
            {
                if (viewQuery.useLiteralStrings)
                    val = viewQuery.value.get_string();
                else
                    val = "\"" + viewQuery.value.get_string() + "\"";
            }
            else
                val = json_to_string(viewQuery.value);

            return query(url_encode(viewQuery.key) + "=" + url_encode(val));
        }

        // Returns the URL of the CouchDB server
        std::string get_server_url() const {return comm->get_server_url();}

        // Returns the URL of the database this view is stored in
        std::string get_db_url() const {return comm->get_server_url() + "/" + url_encode(db);}

        // Returns the URL of the design document this view is stored in
        std::string get_doc_url(bool withRevision = true) const {return get_db_url() + "/" + get_doc_id_and_revision_as_url(withRevision);}

        // Returns the URL of the design document this view is stored in,
        // starting from the document id, with optional revision
        std::string get_doc_id_and_revision_as_url(bool withRevision = true) const
        {
            std::string url = url_encode_doc_id(document);
            if (revision.size() > 0 && withRevision)
                url += "?rev=" + url_encode(revision);
            return url;
        }

        // Returns the URL of this view, with optional revision
        std::string get_view_url(bool withRevision = true) const {return get_db_url() + "/" + get_doc_id_and_revision_and_view_as_url(withRevision);}

        // Returns the URL of this view, starting from the view id, with optional document revision
        std::string get_doc_id_and_revision_and_view_as_url(bool withRevision = true) const
        {
            std::string url = url_encode_doc_id(document) + "/" + url_encode_view_id(id);
            if (revision.size() > 0 && withRevision)
                url += "?rev=" + url_encode(revision);
            return url;
        }

    protected:
        view_results query(const std::string &queries) const
        {
            json::value response;
            view_results results;
            std::string url = "/" + url_encode(db) + "/" + url_encode_doc_id(document) + "/" + url_encode_view_id(id);

            if (revision.size() > 0)
                url += "?rev=" + url_encode(revision);

            if (queries.size() > 0)
                url = add_url_query(url, queries);

            response = comm->get_data(url);
            if (!response.is_object())
                throw error(error::view_unavailable);

            response = response["rows"];
            if (!response.is_array())
                throw error(error::view_unavailable);

            for (json::value val: response)
            {
                if (val.is_object())
                    results.push_back(view_result(val["key"],
                                                  val["value"],
                                                  val["id"].get_string(),
                                                  get_db_url() + "/" + val["id"].get_string()));
            }

            return results;
        }

        std::string getURL(bool withRevision) const
        {
            std::string url = "/" + url_encode(db) + "/" + url_encode_doc_id(document) + "/" + url_encode_view_id(id);
            if (withRevision && revision.size() > 0)
                url += "?rev=" + url_encode(revision);
            return url;
        }

    private:
        std::shared_ptr<base> comm;
        std::string db;
        std::string document;
        std::string id;
        std::string revision;
    };
}

#endif // CPPCOUCH_VIEW_H
