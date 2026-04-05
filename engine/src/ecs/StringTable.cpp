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

    /**
     * @brief Intern a string and return its stable numeric ID.
     *        Adds the string to the table if not already present (thread-safe).
     * @param value The string to intern.
     * @return The unique ID assigned to the string.
     */
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

    /**
     * @brief Intern a C-string and return its stable numeric ID.
     *        Returns 0 for null input (thread-safe).
     * @param value The null-terminated C-string to intern.
     * @return The unique ID assigned to the string, or 0 if value is null.
     */
    uint32_t StringTable::Intern(const char* value) {
        if (!value) {
            return 0;
        }
        return Intern(std::string(value));
    }

    /**
     * @brief Resolve a string ID back to its original string value (thread-safe).
     * @param id The string ID to look up.
     * @return The corresponding string, or an empty string if the ID is invalid.
     */
    std::string StringTable::Resolve(uint32_t id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (id == 0 || id >= s_idToString.size()) {
            return std::string();
        }
        return s_idToString[id];
    }

    /**
     * @brief Attempt to resolve a string ID, writing the result into an output parameter (thread-safe).
     * @param id The string ID to look up.
     * @param out Output parameter populated with the resolved string if found.
     * @return True if the ID was valid and out was populated, false otherwise.
     */
    bool StringTable::TryResolve(uint32_t id, std::string& out) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (id == 0 || id >= s_idToString.size()) {
            return false;
        }
        out = s_idToString[id];
        return true;
    }

    /**
     * @brief Clear the entire string table and reset it to its initial state
     *        with only the reserved empty-string entry at index 0 (thread-safe).
     */
    void StringTable::Clear() {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_stringToId.clear();
        s_idToString.clear();
        s_idToString.emplace_back("");
    }
}
