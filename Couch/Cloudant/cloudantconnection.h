#ifndef CPPCOUCH_CLOUDANTCONNECTION_H
#define CPPCOUCH_CLOUDANTCONNECTION_H

#include "../connection.h"
#include "cloudantdatabase.h"
#include <QFlags>

namespace CPPCOUCH
{
    /* Connection class - The parent object for communicating with Cloudant. Any program
     * communicating with Cloudant must have a CloudantConnection object to handle communications
     * to and from the specialized document/database/view/etc. classes.
     *
     * IMPORTANT NOTE: Any document/database/view/etc. classes require their parent CloudantConnection
     * object to be valid at all times. This is because of internal referencing to the Communication
     * object.
     */

    class CloudantConnection : public Connection
    {
        /* make copy constructor and assignment operator private to prevent copying a CloudantConnection object */
        CloudantConnection(const CloudantConnection &) : Connection() {}
        CloudantConnection &operator=(const CloudantConnection &) {return *this;}

    public:
        CloudantConnection(Network::Http::AsyncConnectionManager *_network = 0, const User &user = User(), AuthenticationType authType = Authentication_None, int timeout = -1)
            : Connection(_network, user, authType, timeout) {}
        CloudantConnection(const std::string &url, Network::Http::AsyncConnectionManager *_network = 0, const User &user = User(), AuthenticationType authType = Authentication_None, int timeout = -1)
            : Connection(url, _network, user, authType, timeout) {}
        virtual ~CloudantConnection() {}

        //specialized login for cloudant. logout is still the same and doesn't need to be reimplemented
        virtual Error login()
        {
            switch (comm.getAuthenticationType())
            {
                case Authentication_Cookie:
                {
                    Communication::HeaderMap headers;
                    QPair<JsonDocument, Error> pair;
                    std::string auth;

                    auth = comm.getUser().toXWWWFormurl_encoded();
                    headers["Content-Type"] = "application/x-www-form-url_encoded";

                    comm.setAuthenticationType(Authentication_None); //clear so comm won't try to use the cookie to log in right away
                    pair = comm.getData("/_session", headers, "POST", auth);

                    comm.setAuthenticationType(Authentication_Cookie); //reset to original value
                    return pair.second;
                }
                case Authentication_Basic:
                case Authentication_None:
                default:
                    break;
            }

            return Error();
        }

        //returns a Cloudant-generated API key (randomly generated username and password)
        virtual QPair<User, Error> getNewApiKey()
        {
            User user;
            JsonObject obj;
            QPair<JsonDocument, Error> pair = comm.getData("/_api/v2/api_keys", "POST");

            if (pair.second.isError())
                return QPair<User, Error>(User(), pair.second);

            obj = pair.first.object();

            user.setUser(obj.value("key").toString().toUtf8());
            user.setPassword(obj.value("password").toString().toUtf8());

            return QPair<User, Error>(user, pair.second);
        }
    };
}

#endif // CPPCOUCH_CLOUDANTCONNECTION_H
