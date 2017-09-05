#ifndef CPPCOUCH_CLOUDANTDATABASE_H
#define CPPCOUCH_CLOUDANTDATABASE_H

#include "../database.h"

namespace CPPCOUCH
{
    enum CloudantRole
    {
        CloudantReaderRole = 0x01,
        CloudantWriterRole = 0x02,
        CloudantAdminRole  = 0x04,
        CloudantReplicatorRole = 0x08,
        CloudantDBUpdatesRole = 0x10,
        CloudantDesignRole = 0x20,
        CloudantShardsRole = 0x40,
        CloudantSecurityRole = 0x80
    };

    typedef unsigned int CloudantRoles;

    class CloudantDatabase : public Database
    {
    public:
        static JsonArray convertRolesToJSON(CloudantRoles roles)
        {
            JsonArray arr;
            if (roles & CloudantReaderRole)
                arr.append(QString("_reader"));
            if (roles & CloudantWriterRole)
                arr.append(QString("_writer"));
            if (roles & CloudantAdminRole)
                arr.append(QString("_admin"));
            if (roles & CloudantReplicatorRole)
                arr.append(QString("_replicator"));
            if (roles & CloudantDBUpdatesRole)
                arr.append(QString("_db_updates"));
            if (roles & CloudantDesignRole)
                arr.append(QString("_design"));
            if (roles & CloudantShardsRole)
                arr.append(QString("_shards"));
            if (roles & CloudantSecurityRole)
                arr.append(QString("_security"));
            return arr;
        }

        static CloudantRoles convertJSONToRoles(const JsonArray &roles)
        {
            CloudantRoles out = 0;
            for (int i = 0; i < roles.size(); ++i)
            {
                QString str = roles.at(i).toString();
                if (str == "_reader")
                    out |= CloudantReaderRole;
                else if (str == "_writer")
                    out |= CloudantWriterRole;
                else if (str == "_admin")
                    out |= CloudantAdminRole;
                else if (str == "_replicator")
                    out |= CloudantReplicatorRole;
                else if (str == "_db_updates")
                    out |= CloudantDBUpdatesRole;
                else if (str == "_design")
                    out |= CloudantDesignRole;
                else if (str == "_shards")
                    out |= CloudantShardsRole;
            }

            return out;
        }

        CloudantDatabase(const Database &other) : Database(other) {}
        CloudantDatabase(const CloudantDatabase &other) : Database(other) {}
        virtual ~CloudantDatabase() {}

        CloudantDatabase &operator=(const Database &other)
        {
            Database::operator=(other);
            return *this;
        }
        CloudantDatabase &operator=(const CloudantDatabase &other)
        {
            Database::operator=(other);
            return *this;
        }

        virtual QPair<JsonObject, Error> getCloudantPermissions()
        {
            JsonObject obj;
            QPair<JsonDocument, Error> pair = comm->get_data("/_api/v2/db/" + url_encode(getDatabaseName()) + "/_security", "GET");

            if (pair.second.isError())
                return QPair<JsonObject, Error>(JsonObject(), pair.second);

            obj = pair.first.object().value("cloudant").toObject();

            return QPair<JsonObject, Error>(obj, pair.second);
        }

        virtual QPair<QList<QPair<std::string, CloudantRoles> >, Error> getCloudantUserPermissions()
        {
            JsonObject obj;
            QStringList keys;
            QList<QPair<std::string, CloudantRoles> > users;
            QPair<JsonDocument, Error> pair = comm->get_data("/_api/v2/db/" + url_encode(getDatabaseName()) + "/_security", "GET");

            if (pair.second.isError())
                return QPair<QList<QPair<std::string, CloudantRoles> >, Error>(users, pair.second);

            obj = pair.first.object().value("cloudant").toObject();
            keys = obj.keys();

            for (int i = 0; i < keys.size(); ++i)
            {
                QPair<std::string, CloudantRoles> data;
                data.first = keys.at(i).toUtf8();
                data.second = convertJSONToRoles(obj.value(keys.at(i)).toArray());
                users.append(data);
            }

            return QPair<QList<QPair<std::string, CloudantRoles> >, Error>(users, Error());
        }

        virtual Error setCloudantPermissions(const JsonObject &permissions)
        {
            JsonObject obj;
            std::string data;
            Error err;

            obj["cloudant"] = permissions;
            data = JsonDocument(obj).toJson(JsonDocument::Compact);

            QPair<JsonDocument, Error> pair = comm->get_data("/_api/v2/db/" + url_encode(getDatabaseName()) + "/_security", "PUT", data);

            obj = pair.first.object();
            if (!obj.value("ok").toBool())
                err = Error(Error::UnknownError, obj.value("reason").toString());

            return err;
        }

        virtual Error setCloudantUserPermissions(const QList<QPair<std::string, CloudantRoles> > &userPermissions)
        {
            JsonObject obj;

            for (int i = 0; i < userPermissions.size(); ++i)
            {
                obj.add(QString::fromUtf8(userPermissions.at(i).first),
                        convertRolesToJSON(userPermissions.at(i).second));
            }

            return setCloudantPermissions(obj);
        }
    };
}

#endif // CPPCOUCH_CLOUDANTDATABASE_H
