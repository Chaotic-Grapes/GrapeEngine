#ifndef CHUNK_H
#define CHUNK_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>
#include <cassert>

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
        Chunk(const uint32_t capacity, std::vector<ChunkLayoutEntry> layout, const size_t totalSize)
            : m_capacity(capacity), m_count(0), m_buffer(new uint8_t[totalSize]), m_layout(std::move(layout)) {}

        uint32_t Capacity() const noexcept        { return m_capacity; }
        uint32_t Count() const noexcept           { return m_count; }
        void SetCount(const uint32_t c) noexcept  { m_count = c; }

        void* ComponentPtr(const uint32_t compIndex, const uint32_t slot) noexcept {
            const auto &chunkLayoutEntry = m_layout[compIndex];
            return m_buffer.get() + chunkLayoutEntry.Offset + chunkLayoutEntry.Stride * static_cast<size_t>(slot);
        }
        const void* ComponentPtr(const uint32_t compIndex, const uint32_t slot) const noexcept {
            auto &e = m_layout[compIndex];
            return m_buffer.get() + e.Offset + e.Stride * static_cast<size_t>(slot);
        }

        uint32_t RemoveSwapBack(const uint32_t dst) noexcept {
            assert(m_count > 0 && dst < m_count);
            const uint32_t last = m_count - 1;
            if (dst != last) {
                for (size_t i = 0; i < m_layout.size(); ++i) {
                    std::memmove(ComponentPtr(static_cast<uint32_t>(i), dst),
                                ComponentPtr(static_cast<uint32_t>(i), last),
                                m_layout[i].Stride);
                }
            }
            --m_count;
            return last;
        }

    private:
        uint32_t m_capacity = 0;
        uint32_t m_count = 0;
        std::unique_ptr<uint8_t[]> m_buffer;
        std::vector<ChunkLayoutEntry> m_layout;
    };
}

#endif
