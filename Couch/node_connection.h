#ifndef CPPCOUCH_NODE_CONNECTION_H
#define CPPCOUCH_NODE_CONNECTION_H

#include "database.h"
#include "connection.h"
#include "user.h"

#include "shared.h"

namespace couchdb
{
    /* node_connection class - A node object for communicating with a single CouchDB server node.
     * Any program wanting to get or set configuration options for the CouchDB server will require
     * an instance of this class.
     *
     * IMPORTANT NOTE: Any document/database/view/etc. classes require their parent Connection
     * object to be valid at all times. This is because of internal referencing to the Communication
     * object.
     */

    template<typename http_client> class locator;
    template<typename http_client> class database;
    template<typename http_client> class document;
    template<typename http_client> class changes;

    template<typename http_client>
    class node_connection : public connection<http_client>
    {
        friend class locator<http_client>;
        friend class database<http_client>;
        friend class document<http_client>;
        friend class changes<http_client>;
        friend class connection<http_client>;
        friend class cluster_connection<http_client>;

        typedef communication<http_client> base;

    protected:
        node_connection(unsigned short local_port, const std::string &node_name, std::shared_ptr<base> c)
            : connection<http_client>(c)
            , node_name_(node_name)
            , node_local_port_(local_port)
        {}

    public:
        virtual ~node_connection() {}

        // Attempts to restart the CouchDB node. After this is called, there is no guarantee the server will come back online
        virtual node_connection &restart_server()
        {
            if (!this->get_supports_clusters())
            {
                if (!this->comm->get_data("/_restart", "POST")["ok"].asBool())
                    throw error(error::request_failed);
            }
            else if (node_name_.empty()) // If clusters are supported and there is no node name, this is being called with an invalid node reference
                throw error(error::invalid_argument);
            else
            {
                typename base::state save = this->comm->get_current_state();
                try
                {
                    typename base::http_client_url_t url;

                    // Modify port to local node
                    url.from_string(this->comm->get_server_url());
                    url.set_port(node_local_port_);
                    this->comm->set_server_url(url.to_string());

                    if (!this->comm->get_data("/_restart", "POST")["ok"].asBool())
                        throw error(error::request_failed);

                    this->comm->set_current_state(save);
                } catch (couchdb::error &)
                {
                    this->comm->set_current_state(save);
                    throw;
                }
            }

            return *this;
        }

        // Returns the entire CouchDB config object
        virtual Json::Value get_config() {return this->comm->get_data(get_config_node_name() + "/_config");}

        // Returns the CouchDB config section specified
        virtual Json::Value get_config_section(const std::string &section) {return this->comm->get_data(get_config_node_name() + "/_config/" + url_encode(section));}

        // Returns the specified CouchDB config key of specified section of config
        virtual Json::Value get_config_key(const std::string &section, const std::string &key)
        {
            return this->comm->get_data(get_config_node_name() + "/_config/" + url_encode(section) + "/" + url_encode(key));
        }

        // Sets the specified CouchDB config key of specified section of config
        // Returns the old value of the key
        virtual Json::Value set_config_key(const std::string &section, const std::string &key, const Json::Value &value)
        {
            return this->comm->get_data(get_config_node_name() + "/_config/" + url_encode(section) + "/" + url_encode(key),
                                                             "PUT",
                                                             json_to_string(value));
        }

        // Deletes the specified CouchDB config key of specified section of config
        // Returns the value of the key before it was deleted
        virtual Json::Value delete_config_key(const std::string &section, const std::string &key)
        {
            return this->comm->get_data(get_config_node_name() + "/_config/" + url_encode(section) + "/" + url_encode(key), "DELETE");
        }

        // Tries to create a new CouchDB admin
        virtual Json::Value create_admin(const std::string &name, const std::string &pass)
        {
            return this->comm->get_data(get_config_node_name() + "/_config/admins/" + url_encode(name), "PUT", json_to_string(string_to_json(pass)));
        }

        // Returns a list of administrators' names from CouchDB
        virtual std::vector<std::string> list_admin_names()
        {
            std::vector<std::string> list;
            Json::Value response = this->comm->get_data(get_config_node_name() + "/_config/admins", "GET");
            if (!response.isObject())
                throw error(error::bad_response);

            for (auto it = response.begin(); it != response.end(); ++it)
                list.push_back(it.key().asString());

            return list;
        }

        // Tries to remove a CouchDB admin
        virtual Json::Value delete_admin(const std::string &name)
        {
            return this->comm->get_data(get_config_node_name() + "/_config/admins/" + url_encode(name), "DELETE");
        }

        // get_node_name() returns an empty string on a non-cluster-supporting (i.e. CouchDB v1.x.x or lower) CouchDB instance,
        // or an invalid node reference of a cluster (for example, through cluster_connection::end()).
        // Otherwise, it returns the name of the node as returned by the /_membership endpoint
        virtual std::string get_node_name() const {return node_name_;}

    protected:
        std::string get_config_node_name() const
        {
            if (node_name_.empty())
                return {};

            return "/_node/" + node_name_;
        }

        std::string node_name_;
        unsigned short node_local_port_; // node_local_port_ is the port, accessible from the localhost, that provides access to the internal node configuration
    };

    template<typename http_client, typename... args>
    std::shared_ptr<node_connection<typename http_client::type>> make_node_connection(args... arguments)
    {
        return make_connection<http_client>(arguments...)->upgrade_to_node_connection();
    }

    template<typename http_client, typename... args>
    std::shared_ptr<node_connection<typename http_client::type>> make_node_connection(const http_client &client, args... arguments)
    {
        return make_connection(client, arguments...)->upgrade_to_node_connection();
    }

    template<typename http_client, typename... args>
    std::shared_ptr<node_connection<typename http_client::type>> make_custom_node_connection(unsigned short node_local_port, args... arguments)
    {
        return make_connection(arguments...)->upgrade_to_node_connection(node_local_port);
    }

    template<typename http_client, typename... args>
    std::shared_ptr<node_connection<typename http_client::type>> make_custom_node_connection(unsigned short node_local_port, const http_client &client, args... arguments)
    {
        return make_connection(client, arguments...)->upgrade_to_node_connection(node_local_port);
    }
}

#endif // CPPCOUCH_NODE_CONNECTION_H
