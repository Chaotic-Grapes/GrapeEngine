/* Start Header *****************************************************************/
/*!
\file    LayerManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the LayerManager
class, responsible for managing multiple layers in the application. It
allows adding, removing, and switching between layers, as well as
serializing and deserializing layers to and from JSON files.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef LAYERMANAGER_H
#define LAYERMANAGER_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include "ecs/World.h"

namespace Scenes {
    class LayerManager {
    public:

        /**
         * @brief Creates or retrieves a layer ID by its name.
         * @param name The name of the layer.
         * @return The ID of the layer.
         */
        uint16_t CreateOrGetLayer(const std::string& name) {
            auto it = m_nameToId.find(name);
            if (it != m_nameToId.end())
                return it->second;

            uint16_t id = static_cast<uint16_t>(m_idToEntities.size());
            m_nameToId[name] = id;
            m_idToName.push_back(name);
            m_idToEntities.emplace_back();

            return id;
        }

        /**
         * @brief Gets the name of a layer by its ID.
         * @param id The ID of the layer.
         * @return The name of the layer.
         */
        void OnLayerSet(ECS::Entity e, uint16_t id) {
            _ensureCapacity(id);
            m_idToEntities[id].insert(e);
        }

        /**
         * @brief Removes an entity from a layer by its ID.
         * @param e The entity to remove.
         * @param id The ID of the layer.
         */
        void OnLayerRemoved(ECS::Entity e, uint16_t id) {
            if (id < m_idToEntities.size())
                m_idToEntities[id].erase(e);
        }

        /**
         * @brief Gets the name of a layer by its ID.
         * @param id The ID of the layer.
         * @return The name of the layer.
         */
        const std::unordered_set<ECS::Entity, ECS::EntityHash>& EntitiesIn(uint16_t id) const {
            static const std::unordered_set<ECS::Entity, ECS::EntityHash> EMPTY{};
            if (id >= m_idToEntities.size())
                return EMPTY;

            return m_idToEntities[id];
        }

        /**
         * @brief Gets the ID of a layer by its name.
         * @param name The name of the layer.
         * @return The ID of the layer, or std::nullopt if not found.
         */
        std::optional<uint16_t> IdOf(const std::string& name) const {
            auto it = m_nameToId.find(name);
            if (it == m_nameToId.end())
                return std::nullopt;

            return it->second;
        }

    private:
        void _ensureCapacity(uint16_t id) {
            while (m_idToEntities.size() <= id)
                m_idToEntities.emplace_back();
        }

    private:
        std::unordered_map<std::string, uint16_t> m_nameToId;
        std::vector<std::string> m_idToName;
        std::vector<std::unordered_set<ECS::Entity, ECS::EntityHash>> m_idToEntities;
    };
}

#endif
