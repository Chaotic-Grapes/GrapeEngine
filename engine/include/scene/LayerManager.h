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
#include <algorithm>
#include <utility>
#include "ecs/World.h"

namespace Scenes {
    // Stable layer identifier (never changes once assigned)
    using LayerId = uint16_t;

    /**
     * @brief Unified layer data structure containing all per-layer state.
     * Separates identity (LayerId) from ordering (m_drawOrder in LayerManager).
     */
    struct LayerData {
        // Identity & metadata
        std::string name;

        // Runtime toggles (affect physics, rendering, and logic)
        bool renderEnabled = true;   // If false, layer is not rendered
        bool updateEnabled = true;   // If false, logic updates skip this layer
        bool physicsEnabled = true;  // If false, physics simulation skips this layer

        // Editor-only flags (do not affect runtime)
        bool editorVisible = true;   // If false, hidden in editor viewport
        bool editorLocked = false;   // If false, layer cannot be selected/modified in editor

        // Physics collision filtering (32-bit mask)
        // Bit i set means this layer can collide with layer i
        uint32_t collisionMask = 0xFFFFFFFFu;

        // Entity membership for this layer
        std::unordered_set<ECS::Entity, ECS::EntityHash> entities;
    };

    class LayerManager {
    public:

        // Construct a LayerManager with some sane default layers pre-registered.
        // Default layers are assigned stable IDs and pre-filled into m_drawOrder.
        LayerManager() {
            _initializeDefaults();
        }

        // ====================================================================
        // Public API - Layer Creation & Management
        // ====================================================================

        /**
         * @brief Creates or retrieves a layer ID by its name.
         * @param name The name of the layer.
         * @return The ID of the layer.
         */
        uint16_t CreateOrGetLayer(const std::string& name) {
            auto it = m_nameToId.find(name);
            if (it != m_nameToId.end())
                return it->second;

            // Find next available slot
            uint16_t id = static_cast<uint16_t>(m_layers.size());
            _ensureCapacity(id);

            m_layers[id].name = name;
            m_nameToId[name] = id;
            m_drawOrder.push_back(id);

            return id;
        }

        /**
         * @brief Notify LayerManager when an entity is assigned to a layer.
         * @param e The entity being assigned.
         * @param id The layer ID.
         */
        void OnLayerSet(ECS::Entity e, uint16_t id) {
            _ensureCapacity(id);
            m_layers[id].entities.insert(e);
        }

        /**
         * @brief Notify LayerManager when an entity is removed from a layer.
         * @param e The entity being removed.
         * @param id The layer ID.
         */
        void OnLayerRemoved(ECS::Entity e, uint16_t id) {
            if (id < m_layers.size())
                m_layers[id].entities.erase(e);
        }

        /**
         * @brief Get all entities in a layer.
         * @param id The layer ID.
         * @return Reference to the entity set for this layer.
         */
        const std::unordered_set<ECS::Entity, ECS::EntityHash>& EntitiesIn(uint16_t id) const {
            static const std::unordered_set<ECS::Entity, ECS::EntityHash> EMPTY{};
            if (id >= m_layers.size())
                return EMPTY;

            return m_layers[id].entities;
        }

        /**
         * @brief Get the LayerData for a given layer ID.
         * @param id The layer ID.
         * @return Reference to the LayerData.
         */
        LayerData& Get(LayerId id) {
            if (id >= m_layers.size())
                _ensureCapacity(id);
            return m_layers[id];
        }

        /**
         * @brief Get the LayerData for a given layer ID (const version).
         * @param id The layer ID.
         * @return Const reference to the LayerData.
         */
        const LayerData& Get(LayerId id) const {
            static const LayerData EMPTY_LAYER{};
            if (id >= m_layers.size())
                return EMPTY_LAYER;
            return m_layers[id];
        }

        /**
         * @brief Get the render order of layers.
         * @return Reference to the vector of LayerIds in render order.
         */
        const std::vector<LayerId>& DrawOrder() const {
            return m_drawOrder;
        }

        /**
         * @brief Lists all registered layers.
         * @returns A vector of (id, name) for all registered layers.
         */
        std::vector<std::pair<uint16_t, std::string>> ListLayers() const {
            std::vector<std::pair<uint16_t, std::string>> out;
            for (uint16_t i = 0; i < m_layers.size(); ++i) {
                if (!m_layers[i].name.empty()) {
                    out.emplace_back(i, m_layers[i].name);
                }
            }
            return out;
        }

