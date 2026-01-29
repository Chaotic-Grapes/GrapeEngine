/* Start Header *****************************************************************/
/*!
\file    StringTable.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\brief
Implementation of the ECS string interning table.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/StringTable.h"

namespace ECS {

    std::mutex StringTable::s_mutex{};
    std::unordered_map<std::string, uint32_t> StringTable::s_stringToId{};
    std::vector<std::string> StringTable::s_idToString{ "" }; // index 0 reserved

    uint32_t StringTable::Intern(const std::string& value) {
        std::lock_guard<std::mutex> lock(s_mutex);

        auto it = s_stringToId.find(value);
        if (it != s_stringToId.end()) {
            return it->second;
        }

        uint32_t id = static_cast<uint32_t>(s_idToString.size());
        s_idToString.push_back(value);
        s_stringToId.emplace(value, id);
        return id;
    }

    uint32_t StringTable::Intern(const char* value) {
        if (!value) {
            return 0;
        }
        return Intern(std::string(value));
    }

    std::string StringTable::Resolve(uint32_t id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (id == 0 || id >= s_idToString.size()) {
            return std::string();
        }
        return s_idToString[id];
    }

    bool StringTable::TryResolve(uint32_t id, std::string& out) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (id == 0 || id >= s_idToString.size()) {
            return false;
        }
        out = s_idToString[id];
        return true;
    }

    void StringTable::Clear() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_stringToId.clear();
        s_idToString.clear();
        s_idToString.emplace_back("");
    }
}
