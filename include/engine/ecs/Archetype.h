#ifndef ARCHETYPE_H
#define ARCHETYPE_H

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

        Archetype(Signature sig, std::vector<ComponentInfo> infos, uint32_t chunkCapacity = 256, size_t chunkTargetBytes = 16 * 1024)
            : m_signature(std::move(sig)), m_chunkCapacity(chunkCapacity) {
            for (uint32_t i = 0; i < infos.size(); ++i) {
                m_idToCompIndex[infos[i].Id] = i;
            }

            m_componentInfos = std::move(infos);
            _BuildLayout(chunkTargetBytes);
            _NewChunk();
        }

        const Signature& GetSignature() const noexcept   { return m_signature; }
        bool Has(TypeId t) const noexcept                { return m_idToCompIndex.find(t) != m_idToCompIndex.end(); }
        ComponentIndex GetComponentIndex(TypeId t) const { return static_cast<ComponentIndex>(m_idToCompIndex.at(t)); }
        uint32_t GetChunkCount() const noexcept          { return static_cast<uint32_t>(m_chunks.size()); }

        std::pair<uint32_t,uint32_t> Insert() {
            if (m_chunks.empty() || m_chunks.back()->Count() == m_chunks.back()->Capacity()) {
                _NewChunk();
            }

            Chunk* c = m_chunks.back().get();
            uint32_t slot = c->Count();
            c->SetCount(slot + 1);

            return { uint32_t(m_chunks.size()-1), slot };
        }

        uint32_t RemoveSwapBack(uint32_t chunkIndex, uint32_t slot) {
            Chunk* c = m_chunks[chunkIndex].get();
            uint32_t lastSlot = c->RemoveSwapBack(slot);

            if (c->Count() == 0 && m_chunks.size() > 1) {
                m_chunks.erase(m_chunks.begin() + chunkIndex);
                return lastSlot;
            }

            return lastSlot;
        }

        template<typename T>
        T* Get(uint32_t chunkIndex, uint32_t slot) {
            auto it = m_idToCompIndex.find(TypeIdOf<T>());
            assert(it != m_idToCompIndex.end());

            return reinterpret_cast<T*>(m_chunks[chunkIndex]->ComponentPtr(it->second, slot));
        }
        void* GetRaw(TypeId t, uint32_t chunkIndex, uint32_t slot) {
            auto it = m_idToCompIndex.find(t);
            assert(it != m_idToCompIndex.end());

            return m_chunks[chunkIndex]->ComponentPtr(it->second, slot);
        }

        Chunk* GetChunk(uint32_t chunkIndex)                         { return m_chunks[chunkIndex].get(); }
        const Chunk* GetChunk(uint32_t chunkIndex) const             { return m_chunks[chunkIndex].get(); }
        const std::vector<std::unique_ptr<Chunk>>& GetChunks() const { return m_chunks; }
        const std::vector<ComponentInfo>& GetComponents() const      { return m_componentInfos; }

    private:
        void _BuildLayout(size_t targetBytes) {
            size_t totalStride = 0;
            for (auto& i : m_componentInfos) 
                totalStride += i.Size;
                
            if (m_chunkCapacity == 0) {
                m_chunkCapacity = uint32_t(std::max<size_t>(1, targetBytes / std::max<size_t>(1, totalStride)));

                if (m_chunkCapacity > 1024) 
                    m_chunkCapacity = 1024;
            }
            
            m_layout.resize(m_componentInfos.size());
            size_t offset = 0;
            constexpr size_t ALIGN_64B = 64;
            auto AlignUp = [](size_t x, size_t a) { return (x + (a - 1)) & ~(a - 1); };

            for (size_t i = 0; i < m_componentInfos.size(); ++i) {
                auto& ci = m_componentInfos[i];

                offset = AlignUp(offset, std::max<size_t>(ALIGN_64B, ci.Align));
                m_layout[i] = ChunkLayoutEntry{ offset, ci.Size, ci.Align };
                offset += ci.Size * size_t(m_chunkCapacity);
            }
            
            m_totalBytes = offset;
        }

        void _NewChunk() {
            auto ch = std::make_unique<Chunk>(m_chunkCapacity, m_layout, m_totalBytes);
            m_chunks.push_back(std::move(ch));
        }

    private:
        Signature m_signature;
        std::unordered_map<TypeId, ComponentIndex> m_idToCompIndex;
        std::vector<ComponentInfo> m_componentInfos;
        std::vector<ChunkLayoutEntry> m_layout;
        std::vector<std::unique_ptr<Chunk>> m_chunks;
        uint32_t m_chunkCapacity = 0;
        size_t m_totalBytes = 0;
    };
}

#endif
