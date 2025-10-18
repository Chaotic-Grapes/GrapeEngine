#ifndef WORLD_H
#define WORLD_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <tuple>
#include <type_traits>
#include <cassert>
#include <functional>
#include "ecs/Entity.h"
#include "ecs/Archetype.h"
#include "ecs/Components.h"
#include "ecs/Signature.h"
#include "ecs/ComponentRegistry.h"
#include "math/Vector3D.h"
#include "math/Matrix4x4.h"
#include "math/Quaternion.h"

namespace ECS {
    // Relationship component for hierarchy
    struct Parent { Entity ParentEntity{NULL_ENTITY}; };

    // Layers: one component holding a small integer id per entity
    struct Layer { uint16_t Id = 0; };

    // Move this?
    struct LocalTransform { Vector3D Position{0,0,0}; Quaternion Rotation{0,0,0,1}; Vector3D Scale{1,1,1}; };
    struct WorldTransform { Matrix4x4 Matrix{}; bool Dirty = true; };

    // World holds all archetypes and entity allocation, moves entities across archetypes on structural changes.
    class World {
    public:
        World() = default;

        // Entity lifecycle
        Entity Create() {
            EntityId idx;
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

        template<typename... Ts>
        Entity Create(const Ts&... comps) {
            Entity e = Create();
            _emplaceComponents(e, comps...);

            return e;
        }

        bool Alive(Entity e) const {
            return e.Index < m_generations.size() && m_generations[e.Index] == e.Generation;
        }

        void Destroy(Entity e) {
            if (!Alive(e))
                return;

            auto &loc = m_locations[e.Index];
            if (loc.ArchetypePtr) {
                _removeFromArchetype(e, loc);
            }

            ++m_generations[e.Index];
            m_free.push_back(e.Index);
        }

        // Component API
        template<typename T>
        bool Has(Entity e) const {
            if (!Alive(e))
                return false;

            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr)
                return false;

            return loc.ArchetypePtr->Has(TypeIdOf<T>());
        }

        template<typename T>
        T& Get(Entity e) {
            assert(Alive(e));
            auto& loc = m_locations[e.Index];
            assert(loc.ArchetypePtr && loc.ArchetypePtr->Has(TypeIdOf<T>()));

            return *reinterpret_cast<T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }
        template<typename T>
        const T& Get(Entity e) const {
            assert(Alive(e));
            auto& loc = m_locations[e.Index];
            assert(loc.ArchetypePtr && loc.ArchetypePtr->Has(TypeIdOf<T>()));

            return *reinterpret_cast<const T*>(loc.ArchetypePtr->GetRaw(TypeIdOf<T>(), loc.ChunkIndex, loc.SlotIndex));
        }

        template<typename T, typename... TArgs>
        T& Add(Entity e, TArgs&&... args) {
            assert(Alive(e));
            auto t = TypeIdOf<T>();
            auto mover = [&, this](Entity, void* dstArchetypeSlot) {
                new (dstArchetypeSlot) T(std::forward<TArgs>(args)...);
            };
            _structuralAdd<T>(e, t, mover);
            _onComponentAdded(e, t);

            return Get<T>(e);
        }

        template<typename T>
        void Remove(Entity e) {
            assert(Alive(e));
            if (!Has<T>(e))
                return;

            auto t = TypeIdOf<T>();
            _onComponentRemoving(e, t);
            _structuralRemove<T>(e, t);
        }

        template<typename T>
        T& Set(Entity e, T value) {
            if (Has<T>(e)) {
                Get<T>(e) = std::move(value);
                _onComponentChanged(e, TypeIdOf<T>());
                return Get<T>(e);
            }
            return Add<T>(e, std::move(value));
        }

        // Query iteration: iterate all archetypes that contain Ts...
        template<typename... Ts, typename TFn>
        void Each(TFn&& fn) {
            Signature req({ TypeIdOf<std::decay_t<Ts>>()... });

            for (auto& kv : m_archetypes) {
                auto& archPtr = kv.second;

                if (!archPtr)
                    continue;
                if (!archPtr->GetSignature().ContainsAll(req))
                    continue;

                auto& arch = *archPtr;

                for (uint32_t ci = 0; ci < arch.GetChunkCount(); ++ci) {
                    Chunk* ch = arch.GetChunk(ci);

                    for (uint32_t i = 0; i < ch->Count(); ++i) {
                        Entity ent = _reverseEntity(arch, ci, i);

                        if (ent.IsNull())
                            continue;
                        fn(ent, *reinterpret_cast<std::decay_t<Ts>*>(arch.GetRaw(TypeIdOf<std::decay_t<Ts>>(), ci, i))...);
                    }
                }
            }
        }

        // Deferred structural changes (for safe modifications during iteration)
        class DeferGuard {
        public:
            explicit DeferGuard(World& w) : m_world(w), m_active(true) { m_world.BeginDefer(); }
            ~DeferGuard() { if (m_active) m_world.EndDefer(); }
            DeferGuard(const DeferGuard&) = delete;
            DeferGuard& operator=(const DeferGuard&) = delete;
        private:
            World& m_world;
            bool m_active = false;
        };

