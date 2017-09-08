#ifndef CPPCOUCH_VIEWINFORMATION_H
#define CPPCOUCH_VIEWINFORMATION_H

#include "../shared.h"

namespace couchdb
{
    /* ViewInformation class - This class stores information about a single view object
     * within a design document. The view name is stored in 'name' and the object
     * should always contain a "map" key with a string containing the function definition,
     * and an optional "reduce" key with a string containing the function definition.
     *
     * UNTESTED
     */

    class view_information
    {
    public:
        view_information(const std::string &name, const std::string &map, const std::string &reduce = std::string())
            : name(name)
            , map(map)
            , reduce(reduce)
        {}

        json::value to_json() const
        {
            json::value result;
            result["map"] = map;
            if (!reduce.empty())
                result["reduce"] = reduce;
            return result;
        }

        std::string name, map, reduce;
    };
}

#endif // CPPCOUCH_VIEWINFORMATION_H
