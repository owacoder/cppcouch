#ifndef CPPCOUCH_CLUSTER_CONNECTION_H
#define CPPCOUCH_CLUSTER_CONNECTION_H

#include "database.h"
#include "connection.h"
#include "user.h"

#include "shared.h"

namespace couchdb
{
    /* Connection class - The parent object for communicating with CouchDB. Any program
     * communicating with CouchDB must have a Connection object to handle communications
     * to and from the specialized document/database/view/etc. classes.
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
    class cluster_connection : public connection<http_client>
    {
        friend class locator<http_client>;
        friend class database<http_client>;
        friend class document<http_client>;
        friend class changes<http_client>;
        friend class connection<http_client>;
        friend class const_iterator;

        typedef communication<http_client> base;
        typedef node_connection<http_client> node_type;

    protected:
        unsigned short node_local_port_; // node_local_port_ is the port, accessible from the localhost, that provides access to the internal node configuration

        cluster_connection(unsigned short local_node_port, std::shared_ptr<base> comm) : connection<http_client>(comm), node_local_port_(local_node_port) {}

    public:
        class const_iterator : std::iterator<std::forward_iterator_tag, std::shared_ptr<node_type>>
        {
            friend class cluster_connection<http_client>;

        protected:
            const_iterator(cluster_connection<http_client> *p, const std::string &d = {}) : d(d), p(p) {}

        public:
            const_iterator() : p(NULL) {}

            node_type operator*() {return node_type(p->node_local_port_, d, p->comm);}
            std::shared_ptr<node_type> operator->() {return std::make_shared<node_type>(node_type(p->node_local_port_, d, p->comm));}

            bool operator==(const const_iterator &other) {return d == other.d;}
            bool operator!=(const const_iterator &other) {return d != other.d;}

            const_iterator &operator++() {return get_next_node();}
            const_iterator operator++(int) {const_iterator save(*this); get_next_node(); return save;}

        private:
            const_iterator &get_next_node()
            {
                if (p && !d.empty())
                {
                    auto nodes = p->list_cluster_node_names();
                    for (auto it = nodes.begin(); it != nodes.end(); ++it)
                    {
                        if (*it == d)
                        {
                            if (it+1 == nodes.end())
                                d.clear();
                            else
                                d = *(it + 1);

                            break;
                        }
                    }
                }

                return *this;
            }

            std::string d; // This contains the name of the node as returned by CouchDB
            cluster_connection<http_client> *p; // A pointer to the parent connection. An iterator stays valid for as long as the parent is alive.
        };

        virtual ~cluster_connection() {}

        const_iterator begin()
        {
            auto list = list_all_node_names();
            if (list.empty())
                return end();

            return const_iterator(this, list.front());
        }

        const_iterator end() {return const_iterator(this);}

        // Returns all cluster nodes this node knows about, including those returned by get_cluster_nodes()
        virtual std::vector<std::string> list_all_node_names()
        {
            std::vector<std::string> result;
            json::value response = this->comm->get_data("/_membership");
            if (!response.is_object())
                throw error(error::bad_response);

            response = response["all_nodes"];
            if (!response.is_array())
                throw error(error::bad_response);

            for (auto n: response.get_array())
                result.push_back(n.get_string());

            return result;
        }

        // Returns all cluster nodes this node knows about, including those returned by get_cluster_nodes()
        virtual std::vector<std::shared_ptr<node_type>> list_all_nodes()
        {
            std::vector<std::shared_ptr<node_type>> result;
            json::value response = this->comm->get_data("/_membership");
            if (!response.is_object())
                throw error(error::bad_response);

            response = response["all_nodes"];
            if (!response.is_array())
                throw error(error::bad_response);

            for (auto n: response.get_array())
                result.push_back(std::make_shared<node_type>(node_type(this->node_local_port_, n.get_string(), this->comm)));

            return result;
        }

        // Returns all nodes that are part of this node's cluster
        virtual std::vector<std::string> list_cluster_node_names()
        {
            std::vector<std::string> result;
            json::value response = this->comm->get_data("/_membership");
            if (!response.is_object())
                throw error(error::bad_response);

            response = response["cluster_nodes"];
            if (!response.is_array())
                throw error(error::bad_response);

            for (auto n: response.get_array())
                result.push_back(n.get_string());

            return result;
        }

        // Returns all nodes that are part of this node's cluster
        virtual std::vector<node_type> list_cluster_nodes()
        {
            std::vector<node_type> result;
            json::value response = this->comm->get_data("/_membership");
            if (!response.is_object())
                throw error(error::bad_response);

            response = response["cluster_nodes"];
            if (!response.is_array())
                throw error(error::bad_response);

            for (auto n: response.get_array())
                result.push_back(node_type(this->node_local_port_, n.get_string(), this->comm));

            return result;
        }

        // Returns response from /_cluster_setup endpoint
        std::string get_initialization_state()
        {
            json::value response = this->comm->get_data("/_cluster_setup", "GET");
            if (!response.is_object())
                throw error(error::bad_response);

            return response["state"].get_string();
        }

        // Attempts to initialize this server as a single-node configuration
        // The bind_address must specify an IP address to bind to, not a symbolic name (e.g. "0.0.0.0")
        // If no databases are specified, the CouchDB-specified defaults are used
        // NOTE: connections to servers set up as single-node configurations cannot just be upgraded to a node_connection
        //   by calling upgrade_to_node_connection(). They first need to be upgraded to a cluster_connection, and then the node connection
        //   retrieved from there with list_all_nodes() or list_cluster_nodes().
        cluster_connection &initialize_as_single_node(const std::string &bind_address,
                                                      unsigned short port,
                                                      const user &admin,
                                                      const std::vector<std::string> &dbs_to_create = {})
        {
            json::value request;

            if (bind_address.empty() || admin.username().empty() || admin.password().empty())
                throw error(error::invalid_argument, "cluster_connection<http_client>::initialize_as_single_node() received a bad parameter");

            request["action"] = "enable_single_node";
            request["bind_address"] = bind_address;
            if (port)
                request["port"] = port;
            request["username"] = admin.username();
            request["password"] = admin.password();
            if (!dbs_to_create.empty())
            {
                for (auto db: dbs_to_create)
                    request["ensure_dbs_exist"].push_back(db);
            }

            this->comm->get_data("/_cluster_setup", "POST", json_to_string(request));

            return *this;
        }

        // initialize_as_cluster_node() must be called on every node when setting up a cluster
        //
        // Attempts to initialize this server as a cluster node, with number of nodes equal to cluster_nodes
        // The bind_address must specify an IP address to bind to, not a symbolic name (e.g. "0.0.0.0")
        //
        // After this function is called on each node, initialize_cluster() must be called to link nodes together
        // See https://docs.couchdb.org/en/master/cluster/setup.html#the-cluster-setup-wizard
        cluster_connection initialize_as_cluster_node(unsigned int cluster_nodes,
                                                      const std::string &bind_address,
                                                      unsigned short port,
                                                      const user &admin)
        {
            json::value request;

            if (bind_address.empty() || admin.username().empty() || admin.password().empty())
                throw error(error::invalid_argument, "cluster_connection<http_client>::initialize_as_cluster_node() received a bad parameter");

            request["action"] = "enable_cluster";
            request["bind_address"] = bind_address;
            if (port)
                request["port"] = port;
            request["username"] = admin.username();
            request["password"] = admin.password();
            request["node_count"] = cluster_nodes;

            this->comm->get_data("/_cluster_setup", "POST", json_to_string(request));

            return *this;
        }

        // initialize_cluster() must be called on one and only one node when setting up a cluster.
        //
        // Attempts to join all nodes in a cluster, with number of nodes equal to cluster_nodes
        // The bind_address must specify an IP address to bind to, not a symbolic name (e.g. "0.0.0.0")
        // Entries in node_urls must specify remote IP addresses WITH a specified port and administrator credentials. If no administrator credentials are provided
        // in the remote URLs, this function will fail with an `error::invalid_argument` exception.
        //
        // After this function is called on each node, finish_initialize_as_cluster() must be called to link nodes together
        // See https://docs.couchdb.org/en/master/cluster/setup.html#the-cluster-setup-wizard
        cluster_connection &initialize_cluster(unsigned int cluster_nodes,
                                               const std::string &bind_address,
                                               unsigned short port,
                                               const user &admin,
                                               const std::vector<std::string> &node_urls)
        {
            if (bind_address.empty() || admin.username().empty() || admin.password().empty())
                throw error(error::invalid_argument, "cluster_connection<http_client>::initialize_cluster() received a bad parameter");

            for (auto node: node_urls)
            {
                typename http_client::url_type node_url;
                node_url.from_string(node);
                json::value request;

                request["action"] = "enable_cluster";
                request["bind_address"] = bind_address;
                if (port)
                    request["port"] = port;
                request["username"] = admin.username();
                request["password"] = admin.password();
                request["node_count"] = cluster_nodes;
                request["remote_node"] = node_url.get_host();
                request["remote_current_user"] = node_url.get_username();
                request["remote_current_password"] = node_url.get_password();

                this->comm->get_data("/_cluster_setup", "POST", json_to_string(request));

                if (node_url.get_host().empty() || node_url.get_username().empty() || node_url.get_password().empty())
                    throw error(error::invalid_argument, "cluster_connection<http_client>::initialize_cluster() received a bad parameter");

                request = json::value();
                request["action"] = "add_node";
                request["host"] = node_url.get_host();
                if (node_url.get_port())
                    request["port"] = node_url.get_port();
                request["username"] = admin.username();
                request["password"] = admin.password();

                this->comm->get_data("/_cluster_setup", "POST", json_to_string(request));
            }

            return *this;
        }

        // finish_initialize_as_cluster() must be called after the cluster nodes are all linked.
        cluster_connection &finish_initialize_as_cluster()
        {
            json::value request;

            request["action"] = "finish_cluster";

            this->comm->get_data("/_cluster_setup", "POST", json_to_string(request));

            return *this;
        }
    };

    template<typename http_client, typename... args>
    std::shared_ptr<cluster_connection<typename http_client::type>> make_cluster_connection(args... arguments)
    {
        return make_connection<http_client>(arguments...)->upgrade_to_cluster_connection();
    }

    template<typename http_client, typename... args>
    std::shared_ptr<cluster_connection<typename http_client::type>> make_cluster_connection(const http_client &client, args... arguments)
    {
        return make_connection(client, arguments...)->upgrade_to_cluster_connection();
    }

    template<typename http_client, typename... args>
    std::shared_ptr<cluster_connection<typename http_client::type>> make_custom_cluster_connection(unsigned short node_local_port, args... arguments)
    {
        return make_connection(arguments...)->upgrade_to_cluster_connection(node_local_port);
    }

    template<typename http_client, typename... args>
    std::shared_ptr<cluster_connection<typename http_client::type>> make_custom_cluster_connection(unsigned short node_local_port, const http_client &client, args... arguments)
    {
        return make_connection(client, arguments...)->upgrade_to_cluster_connection(node_local_port);
    }
}

#endif // CPPCOUCH_CLUSTER_CONNECTION_H
