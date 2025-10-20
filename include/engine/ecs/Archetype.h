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
            for (uint32_t i = 0; i < infos.size(); ++i) {
                m_idToCompIndex[infos[i].Id] = i;
            }

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
        bool Has(const TypeId t) const noexcept                 { return m_idToCompIndex.find(t) != m_idToCompIndex.end(); }

        /**
		 * @brief Gets the component index for the given component type.
		 * @param t The TypeId of the component.
		 * @return The index of the component within the archetype.
         */
        ComponentIndex GetComponentIndex(const TypeId t) const  { return m_idToCompIndex.at(t); }

        /**
		 * @brief Gets the number of chunks in this archetype.
		 * @return The number of chunks currently allocated in the archetype.
         */
        uint32_t GetChunkCount() const noexcept                 { return static_cast<uint32_t>(m_chunks.size()); }

        std::pair<uint32_t,uint32_t> Insert() {
            if (m_chunks.empty() || m_chunks.back()->Count() == m_chunks.back()->Capacity()) {
                _newChunk();
            }

            Chunk* c = m_chunks.back().get();
            uint32_t slot = c->Count();
            c->SetCount(slot + 1);

            return { static_cast<uint32_t>(m_chunks.size() - 1), slot };
        }

        uint32_t RemoveSwapBack(const uint32_t chunkIndex, const uint32_t slot) {
            Chunk* c = m_chunks[chunkIndex].get();
            const uint32_t lastSlot = c->RemoveSwapBack(slot);

            if (c->Count() == 0 && m_chunks.size() > 1) {
                m_chunks.erase(m_chunks.begin() + chunkIndex);
                return lastSlot;
            }

            return lastSlot;
        }

        template<typename T>
        T* Get(const uint32_t chunkIndex, const uint32_t slot) {
            auto it = m_idToCompIndex.find(TypeIdOf<T>());
            assert(it != m_idToCompIndex.end());

            return static_cast<T*>(m_chunks[chunkIndex]->ComponentPtr(it->second, slot));
        }
        void* GetRaw(const TypeId t, const uint32_t chunkIndex, const uint32_t slot) {
            const auto it = m_idToCompIndex.find(t);
            assert(it != m_idToCompIndex.end());

            return m_chunks[chunkIndex]->ComponentPtr(it->second, slot);
        }

        Chunk* GetChunk(const uint32_t chunkIndex)                   { return m_chunks[chunkIndex].get(); }
        const Chunk* GetChunk(const uint32_t chunkIndex) const       { return m_chunks[chunkIndex].get(); }
        const std::vector<std::unique_ptr<Chunk>>& GetChunks() const { return m_chunks; }
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
        std::unordered_map<TypeId, ComponentIndex> m_idToCompIndex;
        std::vector<ComponentInfo> m_componentInfos;
        std::vector<ChunkLayoutEntry> m_layout;
        std::vector<std::unique_ptr<Chunk>> m_chunks;
        uint32_t m_chunkCapacity = 0;
        size_t m_totalBytes = 0;
    };
}

#endif
