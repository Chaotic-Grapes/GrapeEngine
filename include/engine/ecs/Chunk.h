/* Start Header *****************************************************************/
/*!
\file    Chunk.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration and definition of the Chunk class,
responsible for managing a chunk of entities and their components. It provides
methods for adding, removing, and accessing components within the chunk, as well
as handling memory allocation and deallocation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef CHUNK_H
#define CHUNK_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>
#include "ecs/Entity.h"

namespace ECS {
    // Layout entry for each component type in the chunk
    struct ChunkLayoutEntry {
    public:
        size_t Offset = 0; // Offset of the component data within the chunk
        size_t Stride = 0; // Stride (size) of the component type
        size_t Align = 1;  // Alignment requirement of the component type
    };

    // Chunk represents a contiguous block of memory for storing entities and their components.
    class Chunk {
    public:
        Chunk() = default;

        /**
         * @brief Constructs a new Chunk.
         * @param capacity The maximum number of entities this chunk can hold.
         * @param layout The layout of the chunk, describing the components it contains.
         * @param totalSize The total size of the chunk's buffer.
         */
        Chunk(const uint32_t capacity, std::vector<ChunkLayoutEntry> layout, const size_t totalSize)
            : m_capacity(capacity), m_count(0), m_version(0), m_buffer(new uint8_t[totalSize]), m_layout(std::move(layout)) {
            // Preallocate slots for all potential entities in this chunk
            m_entities.resize(capacity, NULL_ENTITY);
        }

        /**
         * @brief Gets the capacity of the chunk.
         * @return The maximum number of entities this chunk can hold.
         */
        uint32_t Capacity() const noexcept { return m_capacity; }

        /**
         * @brief Gets the current count of entities in the chunk.
         * @return The number of entities currently stored in the chunk.
         */
        uint32_t Count() const noexcept { return m_count; }
        
        /**
         * @brief Sets the current count of entities in the chunk.
         * @param c The new count of entities.
         */
        void SetCount(const uint32_t c) noexcept  { 
            m_count = c; 
            // Increment version when count changes to signal structural modifications
            // to allow queries to skip this chunk if they have already processed this version
            MarkDirty();
        }
        
        /**
         * @brief Gets the current version of the chunk.
         * @return The version number, incremented each time chunk data is modified.
         */
        uint64_t Version() const noexcept { return m_version; }
        
        /**
         * @brief Marks the chunk as modified by incrementing its version.
         * Called whenever component data changes to enable incremental query processing.
         * Queries can track the last processed version to skip unchanged chunks.
         */
        void MarkDirty() noexcept { ++m_version; }

        /**
         * @brief Gets the entity at the specified slot index.
         * @param slot The slot index within the chunk.
         * @return The entity at the specified slot.
         */
        inline Entity GetEntity(const uint32_t slot) const noexcept {
            return m_entities[slot];
        }

        /**
         * @brief Sets the entity at the specified slot index.
         * @param slot The slot index within the chunk.
         * @param entity The entity to set at the specified slot.
         */
        inline void SetEntity(const uint32_t slot, const Entity entity) noexcept {
            m_entities[slot] = entity;
            
            // Mark chunk as dirty since entity assignment modifies chunk data
            MarkDirty();
        }

        /**
         * @brief Gets the vector of all entities in this chunk.
         * @return A const reference to the vector of entities.
         */
        const std::vector<Entity>& Entities() const noexcept { return m_entities; }

        /**
         * @brief Gets a pointer to the component of the specified type for the given slot.
         * @param compIndex The index of the component type in the chunk layout.
         * @param slot The slot index within the chunk.
         * @return A void pointer to the component data.
         */
        void* ComponentPtr(const uint32_t compIndex, const uint32_t slot) noexcept {
            const auto &chunkLayoutEntry = m_layout[compIndex];
            return m_buffer.get() + chunkLayoutEntry.Offset + chunkLayoutEntry.Stride * static_cast<size_t>(slot);
        }

        /**
         * @brief Gets a const pointer to the component of the specified type for the given slot.
         * @param compIndex The index of the component type in the chunk layout.
         * @param slot The slot index within the chunk.
         * @return A const void pointer to the component data.
         */
        const void* ComponentPtr(const uint32_t compIndex, const uint32_t slot) const noexcept {
            auto &e = m_layout[compIndex];
            return m_buffer.get() + e.Offset + e.Stride * static_cast<size_t>(slot);
        }

        /**
         * @brief Gets the base pointer to a component array for optimized iteration.
         * @param compIndex The index of the component type in the chunk layout.
         * @return A void pointer to the start of the component array.
         */
        inline void* ComponentBase(const uint32_t compIndex) noexcept {
            return m_buffer.get() + m_layout[compIndex].Offset;
        }

        /**
         * @brief Gets the base pointer to a component array for optimized iteration (const version).
         * @param compIndex The index of the component type in the chunk layout.
         * @return A const void pointer to the start of the component array.
         */
        inline const void* ComponentBase(const uint32_t compIndex) const noexcept {
            return m_buffer.get() + m_layout[compIndex].Offset;
        }

        /**
         * @brief Gets the stride (size) of a component type in the chunk layout.
         * @param compIndex The index of the component type in the chunk layout.
         * @return The stride (size in bytes) of the component.
         */
        inline size_t ComponentStride(const uint32_t compIndex) const noexcept {
            return m_layout[compIndex].Stride;
        }

        /**
         * @brief Removes an entity from the chunk using the swap-back method.
         * @param dst The slot index of the entity to remove.
         * @return The slot index of the entity that was swapped into the removed entity's position.
         */
        uint32_t RemoveSwapBack(const uint32_t dst) noexcept {
            const uint32_t last = m_count - 1;
            if (dst != last) {
                // Swap component data from last entity into the removed entity's slot
                for (size_t i = 0; i < m_layout.size(); ++i) {
                    std::memmove(ComponentPtr(static_cast<uint32_t>(i), dst),
                                ComponentPtr(static_cast<uint32_t>(i), last),
                                m_layout[i].Stride);
                }
                // Swap entity IDs
                m_entities[dst] = m_entities[last];
            }

            // Clear the last slot to prevent dangling entity references
            m_entities[last] = NULL_ENTITY;
            --m_count;
            
            // Mark chunk dirty after removal so queries can detect structural changes
            // Critical for incremental processing - ensures queries reprocess affected chunks
            MarkDirty();
            
            return last;
        }

    private:
        uint32_t m_capacity = 0;  // Maximum entities this chunk can hold (determines memory allocation)
        uint32_t m_count = 0;     // Current number of entities in this chunk (0 to m_capacity)
        uint64_t m_version = 0;   // Incremented on any modification - enables incremental query processing
                                  // Queries track last seen version to skip unchanged chunks (massive speedup)
        std::unique_ptr<uint8_t[]> m_buffer;        // Raw memory buffer storing all component data
        std::vector<ChunkLayoutEntry> m_layout;     // Describes where each component type lives in the buffer
        std::vector<Entity> m_entities;             // Entity IDs stored directly in chunk for quick entity -> slot lookup
    };
}

#endif
