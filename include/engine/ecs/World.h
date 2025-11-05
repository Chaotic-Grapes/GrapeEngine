/* Start Header *****************************************************************/
/*!
\file    World.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the World class, responsible
for managing the entire Entity-Component-System (ECS) world. It provides methods
for creating, destroying, and managing entities and their components. The World
class also handles the updating of entity transforms and the processing of
systems within the ECS architecture.

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
#include "ecs/Entity.h"
#include "ecs/Archetype.h"
#include "ecs/Components.h"
#include "ecs/Signature.h"
#include "ecs/ComponentRegistry.h"
#include "math/Matrix4x4.h"

namespace ECS {
    // Relationship component for hierarchy
    // Move this to Components.h?
    struct Parent { Entity ParentEntity{NULL_ENTITY}; };

    struct CloneOptions {
    public:
        // If true, keep Parent as-is; otherwise detach cloned entities from their parents
        bool KeepParent = false;
        // Keep Layer component (if present)
        bool KeepLayer = true;
        // Keep Name component (if present)
        bool KeepName = true;
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
            if (!m_free.empty()) {
                idx = m_free.back(); m_free.pop_back();
            }
            else {
                idx = static_cast<EntityId>(m_generations.size());
                m_generations.push_back(0);
                m_locations.push_back(Location{});
            }

            return Entity{ idx, m_generations[idx] };
        }

        /**
		 * @brief Create an entity with the specified components in the world.
		 * @tparam Ts Component types to add to the entity
		 * @param comps Component instances to add to the entity
		 * @return ECS::Entity The newly created entity with the specified components
         */
        template<typename... Ts>
        Entity Create(const Ts&... comps) {
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
            return e.Index < m_generations.size() && m_generations[e.Index] == e.Generation;
        }

        /**
		 * @brief Destroy the specified entity, removing it from the world.
		 * @param e The entity to destroy
         */
        void Destroy(const Entity e) {
            if (!IsAlive(e))
                return;

            auto &loc = m_locations[e.Index];
            if (loc.ArchetypePtr) {
                _removeFromArchetype(e, loc);
            }

            ++m_generations[e.Index];
            m_free.push_back(e.Index);
        }

        // ************** Component API ************** //

        /**
		 * @brief Check if the specified entity has the given component type.
		 * @tparam T The component type to check for
		 * @param e The entity to check
		 * @return bool True if the entity has the component; false otherwise
         */
        template<typename T>
        bool Has(const Entity e) const {
            if (!IsAlive(e))
                return false;

            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr)
                return false;

            return loc.ArchetypePtr->Has(TypeIdOf<T>());
        }

        /**
		 * @brief Get a reference to the specified component type for the given entity.
		 * @tparam T The component type to retrieve
		 * @param e The entity from which to retrieve the component
		 * @return T& Reference to the requested component
         */
        template<typename T>
        T& Get(const Entity e) {
            assert(IsAlive(e));
            auto& loc = m_locations[e.Index];
            assert(loc.ArchetypePtr && loc.ArchetypePtr->Has(TypeIdOf<T>()));

            return *static_cast<T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }

        /**
		 * @brief Get a const reference to the specified component type for the given entity.
		 * @tparam T The component type to retrieve
		 * @param e The entity from which to retrieve the component
		 * @return const T& Const reference to the requested component
         */
        template<typename T>
        const T& Get(const Entity e) const {
            assert(IsAlive(e));
            auto& loc = m_locations[e.Index];
            assert(loc.ArchetypePtr && loc.ArchetypePtr->Has(TypeIdOf<T>()));

            return *static_cast<const T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }

        /**
		 * @brief Add a component of the specified type to the given entity, constructing it with the provided arguments.
		 * @tparam T The component type to add
		 * @tparam TArgs The types of the constructor arguments for the component
		 * @param e The entity to which the component will be added
		 * @param args Constructor arguments for the component
		 * @return T& Reference to the newly added component
         */
        template<typename T, typename... TArgs>
        T& Add(Entity e, TArgs&&... args) {
            assert(IsAlive(e));
            auto t = TypeIdOf<T>();
            auto mover = [&, this](Entity, void* dstArchetypeSlot) {
                new (dstArchetypeSlot) T(std::forward<TArgs>(args)...);
            };
            _structuralAdd<T>(e, t, mover);
            _onComponentAdded(e, t);

            return Get<T>(e);
        }

        /**
		 * @brief Remove the specified component type from the given entity.
		 * @tparam T The component type to remove
		 * @param e The entity from which the component will be removed
         */
        template<typename T>
        void Remove(Entity e) {
            assert(IsAlive(e));
            if (!Has<T>(e))
                return;

            auto t = TypeIdOf<T>();
            _onComponentRemoving(e, t);
            _structuralRemove<T>(e, t);
        }

        /**
		 * @brief Set the specified component type for the given entity, adding it if not already present.
		 * @tparam T The component type to set
		 * @param e The entity for which the component will be set
		 * @param value The new value for the component
		 * @return T& Reference to the set component
         */
        template<typename T>
        T& Set(Entity e, T value) {
            if (Has<T>(e)) {
                Get<T>(e) = std::move(value);
                _onComponentChanged(e, TypeIdOf<T>());
                return Get<T>(e);
            }
            return Add<T>(e, std::move(value));
        }

        /**
		 * @brief Iterate over all entities that have the specified components, invoking the provided function for each.
		 * @tparam Ts The component types to filter entities by
		 * @tparam TFn The type of the function to invoke for each matching entity
		 * @param fn The function to invoke for each matching entity; should accept parameters (Entity, Ts&...)
         */
        template<typename... Ts, typename TFn>
        void Each(TFn&& fn) {
            const Signature req(std::vector<TypeId>{ TypeIdOf<std::decay_t<Ts>>()... });

            // Precompute TypeIds into an array for this invocation
            const std::array<TypeId, sizeof...(Ts)> typeIds = { TypeIdOf<std::decay_t<Ts>>()... };

            // Use cached matching archetypes to avoid re-scanning all archetypes
            const auto& matched = _getMatchingArchetypes(req);
            for (Archetype* arch : matched) {
                if (!arch) continue;

                // Precompute component indices for this archetype to avoid map lookups per entity
                std::array<uint32_t, sizeof...(Ts)> compIdxs{};
                for (size_t k = 0; k < compIdxs.size(); ++k) {
                    compIdxs[k] = static_cast<uint32_t>(arch->GetComponentIndex(typeIds[k]));
                }

                // Lookup chunk mapping once per archetype
                const auto it = m_chunkIndices.find(arch);
                if (it == m_chunkIndices.end())
                    continue;
                const auto& chunkIndexMapping = it->second.Entities;

                for (uint32_t ci = 0; ci < arch->GetChunkCount(); ++ci) {
                    const Chunk* ch = arch->GetChunk(ci);

                    if (ci >= chunkIndexMapping.size())
                        continue;

                    const auto& vec = chunkIndexMapping[ci];

                    for (uint32_t i = 0; i < ch->Count(); ++i) {
                        if (i >= vec.size())
                            continue;

                        const Entity ent = vec[i];
                        if (ent.IsNull())
                            continue;

                        // Gather pointers for each requested component in this slot
                        std::array<void*, sizeof...(Ts)> ptrs{};
                        for (size_t pi = 0; pi < ptrs.size(); ++pi) {
                            ptrs[pi] = const_cast<void*>(ch->ComponentPtr(static_cast<uint32_t>(compIdxs[pi]), i));
                        }

                        // Call the function with properly casted references
                        std::apply([&](auto&&... p) {
                            fn(ent, (*static_cast<std::decay_t<Ts>*>(p))...);
                        }, ptrs);
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
            const Signature req(std::vector<TypeId>{ TypeIdOf<std::decay_t<Ts>>()... });
            // Precompute TypeIds
            const std::array<TypeId, sizeof...(Ts)> typeIds = { TypeIdOf<std::decay_t<Ts>>()... };

            // Use cached matching archetypes to avoid re-scanning all archetypes
            const auto& matched = _getMatchingArchetypes(req);
            for (const Archetype* arch : matched) {
                if (!arch) continue;

                // Precompute component indices for this archetype
                std::array<uint32_t, sizeof...(Ts)> compIdxs{};
                for (size_t k = 0; k < compIdxs.size(); ++k) {
                    compIdxs[k] = static_cast<uint32_t>(arch->GetComponentIndex(typeIds[k]));
                }

                // Lookup chunk mapping once per archetype
                const auto it = m_chunkIndices.find(const_cast<Archetype*>(arch));
                if (it == m_chunkIndices.end())
                    continue;
                const auto& chunkIndexMapping = it->second.Entities;

                for (uint32_t ci = 0; ci < arch->GetChunkCount(); ++ci) {
                    const Chunk* ch = arch->GetChunk(ci);

                    if (ci >= chunkIndexMapping.size())
                        continue;

                    const auto& vec = chunkIndexMapping[ci];

                    for (uint32_t i = 0; i < ch->Count(); ++i) {
                        if (i >= vec.size())
                            continue;

                        const Entity ent = vec[i];
                        if (ent.IsNull())
                            continue;

                        std::array<const void*, sizeof...(Ts)> ptrs{};
                        for (size_t pi = 0; pi < ptrs.size(); ++pi) {
                            ptrs[pi] = ch->ComponentPtr(static_cast<uint32_t>(compIdxs[pi]), i);
                        }

                        std::apply([&](auto&&... p) {
                            fn(ent, (*static_cast<const std::decay_t<Ts>*>(p))...);
                        }, ptrs);
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

            for (auto& [fst, snd] : m_archetypes) {
                auto& archPtr = snd;

                if (!archPtr)
                    continue;
                Archetype* arch = archPtr.get();

                auto it = m_chunkIndices.find(arch);
                if (it == m_chunkIndices.end())
                    continue;
                auto& ci = it->second;

                for (size_t ciIdx = 0; ciIdx < ci.Entities.size(); ++ciIdx) {
                    auto& slots = ci.Entities[ciIdx];
                    for (Entity e : slots) {
                        if (!e.IsNull()) toDestroy.push_back(e);
                    }
                }
            }

            // Destroy after collection to avoid mutating during traversal
            for (const Entity e : toDestroy) {
                Destroy(e);
            }
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
            m_chunkIndices.clear();

            m_locations.clear();
            m_generations.clear();
            m_free.clear();

            // Reset hierarchy indices
            m_hierarchy = HierarchyIndex{};

			/***** !!!!!! *****/
            // Note: Intentionally keep m_componentSizes and chunk tunables so subsequent usage
            // can reuse type size info and capacity heuristics.
			/***** !!!!!! *****/
        }

		// TODO: Properly assess whether deferred structural changes are needed
        // Deferred structural changes (for safe modifications during iteration)
        //class DeferGuard {
        //public:
        //    explicit DeferGuard(World& w) : m_world(w), m_active(true) { m_world.BeginDefer(); }
        //    ~DeferGuard() { if (m_active) m_world.EndDefer(); }
        //    DeferGuard(const DeferGuard&) = delete;
        //    DeferGuard& operator=(const DeferGuard&) = delete;
        //private:
        //    World& m_world;
        //    bool m_active = false;
        //};

        //void BeginDefer() { ++m_deferDepth; }
        //void EndDefer() {
        //    assert(m_deferDepth > 0);
        //    if (--m_deferDepth == 0) {
        //        for (auto& cmd : m_deferred)
        //            cmd(*this);
        //        m_deferred.clear();
        //    }
        //}
        //bool Deferring() const noexcept { return m_deferDepth > 0; }

        /**
		 * @brief Attach a child entity to a parent entity in the hierarchy.
		 * @param child The child entity to attach
		 * @param parent The parent entity to which the child will be attached
         */
        void Attach(const Entity child, const Entity parent) { Set<Parent>(child, Parent{parent}); }

        /**
		 * @brief Detach a child entity from its parent in the hierarchy.
		 * @param child The child entity to detach
         */
        void Detach(const Entity child)                      { if (Has<Parent>(child)) Remove<Parent>(child); }

        /**
		 * @brief Iterate over all children of the specified parent entity, invoking the provided function for each child.
		 * @tparam TFn The type of the function to invoke for each child entity
		 * @param parent The parent entity whose children will be iterated over
		 * @param fn The function to invoke for each child entity; should accept a single parameter of type Entity
         */
        template<typename TFn>
        void ForChildren(const Entity parent, TFn&& fn) const {
            const auto it = m_hierarchy.FirstChild.find(parent);
            if (it == m_hierarchy.FirstChild.end())
                return;

            Entity c = it->second;
            while (!c.IsNull()) {
                fn(c);
                auto itN = m_hierarchy.NextSibling.find(c);
                c = itN == m_hierarchy.NextSibling.end() ? NULL_ENTITY : itN->second;
            }
        }

        /**
		 * @brief Get the parent entity of the specified child entity.
		 * @param child The child entity whose parent will be retrieved
		 * @return ECS::Entity The parent entity, or NULL_ENTITY if the child has no parent
         */
        Entity ParentOf(const Entity child) const {
            const auto it = m_hierarchy.ParentOf.find(child);
            return it == m_hierarchy.ParentOf.end() ? NULL_ENTITY : it->second;
        }

        /**
		 * @brief Clone an alive entity, copying all its components.
		 * @param src Source entity to clone
		 * @param opts Optionally, adjust how certain components are handled during cloning
		 * @return ECS::Entity The newly cloned entity
		 * @exception std::exception Thrown if the source entity is not alive
         */
        Entity Clone(const Entity src, const CloneOptions& opts = {}) {
            if (!IsAlive(src)) 
				throw std::exception("Cannot clone dead entity");

            const Entity dst = Create();

            const auto& srcLoc = m_locations[src.Index];
            if (!srcLoc.ArchetypePtr) {
                // Source has no components; done
                return dst;
            }

            Archetype* from = srcLoc.ArchetypePtr;
            Archetype* to = _getOrCreateArchetype(from->GetSignature());
            auto [ci, slot] = to->Insert();

            // Copy all components raw
            for (const auto& info : from->GetComponents()) {
                void* dstPtr = to->GetRaw(info.Id, ci, slot);
                const void* srcPtr = from->GetRaw(info.Id, srcLoc.ChunkIndex, srcLoc.SlotIndex);
                std::memcpy(dstPtr, srcPtr, info.Size);
            }
            _placeEntity(dst, to, ci, slot);

            // Adjust Parent/Layer/Name per options
            if (!opts.KeepParent && Has<Parent>(dst)) {
                Set<Parent>(dst, Parent{ NULL_ENTITY }); // Detach safely via Set to keep indices consistent
            }

            if (!opts.KeepLayer && Has<Components::Layer>(dst)) {
                Remove<Components::Layer>(dst);
            }

            if (!opts.KeepName && Has<Components::Name>(dst)) {
                // Clear name cheaply
                const Components::Name n{};
                Set<Components::Name>(dst, n);
            }

            // Mark world transform dirty if present
            if (Has<Components::WorldTransform>(dst)) {
                Get<Components::WorldTransform>(dst).Dirty = true;
            }

            return dst;
        }

        /**
         * @brief Get the map of archetypes in the world.
         * @return const std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash>& Reference to the archetypes map
         */
        const std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash>& Archetypes() const { return m_archetypes; }

        // ************** Entity Location ************** //

        // Location of an entity within the world
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
            if (!IsAlive(e))
                return nullptr;
            return &m_locations[e.Index];
        }

    private:
        struct ChunkIndex {
            std::vector<std::vector<Entity>> Entities; // [chunkIndex][slotIndex]
        };

        struct HierarchyIndex {
            std::unordered_map<Entity, Entity, EntityHash> ParentOf;
            std::unordered_map<Entity, Entity, EntityHash> FirstChild;
            std::unordered_map<Entity, Entity, EntityHash> NextSibling;
            std::unordered_map<Entity, Entity, EntityHash> PrevSibling;
        };

        Archetype* _getOrCreateArchetype(const Signature& sig) {
            const auto it = m_archetypes.find(sig);
            if (it != m_archetypes.end())
                return it->second.get();

            std::vector<ComponentInfo> infos;
            infos.reserve(sig.Types().size());
            for (auto t : sig.Types()) {
                const auto si = m_componentSizes[t];
                infos.push_back(ComponentInfo{ t, si.first, si.second });
            }

            auto arch = std::make_unique<Archetype>(sig, std::move(infos), m_chunkCapacity, m_chunkBytes);
            Archetype* ptr = arch.get();
            m_archetypes.emplace(sig, std::move(arch));
            m_chunkIndices[ptr] = ChunkIndex{};

            // Invalidate query cache when a new archetype is created
            ++m_archetypeVersion;
            m_queryCache.clear();

            return ptr;
        }

        template<typename T>
        void _ensureComponentInfo() {
            auto t = TypeIdOf<T>();
            if (m_componentSizes.find(t) == m_componentSizes.end()) {
                m_componentSizes[t] = { sizeof(T), alignof(T) };
            }
        }

        template<typename... Ts>
        void _emplaceComponents(const Entity e, const Ts&... comps) {
            (_ensureComponentInfo<Ts>(), ...);

            const Signature sig({ TypeIdOf<Ts>()... });
            Archetype* to = _getOrCreateArchetype(sig);
            auto [ci, slot] = to->Insert();
            
            ( (new (to->GetRaw(TypeIdOf<Ts>(), ci, slot)) Ts(comps)), ... );

            _placeEntity(e, to, ci, slot);
            (_onComponentAdded(e, TypeIdOf<Ts>()), ...);
        }

		// TODO: Check the mover logic here; seems off
        template<typename T, typename TMover>
        void _structuralAdd(Entity e, TypeId t, TMover&& mover) {
            _ensureComponentInfo<T>();
            auto& loc = m_locations[e.Index];

            if (!loc.ArchetypePtr) {
                const Signature ns({ t });
                Archetype* to = _getOrCreateArchetype(ns);
                auto [ci, slot] = to->Insert();
                mover(e, to->GetRaw(t, ci, slot));
                _placeEntity(e, to, ci, slot);

                return;
            }

            if (loc.ArchetypePtr->Has(t)) {
                T* dst = static_cast<T*>(loc.ArchetypePtr->GetRaw(t, loc.ChunkIndex, loc.SlotIndex));
                *dst = T(std::forward<T>(*dst)); // not typically reached
                // *dst = T(std::forward<TMover>(mover), *dst); // not typically reached

                return;
            }

            const Signature ns = loc.ArchetypePtr->GetSignature().MergedWith(t);
            Archetype* to = _getOrCreateArchetype(ns);
            auto [ci, slot] = to->Insert();

            for (auto& info : loc.ArchetypePtr->GetComponents()) {
                void* dst = to->GetRaw(info.Id, ci, slot);
                const void* src = loc.ArchetypePtr->GetRaw(info.Id, loc.ChunkIndex, loc.SlotIndex);
                std::memcpy(dst, src, info.Size);
            }

            mover(e, to->GetRaw(t, ci, slot));
            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
        }

        template<typename T>
        void _structuralRemove(const Entity e, const TypeId t) {
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr || !loc.ArchetypePtr->Has(t))
                return;

            const Signature ns = loc.ArchetypePtr->GetSignature().Without(t);
            Archetype* to = ns.Types().empty() ? nullptr : _getOrCreateArchetype(ns);
            const uint32_t fromChunk = loc.ChunkIndex;
            const uint32_t fromSlot = loc.SlotIndex;

            if (!to) {
                _removeFromArchetype(e, loc);
                loc.ArchetypePtr = nullptr;

                return;
            }

            auto [ci, slot] = to->Insert();
            for (auto& info : loc.ArchetypePtr->GetComponents()) {
                if (info.Id == t) continue;

                void* dst = to->GetRaw(info.Id, ci, slot);
                const void* src = loc.ArchetypePtr->GetRaw(info.Id, fromChunk, fromSlot);
                std::memcpy(dst, src, info.Size);
            }

            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
        }

        void _placeEntity(const Entity e, Archetype* arch, const uint32_t chunkIndex, const uint32_t slot) {
            m_locations[e.Index] = Location{ arch, chunkIndex, slot };

            auto &ci = m_chunkIndices[arch];
            if (ci.Entities.size() <= chunkIndex)
                ci.Entities.resize(chunkIndex + 1);

            auto &vec = ci.Entities[chunkIndex];
            if (vec.size() <= slot)
                vec.resize(arch->GetChunk(chunkIndex)->Capacity(), NULL_ENTITY);
                
            vec[slot] = e;
        }

        void _removeFromArchetype(const Entity e, Location& loc) {
            (void)e;
            Archetype* arch = loc.ArchetypePtr;
            auto &ci = m_chunkIndices[arch];
            const uint32_t chunkIndex = loc.ChunkIndex;

            // Defensive: ensure mapping vector is large enough
            if (ci.Entities.size() <= chunkIndex) {
                ci.Entities.resize(chunkIndex + 1);
            }

            auto &vec = ci.Entities[chunkIndex];

            // Capture chunk count before removal to detect chunk erasure
            const uint32_t prevChunkCount = arch->GetChunkCount();
            const uint32_t movedFrom = arch->RemoveSwapBack(chunkIndex, loc.SlotIndex);
            const uint32_t postChunkCount = arch->GetChunkCount();
            const bool chunkErased = (postChunkCount + 1u == prevChunkCount);

            if (!chunkErased) {
                // Regular in-chunk swap/remove
                if (movedFrom != loc.SlotIndex) {
                    const Entity moved = vec[movedFrom];
                    vec[loc.SlotIndex] = moved;
                    vec[movedFrom] = NULL_ENTITY;
                    if (!moved.IsNull()) {
                        auto &mloc = m_locations[moved.Index];
                        mloc.SlotIndex = loc.SlotIndex;
                        // ChunkIndex remains the same
                    }
                } else {
                    // Removed last element of the chunk, just clear mapping
                    if (loc.SlotIndex < vec.size()) {
                        vec[loc.SlotIndex] = NULL_ENTITY;
                    }
                }
            } else {
                // The entire chunk at chunkIndex was erased by the archetype.
                // Keep m_chunkIndices in sync and fix affected entity locations.

                // Remove the vector that mirrored the erased chunk
                if (chunkIndex < ci.Entities.size()) {
                    ci.Entities.erase(ci.Entities.begin() + static_cast<std::ptrdiff_t>(chunkIndex));
                }

                // All chunks that were after the erased one have shifted left by 1.
                // Update their entities' recorded ChunkIndex to stay consistent.
                for (uint32_t newChunkIdx = chunkIndex; newChunkIdx < ci.Entities.size(); ++newChunkIdx) {
                    auto &slotVec = ci.Entities[newChunkIdx];
                    for (const Entity ent : slotVec) {
                        if (ent.IsNull()) continue;
                        auto &mloc = m_locations[ent.Index];
                        mloc.ChunkIndex = newChunkIdx;
                    }
                }
            }

            // Detach removed entity from any archetype
            loc.ArchetypePtr = nullptr;
        }

        Entity _reverseEntity(Archetype& arch, const uint32_t chunkIndex, const uint32_t slot) const {
            const auto it = m_chunkIndices.find(&arch);
            if (it == m_chunkIndices.end())
                return NULL_ENTITY;

            const auto &ci = it->second;
            if (chunkIndex >= ci.Entities.size())
                return NULL_ENTITY;

            const auto &vec = ci.Entities[chunkIndex];
            if (slot >= vec.size())
                return NULL_ENTITY;

            return vec[slot];
        }

        Entity _reverseEntity(const Archetype& arch, const uint32_t chunkIndex, const uint32_t slot) const {
            const auto it = m_chunkIndices.find(const_cast<Archetype*>(&arch));
            if (it == m_chunkIndices.end())
                return NULL_ENTITY;

            const auto &ci = it->second;
            if (chunkIndex >= ci.Entities.size())
                return NULL_ENTITY;

            const auto &vec = ci.Entities[chunkIndex];
            if (slot >= vec.size())
                return NULL_ENTITY;

            return vec[slot];
        }

        void _onComponentAdded(const Entity e, const TypeId t) {
            if (t == TypeIdOf<Parent>()) {
                const auto& p = Get<Parent>(e);
                const auto found = m_hierarchy.ParentOf.find(e);

                if (found != m_hierarchy.ParentOf.end()) {
                    if (found->second == p.ParentEntity)
                        return;
                    _unlinkChild(e);
                }

                if (p.ParentEntity.IsNull())
                    return;

                const Entity oldFirst = m_hierarchy.FirstChild[p.ParentEntity];
                m_hierarchy.FirstChild[p.ParentEntity] = e;

                if (!oldFirst.IsNull()) {
                    m_hierarchy.PrevSibling[oldFirst] = e;
                }

                m_hierarchy.NextSibling[e] = oldFirst;
                m_hierarchy.PrevSibling[e] = NULL_ENTITY;
                m_hierarchy.ParentOf[e] = p.ParentEntity;
            }
        }
        void _onComponentChanged(const Entity e, const TypeId t) {
            if (t == TypeIdOf<Parent>()) {
                _onComponentAdded(e, t);
            }
        }
        void _onComponentRemoving(const Entity e, const TypeId t) {
            if (t == TypeIdOf<Parent>()) {
                _unlinkChild(e);
                m_hierarchy.ParentOf.erase(e);
            }
        }

        void _unlinkChild(const Entity e) {
            auto pit = m_hierarchy.ParentOf.find(e);
            if (pit == m_hierarchy.ParentOf.end())
                return;

            const Entity parent = pit->second;
            Entity prev = NULL_ENTITY, next = NULL_ENTITY;

            const auto itPrev = m_hierarchy.PrevSibling.find(e);
            if (itPrev != m_hierarchy.PrevSibling.end())
                prev = itPrev->second;

            const auto itNext = m_hierarchy.NextSibling.find(e);
            if (itNext != m_hierarchy.NextSibling.end())
                next = itNext->second;
            
            if (prev.IsNull()) {
                if (next.IsNull())
                    m_hierarchy.FirstChild.erase(parent);
                else m_hierarchy.FirstChild[parent] = next;
            }
            else {
                m_hierarchy.NextSibling[prev] = next;
            }

            if (!next.IsNull())
                m_hierarchy.PrevSibling[next] = prev;

            m_hierarchy.PrevSibling.erase(e);
            m_hierarchy.NextSibling.erase(e);
        }

        // Returns cached list of archetypes that contain all components in 'req'.
        // Cached data is invalidated whenever new archetypes are created.
        const std::vector<Archetype*>& _getMatchingArchetypes(const Signature& req) const {
            auto it = m_queryCache.find(req);
            if (it != m_queryCache.end()) return it->second;

            auto& vec = m_queryCache[req];
            vec.reserve(m_archetypes.size());
            for (const auto& kv : m_archetypes) {
                const auto& archPtr = kv.second;
                if (archPtr && archPtr->GetSignature().ContainsAll(req)) {
                    vec.push_back(archPtr.get());
                }
            }
            return m_queryCache.at(req);
        }

    private:
        uint32_t m_chunkCapacity = 256;
        size_t m_chunkBytes = 16384; // 16 * 1024;

        std::vector<uint32_t> m_generations;
        std::vector<uint32_t> m_free;
        std::vector<Location> m_locations;

        std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash> m_archetypes;
        std::unordered_map<Archetype*, ChunkIndex> m_chunkIndices;
        std::unordered_map<TypeId, std::pair<size_t,size_t>> m_componentSizes;

        HierarchyIndex m_hierarchy;

        // Query cache: maps a required signature to matching archetypes.
        // Mutable to allow filling cache in const Each.
        mutable std::unordered_map<Signature, std::vector<Archetype*>, SignatureHash> m_queryCache;
        uint64_t m_archetypeVersion = 0;

        using DeferredCmd = std::function<void(World&)>;
        std::vector<DeferredCmd> m_deferred;
        uint32_t m_deferDepth = 0;
    };
}

#endif
