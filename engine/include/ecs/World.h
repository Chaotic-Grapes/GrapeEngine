/* Start Header *****************************************************************/
/*!
\file    World.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the World class, the central
manager for the ECS architecture. Features persistent query caching that survives
archetype creation, archetype graph edges for O(1) component transitions, and
optimized iteration with pre-computed component indices and strides.

Provides entity lifecycle management, hierarchical relationships, and efficient
Each() queries with template-based static caching for maximum performance.

This class is mostly used by developers and would be considered the most
important part of the ECS architecture.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <type_traits>
#include <array>
#include <utility>
#include <cassert>
#include <functional>
#include <algorithm>
#include "ecs/Entity.h"
#include "ecs/Archetype.h"
#include "ecs/Components.h"
#include "ecs/Signature.h"
#include "ecs/ComponentRegistry.h"

#include "math/Matrix4x4.h"

namespace ECS {
    // Forward declarations
    class PrefabManager;

    // Relationship component for hierarchy
    // Move this to Components.h?
    struct Parent { Entity ParentEntity{NULL_ENTITY}; };

    // Options for cloning an entity
    struct CloneOptions {
    public:
        // If true, keep Parent as-is.
        // Otherwise detach cloned entities from their parents
        bool KeepParent = false;
        // Keep Layer component (if present)
        bool KeepLayer = true;
        // Keep Name component (if present)
        bool KeepName = true;
    };

    // Serialized component data for snapshot-based undo/redo
    struct SerializedComponent {
        ComponentTypeId Id;                 // Component type identifier
        std::vector<uint8_t> Data;          // Raw component data
    };

    // World holds all archetypes and entity allocation, moves entities across archetypes on structural changes.
    class World {
    public:
        World() = default;
        
        // Delete copy constructor and copy assignment operator (non-copyable due to unique_ptr)
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        
        // Default move constructor and move assignment operator
        World(World&&) noexcept = default;
        World& operator=(World&&) noexcept = default;

        // ************** Entity Lifecycle ************** //

        /**
		 * @brief Create an empty entity in the world.
		 * @return ECS::Entity The newly created entity
         */
        Entity Create() {
            EntityId idx;
            // If with identical then and else branches [bugprone-branch-close]

            // Reuse from free list if available
            if (!m_free.empty()) {
                // Sort to always reuse lowest index first
                std::sort(m_free.begin(), m_free.end());
                // Takes from FRONT (lowest index)
                idx = m_free.front();
                m_free.erase(m_free.begin());
            }
            else {
                // Create new entity based on current size of generations vector
                idx = static_cast<EntityId>(m_generations.size());

                // Expand the generations vector and locations vector
                // Push back because new entity index is at the end
                // For locations, we initialize with default Location
                m_generations.push_back(0);
                m_locations.push_back(Location{});
            }

            return Entity{ idx, m_generations[idx] };
        }

        /**
		 * @brief Create an entity with a specific ID (for state restoration).
		 * This allows perfect ID preservation when restoring destroyed entities.
		 * @param targetId The desired entity ID
		 * @return ECS::Entity The created entity with the specified ID, or a new entity if ID cannot be reused
         * @warning Do not use this function unless you know what you're doing! This is meant for advanced use cases like state restoration.
         */
        Entity CreateWithId(EntityId targetId) {
            // Ensure generations and locations vectors are large enough
            if (targetId >= m_generations.size()) {
                m_generations.resize(targetId + 1, 0);
                m_locations.resize(targetId + 1, Location{});
            }

            // Check if this ID is in the free list (entity was destroyed)
            auto it = std::find(m_free.begin(), m_free.end(), targetId);
            if (it != m_free.end()) {
                // Remove from free list and reuse with current generation
                m_free.erase(it);
                return Entity{ targetId, m_generations[targetId] };
            }

            // Check if entity with this ID is already alive
            Entity testEntity{ targetId, m_generations[targetId] };
            if (IsAlive(testEntity)) {
                // Cannot reuse this ID - fall back to normal Create()
                return Create();
            }

            // ID is available, use it
            return Entity{ targetId, m_generations[targetId] };
        }

        /**
		 * @brief Create an entity with the specified components in the world.
		 * @tparam Ts Component types to add to the entity
		 * @param comps Component instances to add to the entity
		 * @return ECS::Entity The newly created entity with the specified components
         */
        template<typename... Ts>
        Entity Create(const Ts&... comps) {
            // Basically the same as Create() + Add() for each component, but more efficient

            Entity e = Create();
            _emplaceComponents(e, comps...);

            return e;
        }

        /**
		 * @brief Check if the given entity is alive (valid) in the world.
		 * @param e The entity to check
		 * @return bool True if the entity is alive; false otherwise
         */
        bool IsAlive(const Entity e) const {
            // An entity is considered alive if:
            // - its index is within the valid range,
            // - its generation number matches the current generation, and
            // - it currently resides in an archetype (i.e., has a valid location).
            if (e.Index >= m_generations.size()) return false;
            if (m_generations[e.Index] != e.Generation) return false;
            return m_locations[e.Index].ArchetypePtr != nullptr;
        }

        /**
         * @brief Resolve an entity id to a valid Entity handle with current generation.
         * @param id The entity index/id.
         * @return ECS::Entity with up-to-date generation, or NULL_ENTITY if out of range.
         */
        Entity Resolve(const EntityId id) const {
            if (id >= m_generations.size()) {
                return NULL_ENTITY;
            }
            return Entity{ id, m_generations[id] };
        }

        /**
		 * @brief Destroy the specified entity, removing it from the world.
		 * @param e The entity to destroy
         * @note Assumes the entity is alive, behavior is undefined for dead entities.
         */
        void Destroy(const Entity e) {
            // Assume entity is alive
            // If entity participates in hierarchy, unlink it first
            if (Has<Parent>(e)) {
                _onComponentRemoving(e, TypeIdOf<Parent>());
            }

            // Lookup location and remove from archetype if present
            auto &loc = m_locations[e.Index];
            if (loc.ArchetypePtr) {
                _removeFromArchetype(e, loc);
            }

            // Increment generation to invalidate existing handles
            ++m_generations[e.Index];

            // Add to free list for reuse
            m_free.push_back(e.Index);
        }

        // ************** Component API ************** //

        /**
		 * @brief Check if the specified entity has the given component type.
		 * @tparam T The component type to check for
		 * @param e The entity to check
		 * @return bool True if the entity has the component; false otherwise
         * @note Assumes the entity is alive
         */
        template<typename T>
        bool Has(const Entity e) const {
            // Assume entity is alive

            // Same as Destroy() but returns false if no archetype
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr)
                return false;

            // Otherwise, check if archetype has the component
            return loc.ArchetypePtr->Has(TypeIdOf<T>());
        }

        /**
		 * @brief Get a reference to the specified component type for the given entity.
		 * @tparam T The component type to retrieve
		 * @param e The entity from which to retrieve the component
		 * @return T& Reference to the requested component
         * @note Assumes the entity is alive
         */
        template<typename T>
        inline T& Get(const Entity e) {
            // Assume entity is alive

            // Lookup location, as always
            auto& loc = m_locations[e.Index];

            // Get raw pointer from archetype and cast to T&
            return *static_cast<T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }

        /**
		 * @brief Get a const reference to the specified component type for the given entity.
		 * @tparam T The component type to retrieve
		 * @param e The entity from which to retrieve the component
		 * @return const T& Const reference to the requested component
         * @note Assumes the entity is alive
         */
        template<typename T>
        inline const T& Get(const Entity e) const {
            // Assume entity is alive

            auto& loc = m_locations[e.Index];

            // Same but to const T&
            return *static_cast<const T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }

        /**
		 * @brief Try to get a pointer to the specified component type for the given entity.
		 * @tparam T The component type to retrieve
		 * @param e The entity from which to retrieve the component
		 * @return T* Pointer to the component if it exists; nullptr otherwise
		 * 
		 * @note This combines Has() and Get() into a single operation, eliminating redundant lookups.
		 * @note Prefer this over Has() + Get() pattern for better performance.
         * @note Assumes the entity is alive
         */
        template<typename T>
        T* TryGet(const Entity e) {
            // Assume entity is alive

            // Similar to Has() but returns nullptr if not found
            // Otherwise, get the raw pointer and cast to T*
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr || !loc.ArchetypePtr->Has(TypeIdOf<T>()))
                return nullptr;

            return static_cast<T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }

        /**
		 * @brief Try to get a const pointer to the specified component type for the given entity.
		 * @tparam T The component type to retrieve
		 * @param e The entity from which to retrieve the component
		 * @return const T* Const pointer to the component if it exists; nullptr otherwise
		 * 
		 * @note This combines Has() and Get() into a single operation, eliminating redundant lookups.
		 * @note Prefer this over Has() + Get() pattern for better performance.
         * @note Assumes the entity is alive
         */
        template<typename T>
        const T* TryGet(const Entity e) const {
            // Assume entity is alive

            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr || !loc.ArchetypePtr->Has(TypeIdOf<T>()))
                return nullptr;

            return static_cast<const T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }

        /**
		 * @brief Try to get pointers to multiple components for the given entity.
		 * @tparam Ts The component types to retrieve
		 * @param e The entity from which to retrieve the components
		 * @return std::tuple<Ts*...> Tuple of pointers to the components; any pointer may be nullptr if the component doesn't exist
		 * 
		 * @note This retrieves multiple components in a single call with optimized validation.
		 * @note Check each pointer for nullptr before use.
         * @note Assumes the entity is alive
         */
        template<typename... Ts>
        std::tuple<Ts*...> TryGetComponents(const Entity e) {
            // Assume entity is alive

            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr)
                return std::tuple<Ts*...>{ static_cast<Ts*>(nullptr)... };

            // Check if all components exist first
            const bool hasAll = (loc.ArchetypePtr->Has(TypeIdOf<Ts>()) && ...);
            if (!hasAll)
                return std::tuple<Ts*...>{ static_cast<Ts*>(nullptr)... };

            // All exist, retrieve them
            return std::tuple<Ts*...>{
                static_cast<Ts*>(loc.ArchetypePtr->GetRaw(TypeIdOf<Ts>(), loc.ChunkIndex, loc.SlotIndex))...
            };
        }

        /**
		 * @brief Try to get const pointers to multiple components for the given entity.
		 * @tparam Ts The component types to retrieve
		 * @param e The entity from which to retrieve the components
		 * @return std::tuple<const Ts*...> Tuple of const pointers to the components; any pointer may be nullptr if the component doesn't exist
		 * 
		 * @note This retrieves multiple components in a single call with optimized validation.
		 * @note Check each pointer for nullptr before use.
         * @note Assumes the entity is alive
         */
        template<typename... Ts>
        std::tuple<const Ts*...> TryGetComponents(const Entity e) const {
			// Assume entity is alive

            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr)
                return std::tuple<const Ts*...>{ static_cast<const Ts*>(nullptr)... };

            // Check if all components exist first
            const bool hasAll = (loc.ArchetypePtr->Has(TypeIdOf<Ts>()) && ...);
            if (!hasAll)
                return std::tuple<const Ts*...>{ static_cast<const Ts*>(nullptr)... };

            // All exist, retrieve them
            return std::tuple<const Ts*...>{
                static_cast<const Ts*>(loc.ArchetypePtr->GetRaw(TypeIdOf<Ts>(), loc.ChunkIndex, loc.SlotIndex))...
            };
        }

        /**
		 * @brief Add a component of the specified type to the given entity, constructing it with the provided arguments.
		 * @tparam T The component type to add
		 * @tparam TArgs The types of the constructor arguments for the component
		 * @param e The entity to which the component will be added
		 * @param args Constructor arguments for the component
		 * @return T& Reference to the newly added component
         * @note Assumes the entity is alive
         */
        template<typename T, typename... TArgs>
        inline T& Add(Entity e, TArgs&&... args) {
            // Assume entity is alive

            // Set up mover lambda to construct T in-place
            auto t = TypeIdOf<T>();

            // Capture arguments by value
            auto mover = [&, this](Entity, void* dstArchetypeSlot) {
                new (dstArchetypeSlot) T(std::forward<TArgs>(args)...);
            };

            // Perform structural add
            // This will call the mover to construct the component in the new location
            // Then call _onComponentAdded hook afterwards
            _structuralAdd<T>(e, t, mover);
            _onComponentAdded(e, t); // Notify after addition

            // Return reference to the newly added component
            return Get<T>(e);
        }

        /**
		 * @brief Remove the specified component type from the given entity.
		 * @tparam T The component type to remove
		 * @param e The entity from which the component will be removed
         * @note Assumes the entity is alive
         */
        template<typename T>
        inline void Remove(Entity e) {
			// Assume entity has the component

            // Same as Add<T> but for removal
            auto t = TypeIdOf<T>();
            _onComponentRemoving(e, t); // Notify before removal
            _structuralRemove<T>(e, t); // Perform structural remove
        }

        /**
		 * @brief Set the specified component type for the given entity, adding it if not already present.
		 * @tparam T The component type to set
		 * @param e The entity for which the component will be set
		 * @param value The new value for the component
		 * @return T& Reference to the set component
         * @note Assumes the entity is alive
         */
        template<typename T>
        T& Set(Entity e, T value) {
            // Assume entity is alive

            // If entity has component, update it
            if (Has<T>(e)) {
                // Update existing component by using move semantics
                Get<T>(e) = std::move(value);

                // Then notify change before returning
                _onComponentChanged(e, TypeIdOf<T>());
                return Get<T>(e);
            }

            // Otherwise, add new component
            // Again using move semantics
            return Add<T>(e, std::move(value));
        }

        // ************** Batch Operations ************** //

        /**
         * @brief Begin a batch operation to defer archetype version increments.
         * 
         * Useful when performing multiple structural changes (add/remove components)
         * on many entities. Defers query cache invalidation until EndBatch() is called.
         * 
         * @note Batch operations can be nested (ref-counted)
         */
        void BeginBatch() {
            if (m_batchDepth == 0) {
                m_batchStartVersion = m_archetypeVersion;
            }
            ++m_batchDepth;
        }

        /**
         * @brief End a batch operation.
         * 
         * When the outermost batch ends (depth reaches 0), invalidates query caches
         * if any archetypes were created during the batch.
         */
        void EndBatch() {
            if (m_batchDepth > 0) {
                --m_batchDepth;
                // If depth reached 0, query caches will be invalidated on next Each() call
                // since cacheVersion won't match m_archetypeVersion
            }
        }

        /**
         * @brief Check if currently inside a batch operation.
         * @return True if BeginBatch() was called more times than EndBatch()
         */
        bool IsInBatch() const { return m_batchDepth > 0; }

        // ************** Entity State Capture/Restore ************** //

        /**
         * @brief Capture all components of an entity as serialized data.
         * 
         * Useful for snapshot-based undo/redo. Captures component data in the order
         * they appear in the entity's archetype.
         * 
         * @param e The entity to capture
         * @return Vector of serialized components; empty if entity is not alive
         * @note Assumes the entity is alive
         */
        std::vector<SerializedComponent> CaptureEntityComponents(Entity e) const {
            std::vector<SerializedComponent> result;
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr) return result;

            for (const auto& info : loc.ArchetypePtr->GetComponents()) {
                const void* ptr = loc.ArchetypePtr->GetRaw(info.Id, loc.ChunkIndex, loc.SlotIndex);
                SerializedComponent sc;
                sc.Id = info.Id;
                sc.Data.resize(info.Size);
                std::memcpy(sc.Data.data(), ptr, info.Size);
                result.push_back(std::move(sc));
            }
            return result;
        }

        /**
         * @brief Restore an entity's components from a serialized snapshot.
         * 
         * Removes components not in the snapshot and adds/updates components from the snapshot.
         * 
         * @param e The entity to restore
         * @param comps The serialized component snapshot
         * @note Does nothing if entity is not alive
         */
        void RestoreEntityComponents(Entity e, const std::vector<SerializedComponent>& comps) {
            if (!IsAlive(e)) return;

            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr) return;

            // Collect IDs to restore
            std::unordered_set<ComponentTypeId> restoreIds;
            for (const auto& sc : comps) {
                restoreIds.insert(sc.Id);
            }

            // Remove components not in snapshot
            std::vector<ComponentTypeId> toRemove;
            for (const auto& info : loc.ArchetypePtr->GetComponents()) {
                if (restoreIds.find(info.Id) == restoreIds.end()) {
                    toRemove.push_back(info.Id);
                }
            }
            for (auto id : toRemove) {
                RemoveById(e, id);
            }

            // Add/update components from snapshot
            for (const auto& sc : comps) {
                AddComponentById(e, sc.Id, (void*)sc.Data.data(), sc.Data.size());
            }
        }

        // ************** Entity Introspection ************** //

        /**
         * @brief Get all component IDs currently on an entity.
         * 
         * Useful for editor inspection or generic entity operations.
         * 
         * @param e The entity to inspect
         * @return Vector of component type IDs; empty if entity is not alive
         * @note Assumes the entity is alive
         */
        std::vector<ComponentTypeId> GetEntityComponents(Entity e) const {
            std::vector<ComponentTypeId> result;
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr) return result;

            for (const auto& info : loc.ArchetypePtr->GetComponents()) {
                result.push_back(info.Id);
            }
            return result;
        }

        // ************** Non-Templated Component API (for C# interop) ************** //

        /**
         * @brief Get archetypes matching a signature (for query iteration)
         * @param sig The signature to match
         * @return Vector of matching archetypes
         */
        inline const std::vector<Archetype*>& GetMatchingArchetypes(const Signature& sig) const {
            return _getMatchingArchetypes(sig);
        }

        /**
         * @brief Check if entity has a component by ComponentId
         * @param e The entity to check
         * @param componentId The component type ID
         * @return True if entity has component
         */
        inline bool HasById(Entity e, ComponentTypeId componentId) const {
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr)
                return false;
            return loc.ArchetypePtr->Has(componentId);
        }

        /**
         * @brief Get raw component pointer by ComponentId
         * @param e The entity
         * @param componentId The component type ID
         * @return Pointer to component data, or nullptr if not found
         */
        inline void* GetRawComponentPtr(Entity e, ComponentTypeId componentId) {
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr || !loc.ArchetypePtr->Has(componentId))
                return nullptr;
            return loc.ArchetypePtr->GetRaw(componentId, loc.ChunkIndex, loc.SlotIndex);
        }

        /**
         * @brief Add a component by ComponentId with raw data
         * @param e The entity
         * @param componentId The component type ID
         * @param data Pointer to component data to copy (can be nullptr for default construction)
         * @param size Size of component data
         * @return Pointer to the added component
         */
        inline void* AddComponentById(Entity e, ComponentTypeId componentId, void* data, size_t size) {
            // Ensure component info exists
            if (m_componentSizes.find(componentId) == m_componentSizes.end()) {
                // Try to get metadata from ComponentRegistry (for C# components)
                const auto& meta = ComponentRegistry::Meta(componentId);
                if (meta.Size == 0) {
                    return nullptr; // Component type not registered
                }
                
                // Register this component with the world
                size_t alignment = meta.Align > 0 ? meta.Align : meta.Size;
                m_componentSizes[componentId] = { meta.Size, alignment };
                LOG_INFO("[World::AddComponentById] Auto-registered component ID " << componentId << " (size=" << meta.Size << ", align=" << alignment << ")");
            }

            auto& loc = m_locations[e.Index];
            
            // If entity has no archetype, create new one with just this component
            if (!loc.ArchetypePtr) {
                const Signature ns({ componentId });
                Archetype* to = _getOrCreateArchetype(ns);
                auto [ci, slot] = to->Insert();
                
                void* componentPtr = to->GetRaw(componentId, ci, slot);
                if (data && size > 0) {
                    std::memcpy(componentPtr, data, size);
                } else {
                    const auto& meta = ComponentRegistry::Meta(componentId);
                    if (meta.ctor) meta.ctor(componentPtr);
                }
                
                _placeEntity(e, to, ci, slot);
                _onComponentAdded(e, componentId);
                return componentPtr;
            }
            
            // If entity already has this component, just return pointer
            if (loc.ArchetypePtr->Has(componentId)) {
                return loc.ArchetypePtr->GetRaw(componentId, loc.ChunkIndex, loc.SlotIndex);
            }
            
            // Move to new archetype with added component
            Archetype* from = loc.ArchetypePtr;
            Archetype* to = from->GetAddEdge(componentId);
            
            if (!to) {
                const Signature ns = from->GetSignature().MergedWith(componentId);
                to = _getOrCreateArchetype(ns);
                from->SetAddEdge(componentId, to);
            }
            
            auto [ci, slot] = to->Insert();
            
            // Copy existing components
            for (auto& info : from->GetComponents()) {
                if (info.Id == componentId) continue;
                void* dst = to->GetRaw(info.Id, ci, slot);
                const void* src = from->GetRaw(info.Id, loc.ChunkIndex, loc.SlotIndex);
                std::memcpy(dst, src, info.Size);
            }
            
            // Initialize new component
            void* componentPtr = to->GetRaw(componentId, ci, slot);
            if (data && size > 0) {
                std::memcpy(componentPtr, data, size);
            } else {
                const auto& meta = ComponentRegistry::Meta(componentId);
                if (meta.ctor) meta.ctor(componentPtr);
            }
            
            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
            _onComponentAdded(e, componentId);
            
            return componentPtr;
        }

        /**
         * @brief Register component size/alignment info for a component id.
         * This is used by C# interop to inform an existing World about
         * synthetic (C#) component types so that archetypes can be created
         * correctly when components are added via AddComponentById.
         */
        inline void RegisterComponentSizeById(ComponentTypeId id, size_t size, size_t align) {
            if (m_componentSizes.find(id) == m_componentSizes.end()) {
                m_componentSizes[id] = { size, align };
            }
        }

        /**
         * @brief Remove a component by ComponentId
         * @param e The entity
         * @param componentId The component type ID
         */
        inline void RemoveById(Entity e, ComponentTypeId componentId) {
            auto& loc = m_locations[e.Index];
            
            if (!loc.ArchetypePtr || !loc.ArchetypePtr->Has(componentId)) {
                return; // Entity doesn't have this component
            }
            
            _onComponentRemoving(e, componentId);
            
            Archetype* from = loc.ArchetypePtr;
            Archetype* to = from->GetRemoveEdge(componentId);
            
            if (!to) {
                const Signature ns = from->GetSignature().Without(componentId);
                if (ns.Types().empty()) {
                    to = nullptr;
                } else {
                    to = _getOrCreateArchetype(ns);
                }
                from->SetRemoveEdge(componentId, to);
            }
            
            const uint32_t fromChunk = loc.ChunkIndex;
            const uint32_t fromSlot = loc.SlotIndex;
            
            // If no components left, just remove from archetype
            if (!to) {
                _removeFromArchetype(e, loc);
                loc.ArchetypePtr = nullptr;
                return;
            }
            
            // Copy all components except the one being removed
            auto [ci, slot] = to->Insert();
            for (auto& info : from->GetComponents()) {
                if (info.Id == componentId) continue;
                void* dst = to->GetRaw(info.Id, ci, slot);
                const void* src = from->GetRaw(info.Id, fromChunk, fromSlot);
                std::memcpy(dst, src, info.Size);
            }
            
            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
        }

        /**
		 * @brief Iterate over all entities that have the specified components, invoking the provided function for each.
		 * @tparam Ts The component types to filter entities by
		 * @tparam TFn The type of the function to invoke for each matching entity
		 * @param fn The function to invoke for each matching entity; should accept parameters (Entity, Ts&...)
         */
        template<typename... Ts, typename TFn>
        void Each(TFn&& fn) {
            // This will be a lengthy comment explaining what the function does
            // It iterates over all entities that have the specified components and invokes the provided function for each.
            // ^ This is a brief overview of the function's purpose
            // Each() is basically a lambda-based query system that allows users to define operations on entities

            // Get number of components at compile-time (optimization effort but unsure if it helps)
            constexpr size_t numComponents = sizeof...(Ts);

            // If no components specified, iterate all entities in all archetypes
            // This allows calls like world.Each(fn) to visit every entity.
            if constexpr (numComponents == 0) {
                for (const auto& kv : m_archetypes) {
                    const auto& archPtr = kv.second;
                    if (!archPtr) continue;
                    Archetype* arch = archPtr.get();

                    const uint32_t chunkCount = arch->GetChunkCount();
                    for (uint32_t ci = 0; ci < chunkCount; ++ci) {
                        Chunk* ch = arch->GetChunk(ci);
                        const uint32_t count = ch->Count();
                        for (uint32_t i = 0; i < count; ++i) {
                            const Entity ent = ch->GetEntity(i);
                            if (!ent.IsNull()) fn(ent);
                        }
                    }
                }
                return;
            }
            else { // Need to add `else` to suppress "unreachable code" warning (https://stackoverflow.com/questions/52244640/if-constexpr-and-c4702-and-c4100-and-c4715)
                // Some optimization efforts:
                // Use compile-time query ID to cache archetype list pointer per unique component set
                // Should be faster than unordered_map because template instantiation happens at compile-time
                using QueryTag = std::tuple<std::decay_t<Ts>...>; // std::decay_t removes references and cv-qualifiers from types
                static const std::vector<Archetype*>* cached = nullptr;
                static uint64_t cacheVersion = 0;  // Track which archetype version we last processed
                static std::array<TypeId, numComponents> typeIds = { 0 };
                
                // Check only archetypes added since last version
                if (cached == nullptr || cacheVersion != m_archetypeVersion) {
                    // First time initialization - get component type IDs at runtime
                    if (cached == nullptr) {
                        typeIds = { TypeIdOf<std::decay_t<Ts>>()... };
                    }

                    // Build signature for the requested components
                    const Signature req(std::vector<TypeId>(typeIds.begin(), typeIds.end()));

                    // Get matching archetypes and update cache
                    cached = &_getMatchingArchetypes(req);
                    
                    // Update version to current
                    cacheVersion = m_archetypeVersion;
                }
                const auto& matched = *cached; // Get cached archetype list (shouldn't be null here either way)
                
                // Iterate through all matched archetypes
                for (Archetype* arch : matched) {
                    if (!arch) continue;

                    // Precompute component indices for this archetype to avoid repeated lookups
                    // This avoids repeated lookups during iteration
                    std::array<uint32_t, numComponents> compIdxs;
                    for (size_t k = 0; k < numComponents; ++k) {
                        // Get the component index for this type in the archetype
                        // Store it in the precomputed array
                        compIdxs[k] = arch->GetComponentIndex(typeIds[k]);
                    }

                    // Get number of chunks in the archetype
                    const uint32_t chunkCount = arch->GetChunkCount();
                    
                    // Iterate through all chunks in this archetype
                    for (uint32_t ci = 0; ci < chunkCount; ++ci) {
                        // Get the chunk pointer then its entity count
                        Chunk* ch = arch->GetChunk(ci);
                        const uint32_t count = ch->Count();
                        
                        // Skip empty chunks
                        if (count == 0)
                            continue;
                        
                        // Get base pointers to component arrays and their strides
                        std::array<uint8_t*, numComponents> componentBases;
                        std::array<size_t, numComponents> strides;
                        
                        // Populate them with data from chunk
                        for (size_t pi = 0; pi < numComponents; ++pi) {
                            // ComponentBase() gets start of component array in chunk's memory buffer
                            componentBases[pi] = static_cast<uint8_t*>(ch->ComponentBase(compIdxs[pi]));
                            
                            // Get stride from archetype instead of chunk to avoid indirect lookup
                            // Also note: all chunks in an archetype have identical layout,
                            // so cache at archetype level
                            strides[pi] = arch->GetComponentStride(compIdxs[pi]);
                        }
                        
                        // Get entity array pointer for direct access
                        const Entity* entities = ch->Entities().data();

                        // Now iterate through all entities in the chunk
                        for (uint32_t i = 0; i < count; ++i) {
                            // Invoke the lambda expression with the entity and its components
                            // This uses a helper function to unpack the component pointers
                            // std::index_sequence_for is used to generate index sequences for parameter packs
                            _invokeWithComponents<Ts...>(fn, entities[i], componentBases, strides, i, std::index_sequence_for<Ts...>{});
                        }
                    }
                }
            }
        }

        /**
		 * @brief Iterate over all entities that have the specified components, invoking the provided function for each (const version).
		 * @tparam Ts The component types to filter entities by
		 * @tparam TFn The type of the function to invoke for each matching entity
		 * @param fn The function to invoke for each matching entity; should accept parameters (Entity, const Ts&...)
         */
        template<typename... Ts, typename TFn>
        void Each(TFn&& fn) const {
            // Similar to non-const Each(), but operates on const components

            constexpr size_t numComponents = sizeof...(Ts);

            if constexpr (numComponents == 0) {
                // Iterate all entities across all archetypes
                for (const auto& kv : m_archetypes) {
                    const auto& archPtr = kv.second;
                    if (!archPtr) continue;
                    const Archetype* arch = archPtr.get();

                    const uint32_t chunkCount = arch->GetChunkCount();
                    for (uint32_t ci = 0; ci < chunkCount; ++ci) {
                        const Chunk* ch = arch->GetChunk(ci);
                        const uint32_t count = ch->Count();
                        for (uint32_t i = 0; i < count; ++i) {
                            const Entity ent = ch->GetEntity(i);
                            if (!ent.IsNull()) fn(ent);
                        }
                    }
                }
                return;
            }
            else {
                using QueryTag = std::tuple<std::decay_t<Ts>...>;
                static const std::vector<Archetype*>* cached = nullptr;
                static uint64_t cacheVersion = 0;
                static std::array<TypeId, numComponents> typeIds = { 0 };
                
                if (cached == nullptr || cacheVersion != m_archetypeVersion) {

                    if (cached == nullptr) {
                        typeIds = { TypeIdOf<std::decay_t<Ts>>()... };
                    }
                    
                    const Signature req(std::vector<TypeId>(typeIds.begin(), typeIds.end()));
                    cached = &_getMatchingArchetypes(req);
                    cacheVersion = m_archetypeVersion;
                }
                const auto& matched = *cached;
                
                for (const Archetype* arch : matched) {
                    if (!arch) continue;

                    std::array<uint32_t, numComponents> compIdxs;
                    for (size_t k = 0; k < numComponents; ++k) {
                        compIdxs[k] = arch->GetComponentIndex(typeIds[k]);
                    }

                    const uint32_t chunkCount = arch->GetChunkCount();
                    
                    for (uint32_t ci = 0; ci < chunkCount; ++ci) {
                        const Chunk* ch = arch->GetChunk(ci);
                        const uint32_t count = ch->Count();
                        
                        // Skip empty chunks
                        if (count == 0) continue;
                        
                        std::array<const uint8_t*, numComponents> componentBases;
                        std::array<size_t, numComponents> strides;
                        
                        for (size_t pi = 0; pi < numComponents; ++pi) {
                            componentBases[pi] = static_cast<const uint8_t*>(ch->ComponentBase(compIdxs[pi]));
                            strides[pi] = arch->GetComponentStride(compIdxs[pi]);
                        }
                        
                        const Entity* entities = ch->Entities().data();

                        for (uint32_t i = 0; i < count; ++i) {
                            _invokeWithConstComponents<Ts...>(fn, entities[i], componentBases, strides, i, std::index_sequence_for<Ts...>{});
                        }
                    }
                }
            }
        }

        /**
		 * @brief Destroy all entities in the world safely, invoking per-entity hooks.
		 * 
		 * @note This operation may be slower and more expensive than Clear(), which skips hooks.
		 * @note Consider using Clear() if you simply want to drop all entities quickly.
         */
        void DestroyAll() {
            std::vector<Entity> toDestroy;
            // Reserve a rough capacity to minimize reallocations
            toDestroy.reserve(1024);

            // Collect all alive entities
            for (auto& [sig, archPtr] : m_archetypes) {
                if (!archPtr) // Should not happen, but just in case
                    continue;
                
                // Get raw pointer
                Archetype* arch = archPtr.get();

                // Then iterate through all of its chunks and entities
                for (uint32_t ci = 0; ci < arch->GetChunkCount(); ++ci) {
                    // Self explanatory
                    const Chunk* chunk = arch->GetChunk(ci);
                    const uint32_t count = chunk->Count();
                    
                    // Iterate through all entities in the chunk
                    for (uint32_t slot = 0; slot < count; ++slot) {
                        // Collect entity from chunk
                        const Entity e = chunk->GetEntity(slot);

                        if (!e.IsNull()) {
                            toDestroy.push_back(e); // Collect for destruction if not null
                        }
                    }
                }
            }

            // Destroy after collection to avoid mutating during traversal
            for (const Entity e : toDestroy) {
                Destroy(e); // Destroy each collected entity (hence why it's more expensive)
                // Could consider parallelizing this if needed?
                // Need to consider if it is thread-safe though
            }
        }



        /**
         * @brief Get all archetypes in the world.
         * @return Vector of pointers to all archetypes
         */
        std::vector<Archetype*> GetAllArchetypes() const {
            std::vector<Archetype*> result;
            for (const auto& kv : m_archetypes) {
                if (kv.second) {
                    result.push_back(kv.second.get());
                }
            }
            return result;
        }

        /**
		 * @brief Drop all entities and reset the world state quickly, skipping per-entity hooks.
		 *
		 * @note This operation is the fastest way to clear all entities from the world.
		 * @note However, existing Entity handles become invalid after this operation.
		 * @note Furthermore, no per-entity destruction hooks are invoked; use DestroyAll() if needed.
         */
        void Clear() {
            m_archetypes.clear();

            m_locations.clear();
            m_generations.clear();
            m_free.clear();

            // Reset hierarchy indices
            m_hierarchy = HierarchyIndex{};

			/***** !!!!!! *****/
            // Note: Intentionally keep m_componentSizes and chunk so subsequent usage
            // can reuse type size info and capacity heuristics.
			/***** !!!!!! *****/
        }

        /**
		 * @brief Attach a child entity to a parent entity in the hierarchy.
		 * @param child The child entity to attach
		 * @param parent The parent entity to which the child will be attached
         * @note Assumes both entities are alive
         */
        void Attach(const Entity child, const Entity parent) { Set<Parent>(child, Parent{parent}); }

        /**
		 * @brief Detach a child entity from its parent in the hierarchy.
		 * @param child The child entity to detach
         * @note Assumes the child entity is alive
         */
        void Detach(const Entity child)                      { if (Has<Parent>(child)) Remove<Parent>(child); }

        /**
		 * @brief Iterate over all children of the specified parent entity, invoking the provided function for each child.
		 * @tparam TFn The type of the function to invoke for each child entity
		 * @param parent The parent entity whose children will be iterated over
		 * @param fn The function to invoke for each child entity; should accept a single parameter of type Entity
         * @note Assumes the parent entity is alive
         */
        template<typename TFn>
        void ForChildren(const Entity parent, TFn&& fn) const {
            // Iterate through children using the hierarchy index
            const auto it = m_hierarchy.FirstChild.find(parent);

            // If no children, return
            if (it == m_hierarchy.FirstChild.end())
                return;

            // Iterate through siblings
            Entity c = it->second;
            while (!c.IsNull()) {
                // Invoke the provided lambda expression for the child
                fn(c);

                // Move to next sibling
                auto itN = m_hierarchy.NextSibling.find(c);
                c = itN == m_hierarchy.NextSibling.end() ? NULL_ENTITY : itN->second;
                // Continue until no more siblings (or NULL_ENTITY)
            }
        }

        /**
		 * @brief Get the parent entity of the specified child entity.
		 * @param child The child entity whose parent will be retrieved
		 * @return ECS::Entity The parent entity, or NULL_ENTITY if the child has no parent
         * @note Assumes the child entity is alive
         */
        Entity ParentOf(const Entity child) const {
            // Lookup parent from hierarchy index
            const auto it = m_hierarchy.ParentOf.find(child);

            // Return parent if found, otherwise NULL_ENTITY
            return it == m_hierarchy.ParentOf.end() ? NULL_ENTITY : it->second;
        }

        /**
		 * @brief Clone an alive entity, copying all its components.
		 * @param src Source entity to clone
		 * @param opts Optionally, adjust how certain components are handled during cloning
		 * @return ECS::Entity The newly cloned entity
         * @note Assumes the source entity is alive
         */
        Entity Clone(const Entity src, const CloneOptions& opts = {}) {
            const Entity dst = Create(); // Create new empty entity

            // Copy all components from source to destination
            const auto& srcLoc = m_locations[src.Index];
            if (!srcLoc.ArchetypePtr) {
                // Source has no components; done
                return dst;
            }

            // Perform archetype copy
            Archetype* from = srcLoc.ArchetypePtr;
            Archetype* to = _getOrCreateArchetype(from->GetSignature()); // Get/Create destination archetype from source signature
            auto [ci, slot] = to->Insert(); // Insert into destination archetype

            // Copy all components raw
            // Iterate through all component infos in source archetype
            for (const auto& info : from->GetComponents()) {
                // Copy component data from source to destination
                void* dstPtr = to->GetRaw(info.Id, ci, slot);

                // Get source pointer then memcpy
                const void* srcPtr = from->GetRaw(info.Id, srcLoc.ChunkIndex, srcLoc.SlotIndex);
                std::memcpy(dstPtr, srcPtr, info.Size);
            }
            _placeEntity(dst, to, ci, slot);

            // **** Cloning Options **** //

            // Parent
            if (!opts.KeepParent && Has<Parent>(dst)) {
                Set<Parent>(dst, Parent{ NULL_ENTITY }); // Detach safely via Set to keep indices consistent
            }

            // Layer
            if (!opts.KeepLayer && Has<Components::Layer>(dst)) {
                Remove<Components::Layer>(dst);
            }

            // Name
            if (!opts.KeepName && Has<Components::Name>(dst)) {
                // Clear name cheaply
                const Components::Name n{};
                Set<Components::Name>(dst, n);
            }

            // Mark world transform dirty if present so it gets updated next frame
            // Important as the cloned entity may be moved later
            // Check ECS::Hierarchy on how it is handled
            if (Has<Components::WorldTransform>(dst)) {
                Get<Components::WorldTransform>(dst).Dirty = true;
            }

            // Finally, return the cloned entity
            return dst;
        }

        /**
         * @brief Get the map of archetypes in the world.
         * @return const std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash>& Reference to the archetypes map
         */
        const std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash>& Archetypes() const { return m_archetypes; }

        /**
         * @brief Get the PrefabManager associated with this world.
         * @return PrefabManager* Pointer to the PrefabManager, or nullptr if not set
         */
        PrefabManager* GetPrefabManager() const { return m_prefabManager; }

        /**
         * @brief Set the PrefabManager for this world.
         * @param manager Pointer to the PrefabManager
         */
        void SetPrefabManager(PrefabManager* manager) { m_prefabManager = manager; }

        // ************** Entity Location ************** //

        // Location of an entity within the world
        // Used internally for fast lookups
        struct Location {
        public:
            Archetype* ArchetypePtr = nullptr;
            uint32_t ChunkIndex = 0;
            uint32_t SlotIndex = 0;
        };

        /**
		 * @brief Get the location of the specified entity within the world.
		 * @param e The entity whose location is to be retrieved
		 * @return const Location* Pointer to the entity's location; nullptr if the entity is not alive
         */
        const Location* LocationOf(const Entity e) const {
            if (!IsAlive(e)) // Despite assumption for component API, check here
                return nullptr;

            // Return pointer to location
            return &m_locations[e.Index];
        }

    private:
        // ************** Internal Data Structures ************** //

        // Hierarchy indices for fast parent-child lookups
        // Updated automatically when Parent component is added/removed
        // Used by Attach/Detach/ForChildren/ParentOf functions
        // This is basically a doubly linked list structure stored in hash maps
        struct HierarchyIndex {
            std::unordered_map<Entity, Entity, EntityHash> ParentOf;
            std::unordered_map<Entity, Entity, EntityHash> FirstChild;
            std::unordered_map<Entity, Entity, EntityHash> NextSibling;
            std::unordered_map<Entity, Entity, EntityHash> PrevSibling;
        };

        // ************** Internal Helper Functions ************** //

        // Get or create an archetype for the given signature
        Archetype* _getOrCreateArchetype(const Signature& sig) {
            // Check if archetype already exists
            // Signature is basically a set of component TypeIds
            // TypeIds are unique per component type
            const auto it = m_archetypes.find(sig);
            if (it != m_archetypes.end())
                return it->second.get();

            // Create new archetype
            std::vector<ComponentInfo> infos;

            // Reserve space for component infos
            infos.reserve(sig.Types().size());

            // Then populate component infos based on signature
            for (auto t : sig.Types()) {
                const auto si = m_componentSizes[t];
                infos.push_back(ComponentInfo{ t, si.first, si.second });
            }

            // Finally, create and store the new archetype in the map for signature-based lookup
            auto arch = std::make_unique<Archetype>(sig, std::move(infos), m_chunkCapacity, m_chunkBytes);
            Archetype* ptr = arch.get();
            m_archetypes.emplace(sig, std::move(arch));

            // Add to ordered list for incremental query processing
            m_archetypeList.push_back(ptr);
            
            // Increment version to signal queries that new archetypes exist
            ++m_archetypeVersion;

            return ptr;
        }

        // Ensure component size and alignment info is recorded
        template<typename T>
        void _ensureComponentInfo() {
            // Check if component type T is already recorded
            auto t = TypeIdOf<T>();
            if (m_componentSizes.find(t) == m_componentSizes.end()) {
                // Record size and alignment of component type T
                m_componentSizes[t] = { sizeof(T), alignof(T) };
            }
        }

        // Emplace multiple components into an entity efficiently
        template<typename... Ts>
        void _emplaceComponents(const Entity e, const Ts&... comps) {
            // Ensure all component infos are recorded
            // Could consider fold expression: 
            // https://medium.com/@unknown_guy_10/simplifying-variadic-templates-with-fold-expressions-in-c-17-c-17-features-613bcdf307da
            (void)std::initializer_list<int>{ ( _ensureComponentInfo<Ts>(), 0 )... };

            // Create signature for the component types
            const Signature sig({ TypeIdOf<Ts>()... });
            Archetype* to = _getOrCreateArchetype(sig);
            auto [ci, slot] = to->Insert(); // Insert new slot in the archetype
            
            // Construct each component in-place using placement new
            (void)std::initializer_list<int>{ ( (new (to->GetRaw(TypeIdOf<Ts>(), ci, slot)) Ts(comps)), 0 )... };

            // Finally, place the entity in the new archetype location
            _placeEntity(e, to, ci, slot);

            // Notify component additions
            (void)std::initializer_list<int>{ (_onComponentAdded(e, TypeIdOf<Ts>()), 0)... };
        }

        // Structural add/remove helpers
        // Mover is a callable that constructs T in-place at the destination
        template<typename T, typename TMover>
        void _structuralAdd(Entity e, TypeId t, TMover&& mover) {
            // Ensure component info is recorded
            _ensureComponentInfo<T>();
            auto& loc = m_locations[e.Index];

            // If entity has no archetype, create new one with just T
            if (!loc.ArchetypePtr) {
                // New signature with just T
                const Signature ns({ t });

                // Get/Create archetype for new signature
                Archetype* to = _getOrCreateArchetype(ns);

                // Insert new slot then construct T in-place using mover
                auto [ci, slot] = to->Insert();
                mover(e, to->GetRaw(t, ci, slot));
                _placeEntity(e, to, ci, slot); // Place entity in new archetype

                return; // Done
            }

            // If entity already has T, do nothing
            if (loc.ArchetypePtr->Has(t)) {
                // Get existing component pointer
                T* dst = static_cast<T*>(loc.ArchetypePtr->GetRaw(t, loc.ChunkIndex, loc.SlotIndex));

                // Then update it using move semantics
                *dst = T(std::forward<T>(*dst)); // not typically reached
                // *dst = T(std::forward<TMover>(mover), *dst); // not typically reached

                return; // Already has component, nothing to do
            }

            // Otherwise, move entity to new archetype with T added
            Archetype* from = loc.ArchetypePtr;

            // Check if this transition was cached before
            Archetype* to = from->GetAddEdge(t);
            
            if (!to) {
                // First time adding T to this archetype, compute destination and cache the edge
                const Signature ns = from->GetSignature().MergedWith(t);
                to = _getOrCreateArchetype(ns);
                
                // Cache this transition for next time
                from->SetAddEdge(t, to);
            }
            // Now 'to' points to the destination archetype (either cached or newly computed)
            // Insert new slot in new archetype
            auto [ci, slot] = to->Insert();

            // Copy existing components to new archetype
            for (auto& info : loc.ArchetypePtr->GetComponents()) {
                // Skip T as it will be constructed by mover
                if (info.Id == t) continue;

                // Copy component data from old archetype to new archetype
                void* dst = to->GetRaw(info.Id, ci, slot);

                // Get source pointer then memcpy
                const void* src = loc.ArchetypePtr->GetRaw(info.Id, loc.ChunkIndex, loc.SlotIndex);
                std::memcpy(dst, src, info.Size);
            }

            // Now construct T in-place using mover
            // Then move entity to new archetype
            mover(e, to->GetRaw(t, ci, slot));
            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
        }

        // Structural remove helper
        template<typename T>
        void _structuralRemove(const Entity e, const TypeId t) {
            // Assume entity has the component
            auto& loc = m_locations[e.Index];

            // Basically the same as _structuralAdd for most lines

            // If entity has no archetype or doesn't have T, nothing to do
            if (!loc.ArchetypePtr || !loc.ArchetypePtr->Has(t))
                return;

            Archetype* from = loc.ArchetypePtr;
            
            Archetype* to = from->GetRemoveEdge(t);
            
            if (!to) {
                const Signature ns = from->GetSignature().Without(t);
                
                if (ns.Types().empty()) {
                    to = nullptr;
                } 
                else {
                    to = _getOrCreateArchetype(ns);
                }
                
                from->SetRemoveEdge(t, to);
            }
            
            // Store from location info of entity
            const uint32_t fromChunk = loc.ChunkIndex;
            const uint32_t fromSlot = loc.SlotIndex;

            // If no components left after removal, just remove entity from archetype
            if (!to) {
                _removeFromArchetype(e, loc);
                loc.ArchetypePtr = nullptr;

                return;
            }

            // Otherwise, insert new slot in new archetype
            auto [ci, slot] = to->Insert();
            for (auto& info : loc.ArchetypePtr->GetComponents()) {
                // Skip T as it is being removed
                if (info.Id == t) continue;

                // Copy component data from old archetype to new archetype
                void* dst = to->GetRaw(info.Id, ci, slot);
                const void* src = loc.ArchetypePtr->GetRaw(info.Id, fromChunk, fromSlot);
                std::memcpy(dst, src, info.Size);
            }

            // Move entity to new archetype
            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
        }

        // Place entity in archetype at specified chunk and slot
        void _placeEntity(const Entity e, Archetype* arch, const uint32_t chunkIndex, const uint32_t slot) {
            // Update entity location
            m_locations[e.Index] = Location{ arch, chunkIndex, slot };
            
            // Store entity ID directly in the chunk
            arch->GetChunk(chunkIndex)->SetEntity(slot, e);
        }

        void _removeFromArchetype(const Entity e, Location& loc) {
            (void)e;
            // Remove entity from its current archetype
            Archetype* arch = loc.ArchetypePtr;
            const uint32_t chunkIndex = loc.ChunkIndex;
            const uint32_t slotIndex = loc.SlotIndex;
            
            // Get the chunk pointer
            Chunk* chunk = arch->GetChunk(chunkIndex);
            
            // RemoveSwapBack handles both component data and entity ID swapping
            const uint32_t movedSlot = chunk->RemoveSwapBack(slotIndex);
            
            // If an entity was swapped, update its location
            if (movedSlot != slotIndex) {
                const Entity movedEntity = chunk->GetEntity(slotIndex);

                // Update moved entity's location
                if (!movedEntity.IsNull()) {
                    // Update its location to the new slot index
                    auto& movedLoc = m_locations[movedEntity.Index];
                    movedLoc.SlotIndex = slotIndex;
                }
            }
            
            // Detach removed entity from archetype
            loc.ArchetypePtr = nullptr;
        }

        // Reverse lookup: given archetype, chunk index, slot, get entity
        // Returns NULL_ENTITY if invalid
        Entity _reverseEntity(Archetype& arch, const uint32_t chunkIndex, const uint32_t slot) const {
            // Check bounds
            if (chunkIndex >= arch.GetChunkCount())
                return NULL_ENTITY;
            
            // Get chunk pointer
            const Chunk* ch = arch.GetChunk(chunkIndex);

            // Check slot bounds
            if (slot >= ch->Count())
                return NULL_ENTITY;

            // Get entity at slot
            return ch->GetEntity(slot);
        }

        // Const version of the above
        Entity _reverseEntity(const Archetype& arch, const uint32_t chunkIndex, const uint32_t slot) const {
            if (chunkIndex >= arch.GetChunkCount())
                return NULL_ENTITY;
            
            const Chunk* ch = arch.GetChunk(chunkIndex);
            if (slot >= ch->Count())
                return NULL_ENTITY;
            
            return ch->GetEntity(slot);
        }

        // Component lifecycle hooks
        void _onComponentAdded(const Entity e, const TypeId t) {
            // Handle Parent component additions for hierarchy indexing
            // Update hierarchy indices accordingly
            // Assumes Parent component is well-formed
            if (t == TypeIdOf<Parent>()) {
                // Get Parent component
                // Then find existing parent in index
                const auto& p = Get<Parent>(e);
                const auto found = m_hierarchy.ParentOf.find(e);

                // If already has a parent, unlink first
                if (found != m_hierarchy.ParentOf.end()) {
                    // If same parent, nothing to do
                    if (found->second == p.ParentEntity)
                        return;
                    
                    // Unlink from old parent
                    _unlinkChild(e);
                }

                // If new parent is NULL_ENTITY, nothing more to do
                if (p.ParentEntity.IsNull())
                    return;

                // Get first child of the new parent, if it exists 
                const Entity firstChild = m_hierarchy.FirstChild[p.ParentEntity];
                
                if (firstChild.IsNull()) {
                    // No existing children, make this the first child
                    m_hierarchy.FirstChild[p.ParentEntity] = e;
                    m_hierarchy.NextSibling[e] = NULL_ENTITY;
                    m_hierarchy.PrevSibling[e] = NULL_ENTITY;
                }
                else {
                    // Find the last child in the sibling list
                    Entity lastChild = firstChild;
                    auto it = m_hierarchy.NextSibling.find(lastChild);
                    while (it != m_hierarchy.NextSibling.end() && !it->second.IsNull()) {
                        lastChild = it->second;
                        it = m_hierarchy.NextSibling.find(lastChild);
                    }
                    
                    // Append new child after the last child
                    m_hierarchy.NextSibling[lastChild] = e;
                    m_hierarchy.PrevSibling[e] = lastChild;
                    m_hierarchy.NextSibling[e] = NULL_ENTITY;
                }
                
                m_hierarchy.ParentOf[e] = p.ParentEntity;   // Set parent mapping
            }
        }

        // Component changed hook
        void _onComponentChanged(const Entity e, const TypeId t) {
            // Currently only Parent component changes are relevant
            if (t == TypeIdOf<Parent>())
                _onComponentAdded(e, t); // Reuse addition logic to handle changes
        }

        // Component removing hook
        void _onComponentRemoving(const Entity e, const TypeId t) {
            if (t == TypeIdOf<Parent>()) {
                // Unlink from hierarchy indices
                _unlinkChild(e);

                // Remove from parent mapping
                m_hierarchy.ParentOf.erase(e);
            }
        }

        // Unlink child from its parent and sibling links
        void _unlinkChild(const Entity e) {
            // Find parent
            // If no parent, nothing to do
            auto pit = m_hierarchy.ParentOf.find(e);
            if (pit == m_hierarchy.ParentOf.end())
                return;

            // Get parent entity
            const Entity parent = pit->second;
            Entity prev = NULL_ENTITY,
                   next = NULL_ENTITY; // Initialize to null

            // Find previous and next siblings
            const auto itPrev = m_hierarchy.PrevSibling.find(e);
            if (itPrev != m_hierarchy.PrevSibling.end())
                prev = itPrev->second; // Found previous sibling, set prev

            // Find next sibling
            const auto itNext = m_hierarchy.NextSibling.find(e);
            if (itNext != m_hierarchy.NextSibling.end())
                next = itNext->second; // Found next sibling, set next
            
            // Update parent's first child or previous sibling's next sibling
            if (prev.IsNull()) {
                // Update parent's first child
                if (next.IsNull())
                    m_hierarchy.FirstChild.erase(parent);
                // If no next sibling, remove first child entry
                else 
                    m_hierarchy.FirstChild[parent] = next;
            }

            else { // Has previous sibling
                m_hierarchy.NextSibling[prev] = next; // Link previous sibling to next
            }

            // Update next sibling's previous sibling link
            if (!next.IsNull())
                m_hierarchy.PrevSibling[next] = prev; // Link next sibling to previous

            // Finally, remove this entity's sibling links (both directions)
            m_hierarchy.PrevSibling.erase(e);
            m_hierarchy.NextSibling.erase(e);
        }

        // Returns cached list of archetypes that contain all components in 'req'.
        // Cache is rebuilt from scratch when called
        const std::vector<Archetype*>& _getMatchingArchetypes(const Signature& req) const {
            // Always rebuild the cache for this signature to catch new archetypes
            // This is called when cacheVersion != m_archetypeVersion, meaning archetypes changed
            auto& vec = m_queryCache[req];
            
            // Clear old cache and rebuild from all current archetypes
            vec.clear();
            vec.reserve(m_archetypes.size()); // Reserve space to avoid reallocations
            
            for (const auto& kv : m_archetypes) {
                // Get archetype pointer
                const auto& archPtr = kv.second;

                // Check if archetype's signature contains *ALL* required components
                if (archPtr && archPtr->GetSignature().ContainsAll(req)) {
                    vec.push_back(archPtr.get()); // Add matching archetype to results
                }
            }

            // Return the newly populated cache entry
            return m_queryCache.at(req);
        }

        // Helper to invoke function with component references using base pointers + offsets
        // Unpacks parameter pack using index sequence
        // Assumes Ts are all non-const types
        // Reinterprets base pointers + offsets to call function with component references
        // Uses std::decay_t to strip references/constness for proper casting
        // Unpacks parameter pack using index sequence
        template<typename... Ts, typename TFn, size_t... Is>
        inline void _invokeWithComponents(TFn&& fn, const Entity ent, 
                                   const std::array<uint8_t*, sizeof...(Ts)>& bases,
                                   const std::array<size_t, sizeof...(Ts)>& strides,
                                   const uint32_t index,
                                   std::index_sequence<Is...>) {
            fn(ent, (*reinterpret_cast<std::decay_t<Ts>*>(bases[Is] + strides[Is] * index))...);
        }

        // Helper for const version
        template<typename... Ts, typename TFn, size_t... Is>
        inline void _invokeWithConstComponents(TFn&& fn, const Entity ent,
                                       const std::array<const uint8_t*, sizeof...(Ts)>& bases,
                                       const std::array<size_t, sizeof...(Ts)>& strides,
                                       const uint32_t index,
                                       std::index_sequence<Is...>) const {
            fn(ent, (*reinterpret_cast<const std::decay_t<Ts>*>(bases[Is] + strides[Is] * index))...);
        }

    private:
        uint32_t m_chunkCapacity = 256;
        size_t m_chunkBytes = 16384; // 16 * 1024;

        std::vector<uint32_t> m_generations;
        std::vector<uint32_t> m_free;
        std::vector<Location> m_locations;

        std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash> m_archetypes;
        std::unordered_map<TypeId, std::pair<size_t,size_t>> m_componentSizes;

        // Batch operation depth for deferred archetype version increments
        uint32_t m_batchDepth = 0;
        uint64_t m_batchStartVersion = 0;

        HierarchyIndex m_hierarchy;

        // Query cache: maps a required signature to matching archetypes.
        // Mutable to allow filling cache in const Each.
        // TODO: Consider using a more efficient caching strategy
        mutable std::unordered_map<Signature, std::vector<Archetype*>, SignatureHash> m_queryCache;
        uint64_t m_archetypeVersion = 0;
        
        // Ordered list of all archetypes for incremental query processing
        // When m_archetypeVersion increases, queries only check archetypes added since last update
        std::vector<Archetype*> m_archetypeList;

        // PrefabManager for managing prefab instances and registration
        PrefabManager* m_prefabManager = nullptr;
    };
}

#endif
