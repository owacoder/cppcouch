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
        std::string getLanguage() const {return get_data("language").get_string();}

        // Returns design document options
        json::value getOptions() const {return get_data("options");}

        // Returns design document filters
        json::value getFilters() const {return get_data("filters");}

        // Returns design document lists
        json::value getLists() const {return get_data("lists");}

        // Returns design document rewrites
        json::value getRewrites() const {return get_data("rewrites");}

        // Returns design document shows
        json::value getShows() const {return get_data("shows");}

        // Returns design document updates
        json::value getUpdates() const {return get_data("updates");}

        // Returns the validate_doc_update function definition
        std::string getValidateDocUpdate() const {return get_data("validate_doc_update").get_string();}

        // Returns the raw views object
        json::value getViewsData() const {return get_data("views");}

        // Returns a list of view functions/information
        std::vector<view_information> getViewsInformation() const
        {
            json::value response = get_data("views");
            if (!response.is_object())
                throw error(error::view_unavailable);

            std::vector<view_information> views;
            for (auto it = response.get_object().begin(); it != response.get_object().end(); ++it)
                views.push_back(view_information(it->first, it->second));

            return views;
        }

        // Returns a list of runnable views
        std::vector<view_type> getViews() const
        {
            json::value response = get_data("views");
            if (!response.is_object())
                throw error(error::view_unavailable);

            std::vector<view_type> views;
            for (auto it = response.get_object().begin(); it != response.get_object().end(); ++it)
                views.push_back(view_type(this->comm_, this->db_, this->id_, "_view/" + it->first, this->revision_));

            return views;
        }


        // Sets the function language (normally: javascript)
        void setLanguage(const std::string &language) {set_data("language", language);}

        // Sets design document options
        void setOptions(const json::value &options /* Object */) {set_data("options", options);}

        // Sets design document filters
        void setFilters(const json::value &filters /* Object */) {set_data("filters", filters);}

        // Sets design document lists
        void setLists(const json::value &lists /* Object */) {set_data("lists", lists);}

        // Sets design document rewrites
        void setRewrites(const json::value &rewrites /* Object */) {set_data("rewrites", rewrites);}

        // Sets design document shows
        void setShows(const json::value &shows /* Object */) {set_data("shows", shows);}

        // Sets design document updates
        void setUpdates(const json::value &updates /* Object */) {set_data("updates", updates);}

        // Sets validate_doc_update function definition content
        void setValidateDocUpdate(const std::string &validate_doc_update) {set_data("validate_doc_update", validate_doc_update);}

        // Sets a raw view object (overwrites what views are in the document)
        void setViews(const json::value &views /* Object */) {set_data("views", views);}

        // Sets a list of well defined views (overwrites what views are in the document)
        void setViews(const std::vector<view_information> &views)
        {
            json::value obj;
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

            json::value response = this->comm_->get_data("/" + url_encode(this->db_) + "/_compact/" + n, "POST");
            if (!response.is_object())
                throw error(error::database_unavailable);

            if (response.is_member("error"))
            {
#ifdef CPPCOUCH_DEBUG
                std::cout << "Compaction of view indexes in " << this->id_ << " failed: " << response["reason"].get_string();
#endif
                throw error(error::database_unavailable, response["reason"].get_string());
            }

            if (!response["ok"].get_bool())
                throw error(error::database_unavailable);
        }

    protected:
        json::value get_data(const std::string &key) const
        {
            json::value response = document<http_client>::get_data();
            if (!response.is_object())
                throw error(error::document_unavailable);

            return response[key];
        }

        void set_data(const std::string &key, const json::value &value)
        {
            json::value response = document<http_client>::get_data();
            if (!response.is_object())
                throw error(error::document_unavailable);

            response[key] = value;

            return document<http_client>::set_data(response);
        }
    };
}

#endif // CPPCOUCH_DESIGNDOCUMENT_H
