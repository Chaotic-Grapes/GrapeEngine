/* Start Header *****************************************************************/
/*!
\file    ParallelQuery.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of ParallelQuery - template specializations for building signatures
and querying the world.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/jobs/ParallelQuery.h"
#include "ecs/World.h"
#include "ecs/Archetype.h"

namespace ECS::Jobs {

    // ===================================================================
    // ParallelQuery Specialization: All Components
    // ===================================================================

    template<typename... Components>
    Signature ParallelQuery<Components...>::_buildSignature() {
        Signature sig;
        // Add component types to signature in order using MergedWith fold
        (..., (sig = sig.MergedWith(TypeIdOf<Components>())));
        return sig;
    }

    template<typename... Components>
    std::vector<Chunk*> ParallelQuery<Components...>::GetChunks() const {
        if (!m_world) return {};
        
        std::vector<Chunk*> result;
        
        // Get all archetypes and filter by signature match
        auto archetypes = m_world->GetAllArchetypes();
        for (auto* archetype : archetypes) {
            if (archetype && archetype->GetSignature().ContainsAll(m_signature)) {
                uint32_t chunkCount = archetype->GetChunkCount();
                for (uint32_t i = 0; i < chunkCount; ++i) {
                    auto* chunk = archetype->GetChunk(i);
                    if (chunk) {
                        result.push_back(chunk);
                    }
                }
            }
        }
        
        return result;
    }

    template<typename... Components>
    std::vector<Entity> ParallelQuery<Components...>::GetEntities() const {
        if (!m_world) return {};
        
        std::vector<Entity> result;
        auto chunks = GetChunks();
        
        for (auto* chunk : chunks) {
            if (!chunk) continue;
            
            uint32_t count = chunk->Count();
            const auto& entities = chunk->Entities();
            
            for (uint32_t i = 0; i < count && i < entities.size(); ++i) {
                if (entities[i] != NULL_ENTITY) {
                    result.push_back(entities[i]);
                }
            }
        }
        
        return result;
    }

    // ===================================================================
    // ParallelQuery Specialization: Empty (All Entities)
    // ===================================================================

    inline std::vector<Chunk*> ParallelQuery<>::GetChunks() const {
        if (!m_world) return {};
        
        std::vector<Chunk*> result;
        auto archetypes = m_world->GetAllArchetypes();
        
        for (auto* archetype : archetypes) {
            if (archetype) {
                uint32_t chunkCount = archetype->GetChunkCount();
                for (uint32_t i = 0; i < chunkCount; ++i) {
                    auto* chunk = archetype->GetChunk(i);
                    if (chunk) {
                        result.push_back(chunk);
                    }
                }
            }
        }
        
        return result;
    }

    inline std::vector<Entity> ParallelQuery<>::GetEntities() const {
        if (!m_world) return {};
        
        std::vector<Entity> result;
        auto chunks = GetChunks();
        
        for (auto* chunk : chunks) {
            if (!chunk) continue;
            
            uint32_t count = chunk->Count();
            const auto& entities = chunk->Entities();
            
            for (uint32_t i = 0; i < count && i < entities.size(); ++i) {
                if (entities[i] != NULL_ENTITY) {
                    result.push_back(entities[i]);
                }
            }
        }
        
        return result;
    }

}
