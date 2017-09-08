#include <iostream>

#include "Couch/cppcouch.h"
#include "Network/network.h"
#include <chrono>
#include <thread>

#include "json.h"

template<typename TConnection>
void crawl(TConnection connection)
{
    connection->login();
    std::cout << "supports clusters? " << (connection->get_supports_clusters()? "yes": "no") << std::endl;

    if (connection->get_supports_clusters())
    {
        for (auto node: *connection)
        {
            auto admins = node.list_admin_names();
            std::cout << "===ADMINS=== (" << node.get_node_name() << ")" << std::endl;
            for (auto admin: admins)
                std::cout << admin << std::endl;
        }
    }
    else
    {
        auto admins = connection->upgrade_to_node_connection()->list_admin_names();
        std::cout << "===ADMINS===" << std::endl;
        for (auto admin: admins)
            std::cout << admin << std::endl;
    }

    auto users = connection->list_user_names();
    std::cout << "===USERS===" << std::endl;
    for (auto user: users)
        std::cout << user << std::endl;

    auto dbs = connection->list_dbs();
    std::cout << "===HEIRARCHY===" << std::endl;
    for (auto db: dbs)
    {
        std::cout << db.get_db_name() << std::endl;
        auto docs = db.list_all_docs();
        for (auto doc: docs)
        {
            std::cout << "    " << doc.get_doc_id() << std::endl;
            auto attachments = doc.list_all_attachments();
            for (auto att: attachments)
            {
                std::cout << "        " << att.get_attachment_id() << std::endl;
                std::cout << "            " << att.get_content_type() << std::endl;
                std::cout << "            " << att.get_data() << std::endl;
            }
        }
    }
    connection->logout();
}

int main(int argc, char *argv[])
{
    (void) argc, (void) argv;

    json::value v;

    try
    {
        std::istringstream str("[][null,1{}][false,true,1e-1,null,\"about the right length\",{\"+a key+\":12300},[{\"key1\":null,\"key2\":{\"key\": 9123}}]]");
        for (int i = 0; i < 3; ++i)
        {
            str >> v;
            std::cout << json::to_pretty_json(v, 1) << std::endl;
        }
    }
    catch (json::error e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;

    for (int i = 0; i < 3; ++i)
    {
        v["object_" + std::to_string(i)] = i;
    }
    std::cout << v << std::endl;

    try
    {
        auto connection = couchdb::make_cluster_connection(couchdb::http_impl<>(), "http://localhost:5984", couchdb::user("admin", "admin"), couchdb::auth_none);

        if (!connection)
        {
            std::cout << "Cannot make connection to server\n";
            return 1;
        }

        std::cout << "===SERVER===\n" << connection->get_server_url() << std::endl;

        try
        {
            connection->initialize_as_single_node("0.0.0.0", 5984, couchdb::user("admin", "admin"));
        } catch (couchdb::error) {}
        connection->set_auth_type(couchdb::auth_basic);

        auto db = connection->ensure_db_exists("test_db");
        for (int i = 0; i < 10000; ++i)
        {
            db.ensure_doc_exists("test_" + std::to_string(i));
            std::cout << i << std::endl;
        }

        crawl(connection);
        return 0;
    }
    catch (couchdb::error &e)
    {
        std::cerr << "\n===ERROR===\n" << couchdb::error::errorToString(e.type()) << "\n" << e.reason() << "\n" << e.network_request() << "\n" << e.network_response_code() << "\n" << e.network_response() << std::endl;
    }

    return 0;
}
