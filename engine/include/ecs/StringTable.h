/* Start Header *****************************************************************/
/*!
\file    StringTable.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\brief
Central string interning table for ECS string identifiers. Provides a stable
mapping between string values and uint32_t IDs used in components like Name.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ECS_STRINGTABLE_H
#define ECS_STRINGTABLE_H

#include "Export.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace ECS {

    /**
     * @brief Thread-safe string interning table for ECS components.
     *
     * ID 0 is reserved for invalid or null strings.
     */
    class GRAPEENGINE_API StringTable {
    public:
        /**
         * @brief Intern a string and return its stable ID.
         * @param value Source string to intern.
         * @return Interned string ID.
         */
        static uint32_t Intern(const std::string& value);

        /**
         * @brief Intern a C-string and return its stable ID.
         * @param value Source C-string to intern.
         * @return Interned string ID.
         */
        static uint32_t Intern(const char* value);

        /**
         * @brief Resolve an interned ID back to its string value.
         * @param id Interned string ID.
         * @return Resolved string, or empty string if ID is unknown.
         */
        static std::string Resolve(uint32_t id);

        /**
         * @brief Try resolving an interned ID to a string.
         * @param id Interned string ID.
         * @param out Output string populated on success.
         * @return True when the ID exists in the table.
         */
        static bool TryResolve(uint32_t id, std::string& out);

        /**
         * @brief Clear all interned strings and reset ID allocation.
         */
        static void Clear();

    private:
        static std::mutex s_mutex;
        static std::unordered_map<std::string, uint32_t> s_stringToId;
        static std::vector<std::string> s_idToString;
    };

}

#endif
