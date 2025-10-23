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
    struct ChunkLayoutEntry {
    public:
        size_t Offset = 0;
        size_t Stride = 0;
        size_t Align = 1;
    };

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
            : m_capacity(capacity), m_count(0), m_buffer(new uint8_t[totalSize]), m_layout(std::move(layout)) {
            m_entities.resize(capacity, NULL_ENTITY);
        }

        /**
         * @brief Gets the capacity of the chunk.
         * @return The maximum number of entities this chunk can hold.
         */
        uint32_t Capacity() const noexcept        { return m_capacity; }

        /**
         * @brief Gets the current count of entities in the chunk.
         * @return The number of entities currently stored in the chunk.
         */
        uint32_t Count() const noexcept           { return m_count; }
        
        /**
         * @brief Sets the current count of entities in the chunk.
         * @param c The new count of entities.
         */
        void SetCount(const uint32_t c) noexcept  { m_count = c; }

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
                // Swap component data
                for (size_t i = 0; i < m_layout.size(); ++i) {
                    std::memmove(ComponentPtr(static_cast<uint32_t>(i), dst),
                                ComponentPtr(static_cast<uint32_t>(i), last),
                                m_layout[i].Stride);
                }
                // Swap entity IDs
                m_entities[dst] = m_entities[last];
            }
            // Clear the last slot
            m_entities[last] = NULL_ENTITY;
            --m_count;
            return last;
        }

    private:
        uint32_t m_capacity = 0;
        uint32_t m_count = 0;
        std::unique_ptr<uint8_t[]> m_buffer;
        std::vector<ChunkLayoutEntry> m_layout;
        std::vector<Entity> m_entities;  // Entity IDs stored directly in chunk
    };
}

#endif
