/* Start Header *****************************************************************/
/*!
\file    GUIStringCache.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Defines a per-frame cache for resolving StringTable IDs into std::string.

This avoids repeated StringTable::Resolve calls for frequently used GUI strings.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef GUI_STRING_CACHE_H
#define GUI_STRING_CACHE_H

#include "ecs/StringTable.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace ECS {
    namespace UI {

        class GUIStringCache {
        public:
            void Clear() { m_cache.clear(); }

            const std::string& Resolve(uint32_t id) {
                static const std::string kEmpty;
                if (id == 0) {
                    return kEmpty;
                }

                const auto it = m_cache.find(id);
                if (it != m_cache.end()) {
                    return it->second;
                }

                auto [inserted, _] = m_cache.emplace(id, ECS::StringTable::Resolve(id));
                return inserted->second;
            }

        private:
            std::unordered_map<uint32_t, std::string> m_cache;
        };

    } // namespace UI
} // namespace ECS

#endif