        /**
         * @brief Rename a layer by ID.
         * @param id The layer ID.
         * @param newName The new name for the layer.
         * @returns true on success.
         */
        bool RenameLayer(uint16_t id, const std::string& newName) {
            if (id >= m_layers.size())
                return false;

            const std::string& prev = m_layers[id].name;

            if (prev == newName)
                return true;
            if (!prev.empty())
                m_nameToId.erase(prev);

            m_layers[id].name = newName;
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
            if (!m_layers[id].name.empty())
                return false;

            m_layers[id].name = name;
            m_nameToId[name] = id;
            
            // Add to draw order if not already present
            auto it = std::find(m_drawOrder.begin(), m_drawOrder.end(), id);
            if (it == m_drawOrder.end())
                m_drawOrder.push_back(id);

            return true;
        }

        /**
         * @brief Remove a layer by id. This clears its name and entities but preserves indices.
         * @param id The layer ID.
         */
        void RemoveLayer(uint16_t id) {
            if (id >= m_layers.size())
                return;

            const std::string prev = m_layers[id].name;
            if (!prev.empty())
                m_nameToId.erase(prev);

            m_layers[id].name.clear();
            m_layers[id].entities.clear();
            m_layers[id].renderEnabled = true;
            m_layers[id].updateEnabled = true;
            m_layers[id].physicsEnabled = true;
            m_layers[id].editorVisible = true;
            m_layers[id].editorLocked = false;
            m_layers[id].collisionMask = 0xFFFFFFFFu;

            // Remove from draw order
            auto it = std::find(m_drawOrder.begin(), m_drawOrder.end(), id);
            if (it != m_drawOrder.end())
                m_drawOrder.erase(it);
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

        // ====================================================================
        // Editor API - Visibility & Locking
        // ====================================================================

        /**
         * @brief Mark a layer visible/invisible in the editor UI.
         * @param id The layer ID.
         * @param visible True to make visible, false to hide.
         */
        void SetVisibility(uint16_t id, bool visible) {
            _ensureCapacity(id);
            m_layers[id].editorVisible = visible;
        }

        /**
         * @brief Check if a layer is visible in the editor UI.
         * @param id The layer ID.
         * @return True if visible, false if hidden.
         */
        bool IsVisible(uint16_t id) const {
            if (id >= m_layers.size())
                return true;
            return m_layers[id].editorVisible;
        }

        /**
         * @brief Lock/unlock a layer to prevent modification from editor actions.
         * @param id The layer ID.
         * @param locked True to lock, false to unlock.
         */
        void SetLocked(uint16_t id, bool locked) {
            _ensureCapacity(id);
            m_layers[id].editorLocked = locked;
        }

        /**
         * @brief Check if a layer is locked.
         * @param id The layer ID.
         * @return True if locked, false if unlocked.
         */
        bool IsLocked(uint16_t id) const {
            if (id >= m_layers.size())
                return false;
            return m_layers[id].editorLocked;
        }

        // ====================================================================
        // Rendering API - Enable/Disable at Runtime
        // ====================================================================

        /**
         * @brief Set whether a layer is rendered.
         * @param id The layer ID.
         * @param enabled True to render, false to skip rendering.
         */
        void SetRenderEnabled(uint16_t id, bool enabled) {
            _ensureCapacity(id);
            m_layers[id].renderEnabled = enabled;
        }

        /**
         * @brief Check if a layer is rendered.
         * @param id The layer ID.
         * @return True if rendered, false otherwise.
         */
        bool IsRenderEnabled(uint16_t id) const {
            if (id >= m_layers.size())
                return false;
            return m_layers[id].renderEnabled;
        }

        // ====================================================================
        // Update API - Enable/Disable at Runtime
        // ====================================================================

        /**
         * @brief Set whether a layer's logic is updated.
         * @param id The layer ID.
         * @param enabled True to update, false to skip updates.
         */
        void SetUpdateEnabled(uint16_t id, bool enabled) {
            _ensureCapacity(id);
            m_layers[id].updateEnabled = enabled;
        }

        /**
         * @brief Check if a layer's logic is updated.
         * @param id The layer ID.
         * @return True if updated, false otherwise.
         */
        bool IsUpdateEnabled(uint16_t id) const {
            if (id >= m_layers.size())
                return false;
            return m_layers[id].updateEnabled;
        }

        // ====================================================================
        // Physics API - Enable/Disable & Collision Masks
        // ====================================================================

        /**
         * @brief Set whether a layer participates in physics simulation.
         * @param id The layer ID.
         * @param enabled True to simulate physics, false to skip.
         */
        void SetPhysicsEnabled(uint16_t id, bool enabled) {
            _ensureCapacity(id);
            m_layers[id].physicsEnabled = enabled;
        }

        /**
         * @brief Check if a layer participates in physics simulation.
         * @param id The layer ID.
         * @return True if physics enabled, false otherwise.
         */
        bool IsPhysicsEnabled(uint16_t id) const {
            if (id >= m_layers.size())
                return false;
            return m_layers[id].physicsEnabled;
        }

        /**
         * @brief Get the collision mask for a layer.
         * @param id The layer ID.
         * @return The collision mask.
         */
        uint32_t GetLayerMask(uint16_t id) const {
            if (id >= m_layers.size())
                return 0xFFFFFFFFu;
            return m_layers[id].collisionMask;
        }

        /**
         * @brief Set the collision mask for a layer and sync all colliders in that layer.
         * @param id The layer ID.
         * @param mask The new collision mask.
         * @param world Optional pointer to ECS World for syncing collider components.
         *             If nullptr, only the layer mask is updated (no collider sync).
         */
        void SetLayerMask(uint16_t id, uint32_t mask, ECS::World* world = nullptr) {
            _ensureCapacity(id);
            m_layers[id].collisionMask = mask;
            
            // If a world is provided, sync all colliders in this layer with the new mask
            if (world != nullptr) {
                _syncCollidersForLayer(id, world);
            }
        }
        
        /**
         * @brief Update all colliders in a layer to use the layer's current collision mask.
         * Called automatically by SetLayerMask when a world is provided, but can be called
         * manually if needed to force synchronization.
         */
        void _syncCollidersForLayer(uint16_t layerId, ECS::World* world) {
            if (!world || layerId >= m_layers.size())
                return;
            
            uint32_t layerMask = m_layers[layerId].collisionMask;
            
            // Update all entities in this layer
            for (ECS::Entity entity : m_layers[layerId].entities) {
                if (!world->IsAlive(entity))
                    continue;
                
                // Update circle colliders
                if (world->Has<ECS::Components::CircleCollider2D>(entity)) {
                    auto& circle = world->Get<ECS::Components::CircleCollider2D>(entity);
                    circle.LayerMask = layerMask;
                }
                
                // Update box colliders
                if (world->Has<ECS::Components::BoxCollider2D>(entity)) {
                    auto& box = world->Get<ECS::Components::BoxCollider2D>(entity);
                    box.LayerMask = layerMask;
                }
            }
        }

        /**
         * @brief Set or clear collision between two layers. This updates both
         * layers' masks symmetrically so the collision rule is consistent.
         * @param a First layer id
         * @param b Second layer id
         * @param enabled True to enable collision, false to disable
         */
        void SetCollisionBetween(uint16_t a, uint16_t b, bool enabled) {
            // ensure capacity for both indices
            _ensureCapacity(a);
            _ensureCapacity(b);

            if (a < 32 && b < 32) {
                const uint32_t bitA = (1u << a);
                const uint32_t bitB = (1u << b);
                if (enabled) {
                    m_layers[a].collisionMask |= bitB;
                    m_layers[b].collisionMask |= bitA;
                }
                else {
                    m_layers[a].collisionMask &= ~bitB;
                    m_layers[b].collisionMask &= ~bitA;
                }
            }
        }

        /**
         * @brief Check if two layers can collide based on their collision masks.
         * @param a First layer ID.
         * @param b Second layer ID.
         * @return True if layer a can collide with layer b.
         */
        bool CanCollide(uint16_t a, uint16_t b) const {
            if (a >= m_layers.size() || b >= m_layers.size())
                return false;
            if (b >= 32) // Collision mask is only 32 bits
                return false;
            return (m_layers[a].collisionMask & (1u << b)) != 0;
        }

        // ====================================================================
        // Reordering API
        // ====================================================================

        /**
         * @brief Move a layer in the draw order.
         * Layer IDs never change; only the rendering order changes.
         * @param fromIndex The current index in m_drawOrder.
         * @param toIndex The target index in m_drawOrder.
         * @returns true on success.
         */
        bool MoveDrawOrder(size_t fromIndex, size_t toIndex) {
            if (fromIndex >= m_drawOrder.size() || toIndex >= m_drawOrder.size())
                return false;
            if (fromIndex == toIndex)
                return true;

            // Rotate the draw order
            if (fromIndex < toIndex) {
                std::rotate(m_drawOrder.begin() + fromIndex,
                           m_drawOrder.begin() + fromIndex + 1,
                           m_drawOrder.begin() + toIndex + 1);
            }
            else {
                std::rotate(m_drawOrder.begin() + toIndex,
                           m_drawOrder.begin() + fromIndex,
                           m_drawOrder.begin() + fromIndex + 1);
            }
            return true;
        }

        /**
         * @brief (Deprecated: for compatibility) Move a layer entry from one index to another.
         * This is now a draw order operation only - layer IDs are stable.
         * @param fromId The layer ID to move.
         * @param toId The target position (as an index in draw order).
         * @returns true on success.
         */
        bool MoveLayer(uint16_t fromId, uint16_t toId) {
            // Find the current position of fromId in draw order
            auto fromIt = std::find(m_drawOrder.begin(), m_drawOrder.end(), fromId);
            if (fromIt == m_drawOrder.end())
                return false;

            size_t fromIndex = std::distance(m_drawOrder.begin(), fromIt);
            return MoveDrawOrder(fromIndex, toId);
        }

        /**
         * @brief Swap contents of two layer slots (all data including entities and flags).
         * This is an atomic swap that preserves indices but exchanges the layers' data.
         * @param aId First layer id
         * @param bId Second layer id
         * @returns true on success
         */
        bool SwapLayers(uint16_t aId, uint16_t bId) {
            if (aId >= m_layers.size() || bId >= m_layers.size())
                return false;
            if (aId == bId)
                return true;

            std::swap(m_layers[aId], m_layers[bId]);

            // Rebuild name->id map (names have moved)
            m_nameToId.clear();
            for (uint16_t i = 0; i < m_layers.size(); ++i) {
                if (!m_layers[i].name.empty())
                    m_nameToId[m_layers[i].name] = i;
            }

            return true;
        }

        /**
         * @brief Reset layers to the default well-known set.
         * This clears any user-created layers and re-populates the default
         * layers with their standard IDs, names and default masks/flags.
         */
        void ResetToDefaults() {
            m_nameToId.clear();
            m_layers.clear();
            m_drawOrder.clear();
            _initializeDefaults();
        }

    private:
        /**
         * @brief Initialize default layers (called in constructor and ResetToDefaults).
         */
        void _initializeDefaults() {
            constexpr uint16_t DefaultCapacity = 100;
            m_layers.resize(DefaultCapacity);
            
            // Register well-known default layers with stable IDs
            constexpr uint16_t BackgroundLayer = 0;
            constexpr uint16_t WorldLayer = 1;
            constexpr uint16_t GameplayLayer = 2;
            constexpr uint16_t ForegroundLayer = 3;
            constexpr uint16_t UILayer = 98;
            constexpr uint16_t DebugLayer = 99;

            // Initialize Background
            m_layers[BackgroundLayer].name = "Background";
            m_nameToId["Background"] = BackgroundLayer;
            m_drawOrder.push_back(BackgroundLayer);

            // Initialize World
            m_layers[WorldLayer].name = "World";
            m_nameToId["World"] = WorldLayer;
            m_drawOrder.push_back(WorldLayer);

            // Initialize Gameplay
            m_layers[GameplayLayer].name = "Gameplay";
            m_nameToId["Gameplay"] = GameplayLayer;
            m_drawOrder.push_back(GameplayLayer);

            // Initialize Foreground
            m_layers[ForegroundLayer].name = "Foreground";
            m_nameToId["Foreground"] = ForegroundLayer;
            m_drawOrder.push_back(ForegroundLayer);

            // Initialize UI
            m_layers[UILayer].name = "UI";
            m_nameToId["UI"] = UILayer;
            m_drawOrder.push_back(UILayer);

            // Initialize Debug
            m_layers[DebugLayer].name = "Debug";
            m_nameToId["Debug"] = DebugLayer;
            m_drawOrder.push_back(DebugLayer);
        }

        void _ensureCapacity(uint16_t id) {
            while (m_layers.size() <= id) {
                m_layers.emplace_back();
            }
        }

        // Member variables
        std::unordered_map<std::string, LayerId> m_nameToId;    // name -> stable layer ID
        std::vector<LayerData> m_layers;                         // all layer data, indexed by LayerId (stable)
        std::vector<LayerId> m_drawOrder;                        // render order (reorderable)
    };
}

#endif