        void BeginDefer() { ++m_deferDepth; }
        void EndDefer() {
            assert(m_deferDepth > 0);
            if (--m_deferDepth == 0) {
                for (auto& cmd : m_deferred)
                    cmd(*this);
                m_deferred.clear();
            }
        }
        bool Deferring() const noexcept { return m_deferDepth > 0; }

        // Relationships
        void Attach(Entity child, Entity parent) {
            Set<Parent>(child, Parent{parent});
        }
        void Detach(Entity child) {
            if (Has<Parent>(child)) Remove<Parent>(child);
        }
        template<typename TFn>
        void ForChildren(Entity parent, TFn&& fn) const {
            auto it = m_hierarchy.FirstChild.find(parent);
            if (it == m_hierarchy.FirstChild.end())
                return;

            Entity c = it->second;
            while (!c.IsNull()) {
                fn(c);
                auto itN = m_hierarchy.NextSibling.find(c);
                c = (itN == m_hierarchy.NextSibling.end()) ? NULL_ENTITY : itN->second;
            }
        }
        Entity ParentOf(Entity child) const {
            auto it = m_hierarchy.ParentOf.find(child);
            return it == m_hierarchy.ParentOf.end() ? NULL_ENTITY : it->second;
        }

        const std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash>& Archetypes() const { return m_archetypes; }

