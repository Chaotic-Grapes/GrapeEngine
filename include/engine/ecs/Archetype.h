/* Start Header *****************************************************************/
/*!
\file    Archetype.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the Archetype
class, responsible for managing a group of entities with the same
component composition. It provides methods for adding, removing, and
querying entities within the archetype, as well as handling chunk
allocation and memory management.

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
    struct ComponentInfo {
    public:
        TypeId Id;
        size_t Size;
        size_t Align;
    };

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
            _buildLayout(chunkTargetBytes);
            _newChunk();
        }

        /**
		 * @brief Gets the signature of this archetype.
		 * @return The signature representing the combination of components in this archetype.
         */
        const Signature& GetSignature() const noexcept          { return m_signature; }

        /**
		 * @brief Checks if the archetype contains a component of the given type.
		 * @param t The TypeId of the component to check for.
		 * @return True if the component type is present in the archetype, false otherwise.
         */
        bool Has(const TypeId t) const noexcept {
            // Use lower_bound with (element, value) comparator to avoid reversed comparator calls.
            auto it = std::lower_bound(
                m_componentInfos.begin(), m_componentInfos.end(), t,
                [](const ComponentInfo& ci, const TypeId v) { return ci.Id < v; }
            );
            return it != m_componentInfos.end() && it->Id == t;
        }

        /**
		 * @brief Gets the component index for the given component type.
		 * @param t The TypeId of the component.
		 * @return The index of the component within the archetype.
         */
        ComponentIndex GetComponentIndex(const TypeId t) const {
            auto it = std::lower_bound(
                m_componentInfos.begin(), m_componentInfos.end(), t,
                [](const ComponentInfo& ci, const TypeId v) { return ci.Id < v; }
            );
            assert(it != m_componentInfos.end() && it->Id == t);
            return static_cast<ComponentIndex>(static_cast<uint32_t>(it - m_componentInfos.begin()));
        }

        /**
		 * @brief Gets the number of chunks in this archetype.
		 * @return The number of chunks currently allocated in the archetype.
         */
        uint32_t GetChunkCount() const noexcept                 { return static_cast<uint32_t>(m_chunks.size()); }

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
            auto it = std::lower_bound(
                m_componentInfos.begin(), m_componentInfos.end(), t,
                [](const ComponentInfo& ci, const TypeId v) { return ci.Id < v; }
            );
            assert(it != m_componentInfos.end() && it->Id == t);
            const ComponentIndex idx = static_cast<ComponentIndex>(static_cast<uint32_t>(it - m_componentInfos.begin()));
            return static_cast<T*>(m_chunks[chunkIndex]->ComponentPtr(idx, slot));
        }

        /**
         * @brief Gets a raw pointer to the component of the specified type for the given chunk and slot.
         * @param t The TypeId of the component.
         * @param chunkIndex The index of the chunk.
         * @param slot The slot index within the chunk.
         * @return A void pointer to the component data.
         */
        void* GetRaw(const TypeId t, const uint32_t chunkIndex, const uint32_t slot) {
            auto it = std::lower_bound(
                m_componentInfos.begin(), m_componentInfos.end(), t,
                [](const ComponentInfo& ci, const TypeId v) { return ci.Id < v; }
            );
            assert(it != m_componentInfos.end() && it->Id == t);
            const ComponentIndex idx = static_cast<ComponentIndex>(static_cast<uint32_t>(it - m_componentInfos.begin()));
            return m_chunks[chunkIndex]->ComponentPtr(idx, slot);
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
        Signature m_signature;
        std::vector<ComponentInfo> m_componentInfos;
        std::vector<ChunkLayoutEntry> m_layout;
        std::vector<std::unique_ptr<Chunk>> m_chunks;
        uint32_t m_chunkCapacity = 0;
        size_t m_totalBytes = 0;
    };
}

#endif
