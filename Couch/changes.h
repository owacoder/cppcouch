#ifndef CPPCOUCH_CHANGES_H
#define CPPCOUCH_CHANGES_H

#include "communication.h"
#include "connection.h"
#include <json.h>

#if 0
namespace couchdb
{
    /* Changes class - This class opens a connection to CouchDB in any of the various feed modes,
     * though it was specifically designed for a continuous connection. When a change is made in
     * CouchDB, this class will emit the signal changeOccured() with CouchDB's update.
     * When the poll is stopped by stopPoll(), the changesFeedClosed() signal will be emitted.
     */

    class signal_base
    {
        void change_occured(const json::value &change) = 0;
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
            , ignore(0)
            , pause(false)
            , consumeRequests(true)
        {}

        // Returns the URL of the CouchDB server
        std::string getURL() const {return comm->getURL();}

        // Returns the name database this object is polling
        std::string getDatabaseName() const {return db;}

        // Returns the URL of the database this object is polling
        std::string getDatabaseURL() const {return comm->getURL() + "/" + url_encode(db);}

    public slots:
        //starts a poll of the given database with supplied options
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

        void pausePoll() {pause = true;}
        void unpausePoll(bool ignorePreviousChanges = false)
        {
            if (ignorePreviousChanges)
                changes.clear();
            pause = false;
            // Initiate read immediately
            read(*reply, Network::Http::Response(), boost::system::error_code());
        }

        //clear all IGNORE requests
        void clearFilters()
        {
            pause = false;
            ignore = 0;
            ignoreList.clear();
            consumeRequests = true;
            changes.clear();
        }

        //ignore next request(s) received, of any type or document
        void ignoreNext(int changes = 1) {ignore += qMax(changes, 1);}
        void revertIgnoreNext(int changes = 1)
        {
            ignore -= qMax(changes, 1);
            if (ignore < 0)
                ignore = 0;
        }

        /* uses a custom command to determine whether to ignore a change signal
         * commands:
         *    "delete:id"    - ignore a deleted document with given id
         *    "delete"       - ignore ALL deleted documents; individual delete requests still get consumed;
         *                      this request does not get consumed (can only be externally cleared)
         *    "update:id"    - ignore an un-deleted document with given id
         *    "update"       - ignore ALL un-deleted documents; individual update requests still get consumed;
         *                      this request does not get consumed (can only be externally cleared)
         *    "all:id"       - ignore all changes with given id
         *    "all"          - ignore ALL changes; individual update requests still get consumed
         *                      this request does not get consumed (can only be externally cleared)
         */
        void ignoreRequest(const std::string &request) {ignoreList.append(request);}
        void revertIgnoreRequest(const std::string &request) {ignoreList.removeOne(request);}
        //either consume requests or not
        void consumeIgnoreRequests(bool b) {consumeRequests = b;}

    signals:
        //emitted when a change occured in CouchDB, with CouchDB's response data
        void changeOccured(const Json::JsonObject &change);

        //emitted when the feed has been canceled
        void changesFeedClosed();

    private slots:
        void closed(Network::Http::AsyncConnection &c,
                    const boost::system::error_code &err)
        {
            (void) c;
            (void) err;
            emit changesFeedClosed();
        }

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

    private:
        signal_type signaller;
        std::string db;
        std::vector<std::string> changes_;
        std::shared_ptr<communication_type> comm;
        int ignore, pause;
        std::vector<std::string> ignoreList;
        bool consumeRequests;
    };
}
#endif

#endif // CPPCOUCH_CHANGES_H