        struct Location {
        public:
            Archetype* ArchetypePtr = nullptr;
            uint32_t ChunkIndex = 0;
            uint32_t SlotIndex = 0;
        };
        const Location* LocationOf(Entity e) const {
            if (!Alive(e))
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
            auto it = m_archetypes.find(sig);
            if (it != m_archetypes.end())
                return it->second.get();

            std::vector<ComponentInfo> infos;
            infos.reserve(sig.Types().size());
            for (auto t : sig.Types()) {
                auto si = m_componentSizes[t];
                infos.push_back(ComponentInfo{ t, si.first, si.second });
            }

            auto arch = std::make_unique<Archetype>(sig, std::move(infos), m_chunkCapacity, m_chunkBytes);
            Archetype* ptr = arch.get();
            m_archetypes.emplace(sig, std::move(arch));
            m_chunkIndices[ptr] = ChunkIndex{};

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
        void _emplaceComponents(Entity e, const Ts&... comps) {
            (_ensureComponentInfo<Ts>(), ...);

            Signature sig({ TypeIdOf<Ts>()... });
            Archetype* to = _getOrCreateArchetype(sig);
            auto [ci, slot] = to->Insert();
            
            ( (new (to->GetRaw(TypeIdOf<Ts>(), ci, slot)) Ts(comps)), ... );

            _placeEntity(e, to, ci, slot);
            (_onComponentAdded(e, TypeIdOf<Ts>()), ...);
        }

        template<typename T, typename TMover>
        void _structuralAdd(Entity e, TypeId t, TMover&& mover) {
            _ensureComponentInfo<T>();
            auto& loc = m_locations[e.Index];

            if (!loc.ArchetypePtr) {
                Signature ns({ t });
                Archetype* to = _getOrCreateArchetype(ns);
                auto [ci, slot] = to->Insert();
                mover(e, to->GetRaw(t, ci, slot));
                _placeEntity(e, to, ci, slot);

                return;
            }

            if (loc.ArchetypePtr->Has(t)) {
                T* dst = reinterpret_cast<T*>(loc.ArchetypePtr->GetRaw(t, loc.ChunkIndex, loc.SlotIndex));
                *dst = T(std::forward<TMover>(mover), *dst); // not typically reached

                return;
            }

            Signature ns = loc.ArchetypePtr->GetSignature().MergedWith(t);
            Archetype* to = _getOrCreateArchetype(ns);
            auto [ci, slot] = to->Insert();

            for (auto& info : loc.ArchetypePtr->GetComponents()) {
                void* dst = to->GetRaw(info.Id, ci, slot);
                void* src = loc.ArchetypePtr->GetRaw(info.Id, loc.ChunkIndex, loc.SlotIndex);
                std::memcpy(dst, src, info.Size);
            }

            mover(e, to->GetRaw(t, ci, slot));
            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
        }

        template<typename T>
        void _structuralRemove(Entity e, TypeId t) {
            auto& loc = m_locations[e.Index];
            if (!loc.ArchetypePtr || !loc.ArchetypePtr->Has(t))
                return;
            
            Signature ns = loc.ArchetypePtr->GetSignature().Without(t);
            Archetype* to = ns.Types().empty() ? nullptr : _getOrCreateArchetype(ns);
            uint32_t fromChunk = loc.ChunkIndex;
            uint32_t fromSlot = loc.SlotIndex;

            if (!to) {
                _removeFromArchetype(e, loc);
                loc.ArchetypePtr = nullptr;

                return;
            }

            auto [ci, slot] = to->Insert();
            for (auto& info : loc.ArchetypePtr->GetComponents()) {
                if (info.Id == t) continue;

                void* dst = to->GetRaw(info.Id, ci, slot);
                void* src = loc.ArchetypePtr->GetRaw(info.Id, fromChunk, fromSlot);
                std::memcpy(dst, src, info.Size);
            }

            _removeFromArchetype(e, loc);
            _placeEntity(e, to, ci, slot);
        }

        void _placeEntity(Entity e, Archetype* arch, uint32_t chunkIndex, uint32_t slot) {
            m_locations[e.Index] = Location{ arch, chunkIndex, slot };

            auto &ci = m_chunkIndices[arch];
            if (ci.Entities.size() <= chunkIndex)
                ci.Entities.resize(chunkIndex + 1);

            auto &vec = ci.Entities[chunkIndex];
            if (vec.size() <= slot)
                vec.resize(arch->GetChunk(chunkIndex)->Capacity(), NULL_ENTITY);
                
            vec[slot] = e;
        }

        void _removeFromArchetype(Entity e, Location& loc) {
            Archetype* arch = loc.ArchetypePtr;
            auto &ci = m_chunkIndices[arch];
            auto &vec = ci.Entities[loc.ChunkIndex];
            uint32_t movedFrom = arch->RemoveSwapBack(loc.ChunkIndex, loc.SlotIndex);

            if (movedFrom != loc.SlotIndex) {
                Entity moved = vec[movedFrom];
                vec[loc.SlotIndex] = moved;
                vec[movedFrom] = NULL_ENTITY;
                auto &mloc = m_locations[moved.Index];
                mloc.SlotIndex = loc.SlotIndex;
            } 
            else {
                vec[loc.SlotIndex] = NULL_ENTITY;
            }

            loc.ArchetypePtr = nullptr;
        }

        Entity _reverseEntity(Archetype& arch, uint32_t chunkIndex, uint32_t slot) const {
            auto it = m_chunkIndices.find(&arch);
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

        void _onComponentAdded(Entity e, TypeId t) {
            if (t == TypeIdOf<Parent>()) {
                const auto& p = Get<Parent>(e);
                auto found = m_hierarchy.ParentOf.find(e);

                if (found != m_hierarchy.ParentOf.end()) {
                    if (found->second == p.ParentEntity)
                        return;
                    _unlinkChild(e);
                }

                if (p.ParentEntity.IsNull())
                    return;
                    
                Entity oldFirst = m_hierarchy.FirstChild[p.ParentEntity];
                m_hierarchy.FirstChild[p.ParentEntity] = e;

                if (!oldFirst.IsNull()) {
                    m_hierarchy.PrevSibling[oldFirst] = e;
                }

                m_hierarchy.NextSibling[e] = oldFirst;
                m_hierarchy.PrevSibling[e] = NULL_ENTITY;
                m_hierarchy.ParentOf[e] = p.ParentEntity;
            }
        }
        void _onComponentChanged(Entity e, TypeId t) {
            if (t == TypeIdOf<Parent>()) {
                _onComponentAdded(e, t);
            }
        }
        void _onComponentRemoving(Entity e, TypeId t) {
            if (t == TypeIdOf<Parent>()) {
                _unlinkChild(e);
                m_hierarchy.ParentOf.erase(e);
            }
        }

        void _unlinkChild(Entity e) {
            auto pit = m_hierarchy.ParentOf.find(e);
            if (pit == m_hierarchy.ParentOf.end())
                return;

            Entity par = pit->second;
            Entity prev = NULL_ENTITY, next = NULL_ENTITY;

            auto itPrev = m_hierarchy.PrevSibling.find(e);
            if (itPrev != m_hierarchy.PrevSibling.end())
                prev = itPrev->second;
            
            auto itNext = m_hierarchy.NextSibling.find(e);
            if (itNext != m_hierarchy.NextSibling.end())
                next = itNext->second;
            
            if (prev.IsNull()) {
                if (next.IsNull())
                    m_hierarchy.FirstChild.erase(par);
                else m_hierarchy.FirstChild[par] = next;
            }
            else {
                m_hierarchy.NextSibling[prev] = next;
            }

            if (!next.IsNull())
                m_hierarchy.PrevSibling[next] = prev;

            m_hierarchy.PrevSibling.erase(e);
            m_hierarchy.NextSibling.erase(e);
        }

    private:
        uint32_t m_chunkCapacity = 256;
        size_t m_chunkBytes = 16 * 1024;

        std::vector<uint32_t> m_generations;
        std::vector<uint32_t> m_free;
        std::vector<Location> m_locations;

        std::unordered_map<Signature, std::unique_ptr<Archetype>, SignatureHash> m_archetypes;
        std::unordered_map<Archetype*, ChunkIndex> m_chunkIndices;
        std::unordered_map<TypeId, std::pair<size_t,size_t>> m_componentSizes;

        HierarchyIndex m_hierarchy;

        using DeferredCmd = std::function<void(World&)>;
        std::vector<DeferredCmd> m_deferred;
        uint32_t m_deferDepth = 0;
    };
}

#endif
