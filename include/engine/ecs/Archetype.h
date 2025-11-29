/* Start Header *****************************************************************/
/*!
\file    Archetype.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the Archetype class,
responsible for managing entities with identical component compositions.
Features archetype graph edges for O(1) component add/remove transitions,
pre-computed component stride cache for fast iteration, and chunk defragmentation
to maintain memory locality. Handles chunk allocation, entity management, and
efficient component access patterns for high-performance ECS queries.

This class is not to be directly used by game code; instead,
use the provided ECS component management APIs.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef ARCHETYPE_H
#define ARCHETYPE_H

#include <algorithm>
#include <vector>
#include <unordered_map>
#include "ecs/Signature.h"
#include "ecs/Chunk.h"
#include "ecs/ComponentRegistry.h"

namespace ECS {
    // Info about each component type in the archetype
    struct ComponentInfo {
    public:
        TypeId Id;      // Component TypeId
        size_t Size;    // Size of the component in bytes
        size_t Align;   // Alignment of the component in bytes
    };

    // Archetype represents a unique combination of components.
    class Archetype {
    public:
        using ComponentIndex = uint32_t;

        /**
         * @brief Constructs a new Archetype with the given signature and component infos.
         * @param sig The signature representing the combination of components in this archetype.
         * @param infos A vector of ComponentInfo structures for each component type in the archetype.
         * @param chunkCapacity The maximum number of entities per chunk. If set to 0, it will be calculated based on chunkTargetBytes.
         * @param chunkTargetBytes The target size in bytes for each chunk. Used to calculate chunkCapacity if it is set to 0.
		 * @note Default value for chunkCapacity is 256 and for chunkTargetBytes is 16384 (16 KB * 1024).
		 */
        Archetype(Signature sig, std::vector<ComponentInfo> infos, const uint32_t chunkCapacity = 256, const size_t chunkTargetBytes = 16384)
            : m_signature(std::move(sig)), m_chunkCapacity(chunkCapacity) {
            // Component infos are provided in Signature order which is sorted by TypeId.
            // Keep them as-is to enable cache-friendly binary searches without hashing.
            m_componentInfos = std::move(infos);
            
            // Build lookup cache for O(1) component index retrieval
            for (size_t i = 0; i < m_componentInfos.size(); ++i) {
                m_componentIndexCache[m_componentInfos[i].Id] = static_cast<ComponentIndex>(i);
            }
            
            _buildLayout(chunkTargetBytes);
            _newChunk();
        }

        /**
		 * @brief Gets the signature of this archetype.
		 * @return The signature representing the combination of components in this archetype.
         */
        const Signature& GetSignature() const noexcept { return m_signature; }

        /**
		 * @brief Checks if the archetype contains a component of the given type.
		 * @param t The TypeId of the component to check for.
		 * @return True if the component type is present in the archetype, false otherwise.
         */
        inline bool Has(const TypeId t) const noexcept {
            // O(1) hash lookup instead of O(log n) binary search
            return m_componentIndexCache.find(t) != m_componentIndexCache.end();
        }

        /**
		 * @brief Gets the component index for the given component type.
		 * @param t The TypeId of the component.
		 * @return The index of the component within the archetype.
         */
        inline ComponentIndex GetComponentIndex(const TypeId t) const {
            // O(1) hash lookup instead of O(log n) binary search
            auto it = m_componentIndexCache.find(t);
            return it->second;
        }
        
        /**
         * @brief Gets the stride (size in bytes) for a component at the given index.
         * Pre-computed during layout build to avoid chunk lookups in iteration hot paths.
         * @param compIdx The component index within the archetype.
         * @return The stride (size) of the component in bytes.
         */
        inline size_t GetComponentStride(const ComponentIndex compIdx) const noexcept { return m_layout[compIdx].Stride; }

        /**
		 * @brief Gets the number of chunks in this archetype.
		 * @return The number of chunks currently allocated in the archetype.
         */
        uint32_t GetChunkCount() const noexcept { return static_cast<uint32_t>(m_chunks.size()); }

        /**
         * @brief Inserts a new entity into the archetype and returns its location.
         * @return A pair containing the chunk index and slot index of the newly inserted entity.
         */
        std::pair<uint32_t,uint32_t> Insert() {
            if (m_chunks.empty() || m_chunks.back()->Count() == m_chunks.back()->Capacity()) {
                _newChunk();
            }

            Chunk* c = m_chunks.back().get();
            uint32_t slot = c->Count();
            c->SetCount(slot + 1);

            return { static_cast<uint32_t>(m_chunks.size() - 1), slot };
        }

        /**
         * @brief Removes an entity from the archetype using the swap-back method.
         * @param chunkIndex The index of the chunk containing the entity.
         * @param slot The slot index of the entity within the chunk.
         * @return The slot index of the entity that was swapped into the removed entity's position
         */
        uint32_t RemoveSwapBack(const uint32_t chunkIndex, const uint32_t slot) {
            Chunk* c = m_chunks[chunkIndex].get();
            const uint32_t lastSlot = c->RemoveSwapBack(slot);

            if (c->Count() == 0 && m_chunks.size() > 1) {
                m_chunks.erase(m_chunks.begin() + chunkIndex);
                return lastSlot;
            }

            return lastSlot;
        }

        /**
         * @brief Gets a pointer to the component of type T for the specified chunk and slot.
         * @tparam T The component type.
         * @param chunkIndex The index of the chunk.
         * @param slot The slot index within the chunk.
         * @return A pointer to the component of type T.
         */
        template<typename T>
        T* Get(const uint32_t chunkIndex, const uint32_t slot) {
            const TypeId t = TypeIdOf<T>();

            // O(1) hash lookup
            auto it = m_componentIndexCache.find(t);            
            if (it == m_componentIndexCache.end()) {
                throw std::runtime_error("Component type not found in archetype.");
            }
            return static_cast<T*>(m_chunks[chunkIndex]->ComponentPtr(it->second, slot));
        }

        /**
         * @brief Gets a raw pointer to the component of the specified type for the given chunk and slot.
         * @param t The TypeId of the component.
         * @param chunkIndex The index of the chunk.
         * @param slot The slot index within the chunk.
         * @return A void pointer to the component data.
         */
        void* GetRaw(const TypeId t, const uint32_t chunkIndex, const uint32_t slot) {
            // O(1) hash lookup
            auto it = m_componentIndexCache.find(t);
            if (it == m_componentIndexCache.end()) {
                throw std::runtime_error("Component type not found in archetype.");
            }
            return m_chunks[chunkIndex]->ComponentPtr(it->second, slot);
        }

        /**
         * @brief Gets a pointer to the specified chunk.
         * @param chunkIndex The index of the chunk to retrieve.
         * @return A pointer to the requested Chunk.
         */
        Chunk* GetChunk(const uint32_t chunkIndex)                   { return m_chunks[chunkIndex].get(); }
        
        /**
         * @brief Gets a pointer to the specified chunk (const version).
         * @param chunkIndex The index of the chunk to retrieve.
         * @return A const pointer to the requested Chunk.
         */
        const Chunk* GetChunk(const uint32_t chunkIndex) const       { return m_chunks[chunkIndex].get(); }
        
        /**
         * @brief Gets the list of chunks in this archetype.
         * @return A const reference to the vector of unique pointers to Chunks.
         */
        const std::vector<std::unique_ptr<Chunk>>& GetChunks() const { return m_chunks; }

        /**
         * @brief Gets the component information for all components in this archetype.
         * @return A const reference to the vector of ComponentInfo structures.
         */
        const std::vector<ComponentInfo>& GetComponents() const      { return m_componentInfos; }
        
        /**
         * @brief Defragments chunks by consolidating sparse chunks into fewer, fuller chunks.
         * Called periodically to improve memory locality after many entity deletions.
         * Returns the number of entities moved during defragmentation.
         * 
         * @param minUtilization Only defrag if chunk utilization falls below this threshold (0.0 to 1.0)
         * @return Number of entities moved during defragmentation
         */
        // uint32_t Defragment(const float minUtilization = 0.5f) {
        //     if (m_chunks.size() <= 1) return 0; // Nothing to defragment with only one chunk
            
        //     uint32_t entitiesMoved = 0;
            
        //     // Work backwards through chunks, moving entities from sparse end chunks to fill earlier chunks
        //     for (int32_t srcIdx = static_cast<int32_t>(m_chunks.size()) - 1; srcIdx >= 0; --srcIdx) {
        //         Chunk* srcChunk = m_chunks[srcIdx].get();
                
        //         // Check if source chunk is sparse enough to warrant moving its entities
        //         const float utilization = static_cast<float>(srcChunk->Count()) / static_cast<float>(m_chunkCapacity);
        //         if (utilization >= minUtilization) continue; // Chunk is full enough, skip it
                
        //         // Try to move entities from this sparse chunk into earlier chunks with free space
        //         while (srcChunk->Count() > 0) {
        //             bool movedEntity = false;
                    
        //             // Find a destination chunk with free space (search from front for best packing)
        //             for (size_t dstIdx = 0; dstIdx < static_cast<size_t>(srcIdx); ++dstIdx) {
        //                 Chunk* dstChunk = m_chunks[dstIdx].get();
                        
        //                 // If destination chunk has space, move an entity from source to destination
        //                 if (dstChunk->Count() < m_chunkCapacity) {
        //                     // Get the last entity from source chunk (efficient swap-back removal)
        //                     const uint32_t srcSlot = srcChunk->Count() - 1;
        //                     const Entity entity = srcChunk->GetEntity(srcSlot);
                            
        //                     // Allocate slot in destination chunk
        //                     const uint32_t dstSlot = dstChunk->Count();
        //                     dstChunk->SetCount(dstSlot + 1);
        //                     dstChunk->SetEntity(dstSlot, entity);
                            
        //                     // Copy component data from source to destination
        //                     for (size_t compIdx = 0; compIdx < m_componentInfos.size(); ++compIdx) {
        //                         const uint32_t idx = static_cast<uint32_t>(compIdx);
        //                         std::memcpy(
        //                             dstChunk->ComponentPtr(idx, dstSlot),
        //                             srcChunk->ComponentPtr(idx, srcSlot),
        //                             m_layout[compIdx].Stride
        //                         );
        //                     }
                            
        //                     // Remove from source chunk (don't call RemoveSwapBack if already decremented count)
        //                     srcChunk->SetCount(srcSlot);
                            
        //                     // Mark both chunks dirty so queries reprocess them
        //                     srcChunk->MarkDirty();
        //                     dstChunk->MarkDirty();
                            
        //                     ++entitiesMoved;
        //                     movedEntity = true;
                            
        //                     // NOTE: Caller must update entity location tracking (m_locations in World)
        //                     // This function only moves the raw data, not the location metadata
                            
        //                     break; // Found destination, move next entity from source
        //                 }
        //             }
                    
        //             // If we couldn't move any entity, source chunk is as defragmented as possible
        //             if (!movedEntity)
        //                 break;
        //         }
                
        //         // If source chunk is now empty, remove it to free memory
        //         if (srcChunk->Count() == 0 && m_chunks.size() > 1) {
        //             m_chunks.erase(m_chunks.begin() + srcIdx);
        //         }
        //     }
            
        //     return entitiesMoved;
        // }
        
        /**
         * @brief Gets the archetype you reach by adding a component (archetype graph edge).
         * Used for O(1) component transitions instead of O(n) archetype search.
         * Returns nullptr if the edge hasn't been cached yet.
         * @param componentType The TypeId of the component being added
         * @return Pointer to the destination archetype, or nullptr if not cached
         */
        Archetype* GetAddEdge(const TypeId componentType) const {
            auto it = m_addEdges.find(componentType);
            return (it != m_addEdges.end()) ? it->second : nullptr;
        }
        
        /**
         * @brief Gets the archetype you reach by removing a component (archetype graph edge).
         * Used for O(1) component transitions instead of O(n) archetype search.
         * Returns nullptr if the edge hasn't been cached yet.
         * @param componentType The TypeId of the component being removed
         * @return Pointer to the destination archetype, or nullptr if not cached
         */
        Archetype* GetRemoveEdge(const TypeId componentType) const {
            auto it = m_removeEdges.find(componentType);
            return (it != m_removeEdges.end()) ? it->second : nullptr;
        }
        
        /**
         * @brief Caches an archetype graph edge for adding a component.
         * Call this when an archetype transition is discovered to cache the result.
         * Future transitions with the same component will be O(1) instead of O(n).
         * @param componentType The TypeId of the component being added
         * @param targetArchetype The archetype you reach by adding this component
         */
        void SetAddEdge(const TypeId componentType, Archetype* targetArchetype) {
            // Cache this transition for instant lookup next time
            // This transforms "add Transform component" from O(n) archetype scan to O(1) hash lookup
            m_addEdges[componentType] = targetArchetype;
        }
        
        /**
         * @brief Caches an archetype graph edge for removing a component.
         * Call this when an archetype transition is discovered to cache the result.
         * Future transitions with the same component will be O(1) instead of O(n).
         * @param componentType The TypeId of the component being removed
         * @param targetArchetype The archetype you reach by removing this component
         */
        void SetRemoveEdge(const TypeId componentType, Archetype* targetArchetype) {
            // Cache this transition for instant lookup next time
            // This transforms "remove Transform component" from O(n) archetype scan to O(1) hash lookup
            m_removeEdges[componentType] = targetArchetype;
        }

    private:
        void _buildLayout(const size_t targetBytes) {
            size_t totalStride = 0;
            for (const auto& i : m_componentInfos) 
                totalStride += i.Size;
                
            if (m_chunkCapacity == 0) {
                m_chunkCapacity = static_cast<uint32_t>(std::max<size_t>(1, targetBytes / std::max<size_t>(1, totalStride)));

                m_chunkCapacity = std::min<uint32_t>(m_chunkCapacity, 1024);
            }
            
            m_layout.resize(m_componentInfos.size());
            size_t offset = 0;
            auto alignUp = [](const size_t x, const size_t a) {
	            return (x + (a - 1)) & ~(a - 1);
            };

            for (size_t i = 0; i < m_componentInfos.size(); ++i) {
	            constexpr size_t ALIGN_64B = 64;
	            auto& ci = m_componentInfos[i];

                offset = alignUp(offset, std::max<size_t>(ALIGN_64B, ci.Align));
                m_layout[i] = ChunkLayoutEntry{ offset, ci.Size, ci.Align };
                offset += ci.Size * static_cast<size_t>(m_chunkCapacity);
            }
            
            m_totalBytes = offset;
        }

        void _newChunk() {
            auto ch = std::make_unique<Chunk>(m_chunkCapacity, m_layout, m_totalBytes);
            m_chunks.push_back(std::move(ch));
        }

    private:
        Signature m_signature;                        // Sorted set of component TypeIds defining this archetype
        std::vector<ComponentInfo> m_componentInfos;  // Size/alignment info per component
        std::unordered_map<TypeId, ComponentIndex> m_componentIndexCache; // O(1) component lookup cache
        std::vector<ChunkLayoutEntry> m_layout;       // Memory layout per component (offset, stride, align)
        std::vector<std::unique_ptr<Chunk>> m_chunks; // Contiguous memory blocks storing entities
        uint32_t m_chunkCapacity = 0;                 // Max entities per chunk (calculated or specified)
        size_t m_totalBytes = 0;                      // Total bytes per chunk including all components

        // Archetype Graph Edges: O(1) component transition cache
        // Instead of searching all archetypes when adding/removing components, we cache transitions
        // Key: ComponentTypeId being added/removed, Value: Destination archetype pointer
        // Example: Entity with [Transform, Renderer] adds Rigidbody -> cached transition to [Transform, Renderer, Rigidbody]
        mutable std::unordered_map<TypeId, Archetype*> m_addEdges;     // Cache for "add component" transitions
        mutable std::unordered_map<TypeId, Archetype*> m_removeEdges;  // Cache for "remove component" transitions
        // mutable keyword means these can be modified even in const methods
    };
}

#endif
