#ifndef CHUNK_H
#define CHUNK_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>
#include <cassert>
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
        Chunk(uint32_t capacity, std::vector<ChunkLayoutEntry> layout, size_t totalSize)
            : m_Capacity(capacity), m_Count(0), m_Buffer(new uint8_t[totalSize]), m_Layout(std::move(layout)) {}

        uint32_t Capacity() const noexcept { return m_Capacity; }
        uint32_t Count() const noexcept { return m_Count; }
        void SetCount(uint32_t c) noexcept { m_Count = c; }

        void* ComponentPtr(uint32_t compIndex, uint32_t slot) noexcept {
            auto &e = m_Layout[compIndex];
            return m_Buffer.get() + e.Offset + e.Stride * size_t(slot);
        }
        const void* ComponentPtr(uint32_t compIndex, uint32_t slot) const noexcept {
            auto &e = m_Layout[compIndex];
            return m_Buffer.get() + e.Offset + e.Stride * size_t(slot);
        }

        uint32_t RemoveSwapBack(uint32_t dst) noexcept {
            assert(m_Count > 0 && dst < m_Count);
            uint32_t last = m_Count - 1;
            if (dst != last) {
                for (size_t i = 0; i < m_Layout.size(); ++i) {
                    std::memmove(ComponentPtr(uint32_t(i), dst),
                                ComponentPtr(uint32_t(i), last),
                                m_Layout[i].Stride);
                }
            }
            --m_Count;
            return last;
        }

    private:
        uint32_t m_Capacity = 0;
        uint32_t m_Count = 0;
        std::unique_ptr<uint8_t[]> m_Buffer;
        std::vector<ChunkLayoutEntry> m_Layout;
    };
}

#endif
