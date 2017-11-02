#ifndef CPPCOUCH_CHANGES_H
#define CPPCOUCH_CHANGES_H

#include "communication.h"
#include "connection.h"
#include <json.h>

namespace couchdb
{
    /* Changes class - This class opens a connection to CouchDB in any of the various feed modes,
     * though it was specifically designed for a continuous connection. When a change is made in
     * CouchDB, this class will emit the signal changeOccured() with CouchDB's update.
     * When the poll is stopped by stopPoll(), the changesFeedClosed() signal will be emitted.
     */

    class signal_base
    {
        virtual void change_occured(const json::value &change) = 0;
    };

    template<typename http_client, typename signal_type>
    class changes
    {
        changes(const changes &) {}
        void operator=(const changes &) {}

        typedef connection<http_client> connection_type;
        typedef communication<http_client> communication_type;

    public:
        changes(std::shared_ptr<connection_type> connection, signal_type signaller = signal_type())
            : signaller(signaller)
            , comm(connection->comm)
        {}

        // Returns the URL of the CouchDB server
        std::string get_server_url() const {return comm->get_server_url();}

        // Returns the name database this object is polling
        std::string get_db_name() const {return db;}

        // Returns the URL of the database this object is polling
        std::string get_db_url() const {return comm->getURL() + "/" + url_encode(db);}

#if 0
        // Starts a poll of the given database with supplied options
        Error startPoll(const std::string &db, const std::string &feedStyle = "continuous", const Queries &options = Queries())
        {
            if (reply != NULL && reply->connected())
                return Error(Error::CommunicationError, tr("Only one poll may be started at a time"));

            if (reply)
                delete reply;

            Network::Http::Request request;

            std::string url = comm->getURL() + "/" + url_encode(db) + "/_changes?feed=" + feedStyle;
            url = addURLQueries(url, options);

            this->db = db;
            request.setUrl(url.toStdString());

            changes.clear();
            reply = comm->network->createConnection();
            comm->network->setPartialResponseHandler(reply, boost::bind(&changes::read, this, _1, _2, _3));
            comm->network->setDisconnectHandler(reply, boost::bind(&changes::closed, this, _1, _2));
            reply->setPartialResponseType(Network::Http::AsyncConnection::ResponseLine);
            reply->getLater(request);
            reply->connect();

            return Error();
        }

        //cancels the current poll operation. changesFeedClosed() will be emitted
        Error stopPoll()
        {
            if (!reply)
                return Error(Error::CommunicationError, tr("No poll has been started"));

            reply->disconnectImmediately();
            delete reply;
            reply = 0;

            return Error();
        }

    private:
        void read(Network::Http::AsyncConnection &c,
                  const Network::Http::Response &response,
                  const boost::system::error_code &err)
        {
            (void) c;
            (void) err;

            std::string data = std::string::fromStdString(response.body());
            std::stringList split = data.split('\n');

            if (pause)
            {
                changes += split;
                return;
            }

            split = changes + split;
            changes.clear();

            foreach (std::string b, split)
            {
                b = b.trimmed();
                if (!b.isEmpty())
                {
                    if (!ignore)
                    {
                        JsonObject obj = JsonDocument::fromJson(b).object();
                        JsonObject doc = obj.value("doc").toObject();
                        bool deleted = doc.value("_deleted").toBool();
                        std::string temp, id = doc.value("_id").toByteArray();
                        bool ignored = false;

                        temp = "delete:" + id;
                        if (deleted && ignoreList.contains(temp))
                            ignored = true, consumeRequests? ignoreList.removeOne(temp): 0;

                        temp = "update:" + id;
                        if (!deleted && ignoreList.contains(temp))
                            ignored = true, consumeRequests? ignoreList.removeOne(temp): 0;

                        temp = "all:" + id;
                        if (ignoreList.contains(temp))
                            ignored = true, consumeRequests? ignoreList.removeOne(temp): 0;

                        if (ignored || ignoreList.contains("all") ||
                            (deleted && ignoreList.contains("delete")) ||
                            (!deleted && ignoreList.contains("update")))
                            continue;

                        emit changeOccured(obj);
                    }
                    else
                        --ignore;
                }
            }
        }
#endif

    private:
        std::string db;
        signal_type signaller;
        std::shared_ptr<communication_type> comm;
    };
}

#endif // CPPCOUCH_CHANGES_H
