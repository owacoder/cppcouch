#ifndef CPPCOUCH_DESIGNDOCUMENT_H
#define CPPCOUCH_DESIGNDOCUMENT_H

#include "../document.h"
#include "view.h"
#include "viewinformation.h"

namespace couchdb
{
    /* DesignDocument class - provides a thin wrapper on top of a normal document for design document
     * methods.
     *
     * UNTESTED
     */

    template<typename http_client> class database;

    template<typename http_client>
    class design_document : public document<http_client>
    {
        friend class database<http_client>;
        friend class locator<http_client>;

        typedef communication<http_client> base;
        typedef view<http_client> view_type;

    protected:
        design_document(std::shared_ptr<base> _comm, const std::string &_db,
                   const std::string &_id, const std::string &_rev)
            : document<http_client>(_comm, _db, _id, _rev)
        {}

    public:
        virtual ~design_document() {}

        bool operator==(const design_document &other) const {return document<http_client>::operator==(other);}
        bool operator!=(const design_document &other) const {return !(*this == other);}

        // Returns the language used by user-defined view functions (normally: javascript)
        std::string getLanguage() const {return get_data("language").asString();}

        // Returns design document options
        Json::Value getOptions() const {return get_data("options");}

        // Returns design document filters
        Json::Value getFilters() const {return get_data("filters");}

        // Returns design document lists
        Json::Value getLists() const {return get_data("lists");}

        // Returns design document rewrites
        Json::Value getRewrites() const {return get_data("rewrites");}

        // Returns design document shows
        Json::Value getShows() const {return get_data("shows");}

        // Returns design document updates
        Json::Value getUpdates() const {return get_data("updates");}

        // Returns the validate_doc_update function definition
        std::string getValidateDocUpdate() const {return get_data("validate_doc_update").asString();}

        // Returns the raw views object
        Json::Value getViewsData() const {return get_data("views");}

        // Returns a list of view functions/information
        std::vector<view_information> getViewsInformation() const
        {
            Json::Value response = get_data("views");
            if (!response.isObject())
                throw error(error::view_unavailable);

            std::vector<view_information> views;
            for (auto it = response.begin(); it != response.end(); ++it)
                views.push_back(view_information(it.key(), *it));

            return views;
        }

        // Returns a list of runnable views
        std::vector<view_type> getViews() const
        {
            Json::Value response = get_data("views");
            if (!response.isObject())
                throw error(error::view_unavailable);

            std::vector<view_type> views;
            for (auto it = response.begin(); it != response.end(); ++it)
                views.push_back(view_type(this->comm_, this->db_, this->id_, "_view/" + it.key().asString(), this->revision_));

            return views;
        }


        // Sets the function language (normally: javascript)
        void setLanguage(const std::string &language) {set_data("language", language);}

        // Sets design document options
        void setOptions(const Json::Value &options /* Object */) {set_data("options", options);}

        // Sets design document filters
        void setFilters(const Json::Value &filters /* Object */) {set_data("filters", filters);}

        // Sets design document lists
        void setLists(const Json::Value &lists /* Object */) {set_data("lists", lists);}

        // Sets design document rewrites
        void setRewrites(const Json::Value &rewrites /* Object */) {set_data("rewrites", rewrites);}

        // Sets design document shows
        void setShows(const Json::Value &shows /* Object */) {set_data("shows", shows);}

        // Sets design document updates
        void setUpdates(const Json::Value &updates /* Object */) {set_data("updates", updates);}

        // Sets validate_doc_update function definition content
        void setValidateDocUpdate(const std::string &validate_doc_update) {set_data("validate_doc_update", validate_doc_update);}

        // Sets a raw view object (overwrites what views are in the document)
        void setViews(const Json::Value &views /* Object */) {set_data("views", views);}

        // Sets a list of well defined views (overwrites what views are in the document)
        void setViews(const std::vector<view_information> &views)
        {
            Json::Value obj;
            for (auto v: views)
                obj[v.name] = v.to_json();
            return set_data("views", obj);
        }

        // Compacts the view indexes for this design document
        void compactViews()
        {
            std::string n(url_encode_doc_id(this->id_));
            if (n.find("_design/") == 0)
                n.erase(0, 8);

            Json::Value response = this->comm_->get_data("/" + url_encode(this->db_) + "/_compact/" + n, "POST");
            if (!response.isObject())
                throw error(error::database_unavailable);

            if (response.isMember("error"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Compaction of view indexes in " << this->id_ << " failed: " << response["reason"].asString();
#endif
                throw error(error::database_unavailable, response["reason"].asString());
            }

            if (!response["ok"].asBool())
                throw error(error::database_unavailable);
        }

    protected:
        Json::Value get_data(const std::string &key) const
        {
            Json::Value response = document<http_client>::get_data();
            if (!response.isObject())
                throw error(error::document_unavailable);

            return response[key];
        }

        void set_data(const std::string &key, const Json::Value &value)
        {
            Json::Value response = document<http_client>::get_data();
            if (!response.isObject())
                throw error(error::document_unavailable);

            response[key] = value;

            return document<http_client>::set_data(response);
        }
    };
}

#endif // CPPCOUCH_DESIGNDOCUMENT_H
