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

    /// <summary>
    /// Thread-safe string interning table for ECS components.
    /// ID 0 is reserved for invalid / null strings.
    /// </summary>
    class GRAPEENGINE_API StringTable {
    public:
        /// <summary>
        /// Intern a string and return its ID.
        /// </summary>
        static uint32_t Intern(const std::string& value);

        /// <summary>
        /// Intern a C-string and return its ID.
        /// </summary>
        static uint32_t Intern(const char* value);

        /// <summary>
        /// Resolve an ID back to a string. Returns empty string if not found.
        /// </summary>
        static std::string Resolve(uint32_t id);

        /// <summary>
        /// Try resolve an ID into an output string. Returns true on success.
        /// </summary>
        static bool TryResolve(uint32_t id, std::string& out);

        /// <summary>
        /// Clear the table and reset IDs. Call only when world is destroyed.
        /// </summary>
        static void Clear();

    private:
        static std::mutex s_mutex;
        static std::unordered_map<std::string, uint32_t> s_stringToId;
        static std::vector<std::string> s_idToString;
    };

}

#endif
