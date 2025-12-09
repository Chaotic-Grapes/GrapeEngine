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
#include <vector>
#include <optional>
#include "ecs/World.h"

namespace Scenes {
    class LayerManager {
    public:

        // Construct a LayerManager with some sane default layers pre-registered.
        // For example, reserve up to 100 layer slots and assign the UI layer
        // to ID 99 so systems can rely on a stable UI layer id.
        LayerManager() {
            constexpr uint16_t DefaultCapacity = 100;
            if (m_idToName.size() < DefaultCapacity) m_idToName.resize(DefaultCapacity);
            if (m_idToEntities.size() < DefaultCapacity) m_idToEntities.resize(DefaultCapacity);

            // Register a set of well-known default layers with stable IDs.
            // New dynamic layers will be allocated after the highest default ID.
            constexpr uint16_t BackgroundLayer = 0;
            constexpr uint16_t WorldLayer = 1;
            constexpr uint16_t GameplayLayer = 2;
            constexpr uint16_t ForegroundLayer = 3;
            constexpr uint16_t UILayer = 98;
            constexpr uint16_t DebugLayer = 99;

            // Populate defaults
            m_nameToId["Background"] = BackgroundLayer;
            m_idToName[BackgroundLayer] = "Background";

            m_nameToId["World"] = WorldLayer;
            m_idToName[WorldLayer] = "World";

            m_nameToId["Gameplay"] = GameplayLayer;
            m_idToName[GameplayLayer] = "Gameplay";

            m_nameToId["Foreground"] = ForegroundLayer;
            m_idToName[ForegroundLayer] = "Foreground";

            m_nameToId["UI"] = UILayer;
            m_idToName[UILayer] = "UI";

            m_nameToId["Debug"] = DebugLayer;
            m_idToName[DebugLayer] = "Debug";
        }


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
         * @brief Lists all registered layers.
         * @returns A vector of (id, name) for all registered layers.
         */
        std::vector<std::pair<uint16_t, std::string>> ListLayers() const {
            std::vector<std::pair<uint16_t, std::string>> out;
            for (uint16_t i = 0; i < m_idToName.size(); ++i) {
                if (!m_idToName[i].empty()) {
                    out.emplace_back(i, m_idToName[i]);
                }
            }
            return out;
        }

        /**
         * @brief Rename a layer by ID.
         * @param id The ID of the layer.
         * @param newName The new name for the layer.
         * @returns true on success.
         */
        bool RenameLayer(uint16_t id, const std::string& newName) {
            if (id >= m_idToName.size()) return false;
            const std::string& prev = m_idToName[id];
            if (prev == newName) return true;
            if (!prev.empty()) m_nameToId.erase(prev);
            m_idToName[id] = newName;
            m_nameToId[newName] = id;
            return true;
        }

        /**
         * @brief Create a layer with the given name and return its ID.
         * @param name The name of the layer.
         * @return The ID of the created layer.
         */
        uint16_t CreateLayer(const std::string& name) {
            return CreateOrGetLayer(name);
        }

        /**
         * @brief Create a layer at a specific ID. Used for redo semantics where
         * a previously created layer should be restored at the same slot.
         * If the slot is already occupied the function will return false.
         */
        bool CreateLayerAt(uint16_t id, const std::string& name) {
            _ensureCapacity(id);
            if (!m_idToName[id].empty()) return false;
            m_idToName[id] = name;
            m_nameToId[name] = id;
            return true;
        }

        /**
         * @brief Mark a layer visible/invisible in the editor UI.
         * @param id The ID of the layer.
         * @param visible True to make visible, false to hide.
         */
        void SetVisibility(uint16_t id, bool visible) {
            _ensureCapacity(id);
            if (m_visibility.size() <= id) m_visibility.resize(m_idToName.size(), true);
            m_visibility[id] = visible;
        }

        /**
         * @brief Check if a layer is visible in the editor UI.
         * @param id The ID of the layer.
         * @return True if visible, false if hidden.
         */
        bool IsVisible(uint16_t id) const {
            if (id >= m_visibility.size()) return true;
            return m_visibility[id];
        }

        /**
         * @brief Lock/unlock a layer to prevent modification from editor actions.
         * @param id The ID of the layer.
         * @param locked True to lock, false to unlock.
         */
        void SetLocked(uint16_t id, bool locked) {
            _ensureCapacity(id);
            if (m_locked.size() <= id) m_locked.resize(m_idToName.size(), false);
            m_locked[id] = locked;
        }

        /**
         * @brief Check if a layer is locked.
         * @param id The ID of the layer.
         * @return True if locked, false if unlocked.
         */
        bool IsLocked(uint16_t id) const {
            if (id >= m_locked.size()) return false;
            return m_locked[id];
        }

        /**
         * @brief Move a layer entry from one index to another (reorder).
         * @param fromId The current ID of the layer.
         * @param toId The target ID to move the layer to.
         * @returns true on success.
         */
        bool MoveLayer(uint16_t fromId, uint16_t toId) {
            if (fromId >= m_idToName.size() || toId >= m_idToName.size()) return false;
            if (fromId == toId) return true;

            std::string name = m_idToName[fromId];
            auto entities = std::move(m_idToEntities[fromId]);
            m_idToName.erase(m_idToName.begin() + fromId);
            m_idToEntities.erase(m_idToEntities.begin() + fromId);
            m_idToName.insert(m_idToName.begin() + toId, name);
            m_idToEntities.insert(m_idToEntities.begin() + toId, std::move(entities));

            // Rebuild name->id map
            m_nameToId.clear();
            for (uint16_t i = 0; i < m_idToName.size(); ++i) {
                if (!m_idToName[i].empty()) m_nameToId[m_idToName[i]] = i;
            }
            return true;
        }

        /**
         * @brief Remove a layer by id. This clears its name and entities but preserves indices.
         * @param id The ID of the layer.
         */
        void RemoveLayer(uint16_t id) {
            if (id >= m_idToName.size()) return;
            const std::string prev = m_idToName[id];
            if (!prev.empty()) m_nameToId.erase(prev);
            m_idToName[id].clear();
            if (id < m_idToEntities.size()) m_idToEntities[id].clear();
            if (id < m_visibility.size()) m_visibility[id] = true;
            if (id < m_locked.size()) m_locked[id] = false;
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
            if (m_idToName.size() <= id) m_idToName.resize(m_idToEntities.size());
            if (m_visibility.size() <= id) m_visibility.resize(m_idToEntities.size(), true);
            if (m_locked.size() <= id) m_locked.resize(m_idToEntities.size(), false);
        }

    private:
        std::unordered_map<std::string, uint16_t> m_nameToId;
        std::vector<std::string> m_idToName;
        std::vector<std::unordered_set<ECS::Entity, ECS::EntityHash>> m_idToEntities;
        std::vector<bool> m_visibility; // editor-only visibility flags per layer
        std::vector<bool> m_locked;     // editor-only lock flags per layer
    };
}

#endif
