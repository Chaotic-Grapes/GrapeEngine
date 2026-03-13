/* Start Header *****************************************************************/
/*!
\file    Interop_World.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of World/Entity interop functions for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include "ecs/events/EventComponents.h"
#include "core/Logger.h"
#include <cstring>
#include <limits>
#include <combaseapi.h>

#ifdef ERROR
#undef ERROR
#endif

// ============================================================================
// Query Iterator Structure
// ============================================================================

/**
 * @brief Structure representing query iteration state
 *
 * Used by WorldInterop_CreateQuery and iteration helpers to keep track of
 * matched archetypes, chunks, and entity positions during a query.
 * 
 * SAFETY NOTES:
 * - The 'archetypes' pointer points to World::GetMatchingArchetypes(&matched) vector
 * - This reference becomes INVALID if archetypes are created/destroyed during iteration
 * - RISK: Nested queries or structural changes during iteration will cause undefined behavior
 * - Current mitigation: Single-threaded execution and queries must complete before World changes
 * 
 * RECOMMENDED FIX:
 * - Cache archetype list snapshot in the iterator at creation time, OR
 * - Add reentrant guard to prevent structural changes during active queries, OR
 * - Document clearly that queries are not reentrant-safe
 */
struct QueryIterator {
    void* worldPtr;
    const void* archetypes;      // std::vector<Archetype*>*
    uint32_t archetypeIndex;
    uint32_t chunkIndex;
    uint32_t entityIndex;
    uint32_t componentCount;
    uint32_t componentTypeIds[8]; // Max 8 required components in a query
    uint32_t optionalCount;
    uint32_t optionalTypeIds[8];
    uint32_t excludeCount;
    uint32_t excludeTypeIds[8];
};

namespace {
    // Convert World pointer
    ECS::World* GetWorld(void* worldPtr) {
        return static_cast<ECS::World*>(worldPtr);
    }

    // Compute FNV-1a hash for component names.
    constexpr uint32_t FNV1aHash(const char* str) {
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str);
            hash *= 16777619u;
            ++str;
        }
        return hash;
    }

    // Component type hash lookup
    ECS::ComponentTypeId GetComponentIdFromHash(uint32_t typeHash) {
        // Use ComponentRegistry to look up component ID from hash
        // This assumes ComponentRegistry has been populated
        return ECS::ComponentRegistry::GetComponentIdFromHash(typeHash);
    }
}

// ============================================================================
// Entity Lifecycle Operations
// ============================================================================

/**
 * @brief Create an entity and add its default LocalTransform component.
 * @param worldPtr Pointer to the World instance.
 * @return uint64_t Packed entity id, or 0 on failure.
 */
INTEROP_API uint64_t WorldInterop_CreateEntity(void* worldPtr) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = world->Create();
    
    // Add default LocalTransform component
    world->Add<ECS::Components::LocalTransform>(entity);
    
    // Pack the entity handle into a 64-bit ID.
    return ECS::EntityUtils::Pack(entity);
}

/**
 * @brief Destroy an entity from scripting.
 */
INTEROP_API void WorldInterop_DestroyEntity(void* worldPtr, uint64_t entityId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (world->IsAlive(entity)) {
        world->Destroy(entity);
    }
}

/**
 * @brief Instantiate an entity from the archetype list index.
 */
INTEROP_API uint64_t WorldInterop_InstantiateFromArchetype(void* worldPtr, uint32_t archetypeIndex) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    const auto archetypes = world->GetAllArchetypes(); // Use current archetype list; caller provides index into this snapshot.

    if (archetypeIndex >= archetypes.size()) {
        LOG_WARNING("[WorldInterop] Archetype index out of range: " << archetypeIndex);
        return 0;
    }

    ECS::Archetype* archetype = archetypes[archetypeIndex];
    if (!archetype) {
        LOG_WARNING("[WorldInterop] Archetype pointer is null for index: " << archetypeIndex);
        return 0;
    }

    // Instantiate entity from archetype
    ECS::Entity entity = world->Create();
    for (const auto& info : archetype->GetComponents()) {
        world->AddComponentById(entity, info.Id, nullptr, info.Size); // Default-construct each component for the archetype.
    }

    // Pack the entity handle into a 64-bit ID.
    return ECS::EntityUtils::Pack(entity);
}

/**
 * @brief Return the archetype ID for an entity.
 */
INTEROP_API uint32_t WorldInterop_GetArchetypeId(void* worldPtr, uint64_t entityId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);

    if (!world->IsAlive(entity)) {
        LOG_WARNING("[WorldInterop] GetArchetypeId failed: entity is not alive");
        return 0;
    }

    // Compute archetype ID based on entity's component signature
    // This assumes that the archetype ID is derived from the component signature hash
    const auto componentIds = world->GetEntityComponents(entity);
    ECS::Signature signature(componentIds);
    const ECS::SignatureHash hasher;
    return static_cast<uint32_t>(hasher(signature));
}

/**
 * @brief Instantiate an entity from an archetype ID.
 */
INTEROP_API uint64_t WorldInterop_InstantiateFromArchetypeId(void* worldPtr, uint32_t archetypeId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    const auto archetypes = world->GetAllArchetypes();

    // Find archetype matching the given archetype ID
    ECS::SignatureHash hasher;
    ECS::Archetype* matched = nullptr;
    for (auto* archetype : archetypes) {
        if (!archetype) continue;
        if (static_cast<uint32_t>(hasher(archetype->GetSignature())) == archetypeId) {
            matched = archetype;
            break;
        }
    }

    if (!matched) {
        LOG_WARNING("[WorldInterop] Archetype ID not found: " << archetypeId);
        return 0;
    }

    // Instantiate entity from matched archetype
    ECS::Entity entity = world->Create();
    for (const auto& info : matched->GetComponents()) {
        world->AddComponentById(entity, info.Id, nullptr, info.Size);
    }

    // Pack the entity handle into a 64-bit ID.
    return ECS::EntityUtils::Pack(entity);
}

/**
 * @brief Clone an entity and return the new ID.
 */
INTEROP_API uint64_t WorldInterop_CloneEntity(void* worldPtr, uint64_t entityId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);

    if (!world->IsAlive(entity)) {
        LOG_WARNING("[WorldInterop] CloneEntity failed: entity is not alive");
        return 0;
    }

    ECS::Entity cloned = world->Clone(entity); // Preserve all components using the native clone path.
    // Pack the entity handle into a 64-bit ID.
    return ECS::EntityUtils::Pack(cloned);
}

/**
 * @brief Check whether an entity is still alive.
 */
INTEROP_API bool WorldInterop_IsEntityAlive(void* worldPtr, uint64_t entityId) {
    if (!worldPtr) {
        return false;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    return world->IsAlive(entity);
}

// ============================================================================
// Component Operations
// ============================================================================

/**
 * @brief Check whether an entity has a component by type hash.
 * @param worldPtr Pointer to the World instance.
 * @param entityId Packed entity id.
 * @param componentTypeHash FNV-1a hash of the component type.
 * @return true if the entity has the component, false otherwise.
 */
INTEROP_API bool WorldInterop_HasComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash) {
    if (!worldPtr) {
        return false;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return false;
    }

    ECS::ComponentTypeId componentId = GetComponentIdFromHash(componentTypeHash);
    if (componentId == ECS::NULL_COMPONENT_ID) {
        LOG_WARNING("[WorldInterop] Unknown component type hash: " << componentTypeHash);
        return false;
    }

    return world->HasById(entity, componentId);
}

/**
 * @brief Return a raw pointer to a component by type hash.
 */
INTEROP_API void* WorldInterop_GetComponentPtr(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash) {
    if (!worldPtr) {
        return nullptr;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return nullptr;
    }

    ECS::ComponentTypeId componentId = GetComponentIdFromHash(componentTypeHash);
    if (componentId == ECS::NULL_COMPONENT_ID) {
        LOG_WARNING("[WorldInterop] Unknown component type hash: " << componentTypeHash);
        return nullptr;
    }

    // Get raw component pointer
    return world->GetRawComponentPtr(entity, componentId);
}

/**
 * @brief Add a component by type hash.
 */
INTEROP_API void* WorldInterop_AddComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, void* componentData, int componentSize) {
    if (!worldPtr) {
        return nullptr;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return nullptr;
    }

    ECS::ComponentTypeId componentId = GetComponentIdFromHash(componentTypeHash);
    if (componentId == ECS::NULL_COMPONENT_ID) {
        LOG_WARNING("[WorldInterop] Unknown component type hash: " << componentTypeHash);
        return nullptr;
    }

    // Add component and copy data
    void* addedComponentPtr = world->AddComponentById(entity, componentId, componentData, componentSize);
    
    return addedComponentPtr;
}

/**
 * @brief Remove a component by type hash.
 */
INTEROP_API void WorldInterop_RemoveComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash) {
    if (!worldPtr) {
        return;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    
    if (!world->IsAlive(entity)) {
        return;
    }

    ECS::ComponentTypeId componentId = GetComponentIdFromHash(componentTypeHash);
    if (componentId == ECS::NULL_COMPONENT_ID) {
        LOG_WARNING("[WorldInterop] Unknown component type hash: " << componentTypeHash);
        return;
    }

    world->RemoveById(entity, componentId);
}

// ============================================================================
// Hierarchy Operations
// ============================================================================

namespace {
    // Validate packed entity IDs before unpacking.
    uint64_t InvalidPackedEntityId() {
        return (std::numeric_limits<uint64_t>::max)();
    }
}

/**
 * @brief Attach a child entity to a parent.
 */
INTEROP_API void WorldInterop_AttachChild(void* worldPtr, uint64_t childId, uint64_t parentId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity child = ECS::EntityUtils::Unpack(childId);
    ECS::Entity parent = ECS::EntityUtils::Unpack(parentId);

    if (!world->IsAlive(child) || !world->IsAlive(parent)) {
        LOG_WARNING("[WorldInterop] AttachChild failed: child or parent is not alive");
        return;
    }

    world->Attach(child, parent);
}

/**
 * @brief Detach a child entity from its parent.
 */
INTEROP_API void WorldInterop_DetachChild(void* worldPtr, uint64_t childId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity child = ECS::EntityUtils::Unpack(childId);

    if (!world->IsAlive(child)) {
        LOG_WARNING("[WorldInterop] DetachChild failed: child is not alive");
        return;
    }

    world->Detach(child);
}

/**
 * @brief Return the parent entity ID.
 */
INTEROP_API uint64_t WorldInterop_GetParent(void* worldPtr, uint64_t childId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return InvalidPackedEntityId();
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity child = ECS::EntityUtils::Unpack(childId);
    if (!world->IsAlive(child)) {
        LOG_WARNING("[WorldInterop] GetParent failed: child is not alive");
        return InvalidPackedEntityId();
    }

    ECS::Entity parent = world->ParentOf(child);
    if (parent.IsNull() || !world->IsAlive(parent)) {
        return InvalidPackedEntityId();
    }

    // Pack the entity handle into a 64-bit ID.
    return ECS::EntityUtils::Pack(parent);
}

/**
 * @brief Return the first child entity ID.
 */
INTEROP_API uint64_t WorldInterop_GetFirstChild(void* worldPtr, uint64_t parentId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return InvalidPackedEntityId();
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity parent = ECS::EntityUtils::Unpack(parentId);
    if (!world->IsAlive(parent)) {
        LOG_WARNING("[WorldInterop] GetFirstChild failed: parent is not alive");
        return InvalidPackedEntityId();
    }

    ECS::Entity child = world->FirstChildOf(parent);
    if (child.IsNull() || !world->IsAlive(child)) {
        return InvalidPackedEntityId();
    }

    // Pack the entity handle into a 64-bit ID.
    return ECS::EntityUtils::Pack(child);
}

/**
 * @brief Return the next sibling entity ID.
 */
INTEROP_API uint64_t WorldInterop_GetNextSibling(void* worldPtr, uint64_t childId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return InvalidPackedEntityId();
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity child = ECS::EntityUtils::Unpack(childId);
    if (!world->IsAlive(child)) {
        LOG_WARNING("[WorldInterop] GetNextSibling failed: child is not alive");
        return InvalidPackedEntityId();
    }

    ECS::Entity sibling = world->NextSiblingOf(child);
    if (sibling.IsNull() || !world->IsAlive(sibling)) {
        return InvalidPackedEntityId();
    }

    // Pack the entity handle into a 64-bit ID.
    return ECS::EntityUtils::Pack(sibling);
}

/**
 * @brief Return the number of children for an entity.
 */
INTEROP_API int WorldInterop_GetChildCount(void* worldPtr, uint64_t parentId) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity parent = ECS::EntityUtils::Unpack(parentId);
    if (!world->IsAlive(parent)) {
        LOG_WARNING("[WorldInterop] GetChildCount failed: parent is not alive");
        return 0;
    }

    return world->ChildCountOf(parent);
}

// ============================================================================
// Query Operations
// ============================================================================

/**
 * @brief Create a query iterator from required, optional, and excluded component hashes.
 * @param worldPtr Pointer to the World instance.
 * @param componentHashes Array of required component hashes.
 * @param componentCount Number of required components.
 * @param optionalHashes Array of optional component hashes.
 * @param optionalCount Number of optional components.
 * @param excludeHashes Array of excluded component hashes.
 * @param excludeCount Number of excluded components.
 * @param outIterator Output iterator state.
 * @return true if query setup succeeded, false otherwise.
 */
INTEROP_API bool WorldInterop_CreateQuery(void* worldPtr, uint32_t* componentHashes, int componentCount, uint32_t* optionalHashes, int optionalCount, uint32_t* excludeHashes, int excludeCount, QueryIterator* outIterator) {
    if (!worldPtr || !outIterator || componentCount <= 0 || componentCount > 8) {
        return false;
    }

    if (optionalCount < 0 || optionalCount > 8) return false;
    if (excludeCount < 0 || excludeCount > 8) return false;

    ECS::World* world = GetWorld(worldPtr);
    const uint32_t collisionHash = FNV1aHash("CollisionEventBuffer");
    const uint32_t collisionExitHash = FNV1aHash("CollisionExitEventBuffer");
    bool hasCollision = false;
    bool hasCollisionExit = false;
    
    // Convert hashes to component IDs
    std::vector<ECS::ComponentTypeId> componentIds;
    componentIds.reserve(componentCount);
    
    for (int i = 0; i < componentCount; ++i) {
        if (componentHashes[i] == collisionHash) {
            hasCollision = true;
        }
        if (componentHashes[i] == collisionExitHash) {
            hasCollisionExit = true;
        }
        ECS::ComponentTypeId id = GetComponentIdFromHash(componentHashes[i]);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[WorldInterop] Unknown component type hash in query: " << componentHashes[i]);
            return false;
        }
        componentIds.push_back(id);
        outIterator->componentTypeIds[i] = id;
    }
    // Process optional components
    outIterator->optionalCount = 0;
    for (int i = 0; i < optionalCount; ++i) {
        ECS::ComponentTypeId id = GetComponentIdFromHash(optionalHashes[i]);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[WorldInterop] Unknown optional component type hash in query: " << optionalHashes[i]);
            return false;
        }
        outIterator->optionalTypeIds[i] = id;
        outIterator->optionalCount++;
    }

    // Process exclude components
    outIterator->excludeCount = 0;
    for (int i = 0; i < excludeCount; ++i) {
        ECS::ComponentTypeId id = GetComponentIdFromHash(excludeHashes[i]);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[WorldInterop] Unknown exclude component type hash in query: " << excludeHashes[i]);
            return false;
        }
        outIterator->excludeTypeIds[i] = id;
        outIterator->excludeCount++;
    }
    
    // Create signature for required components and get matching archetypes
    ECS::Signature sig(componentIds);
    // Keep reference to world's archetype list; we'll skip excluded archetypes during iteration
    auto& matched = world->GetMatchingArchetypes(sig);

    // Initialize iterator (point at the world's matched archetypes)
    outIterator->worldPtr = worldPtr;
    outIterator->archetypes = &matched;
    outIterator->archetypeIndex = 0;
    outIterator->chunkIndex = 0;
    outIterator->entityIndex = 0;
    outIterator->componentCount = componentCount;
    
    return !matched.empty();
}

/**
 * @brief Advance the query iterator to the next match.
 */
INTEROP_API bool WorldInterop_QueryNext(QueryIterator* iterator, uint64_t* outEntityId) {
    if (!iterator || !iterator->archetypes) {
        return false;
    }

    ECS::World* world = GetWorld(iterator->worldPtr);
    const auto& archetypes = *static_cast<const std::vector<ECS::Archetype*>*>(iterator->archetypes);
    const uint32_t collisionHash = FNV1aHash("CollisionEventBuffer");
    const uint32_t collisionExitHash = FNV1aHash("CollisionExitEventBuffer");
    const ECS::ComponentTypeId collisionId = ECS::ComponentRegistry::GetComponentIdFromHash(collisionHash);
    const ECS::ComponentTypeId collisionExitId = ECS::ComponentRegistry::GetComponentIdFromHash(collisionExitHash);
    
    // Iterate through archetypes, chunks, and entities
    while (iterator->archetypeIndex < archetypes.size()) {
        ECS::Archetype* archetype = archetypes[iterator->archetypeIndex];
        
        if (!archetype) {
            iterator->archetypeIndex++;
            continue;
        }

        // If this archetype contains any excluded components, skip it
        if (iterator->excludeCount > 0) {
            bool hasExcluded = false;
            for (uint32_t exIdx = 0; exIdx < iterator->excludeCount; ++exIdx) {
                ECS::ComponentTypeId exId = iterator->excludeTypeIds[exIdx];
                if (archetype->Has(exId)) { hasExcluded = true; break; }
            }
            if (hasExcluded) {
                iterator->archetypeIndex++;
                iterator->chunkIndex = 0;
                iterator->entityIndex = 0;
                continue;
            }
        }
        
        while (iterator->chunkIndex < archetype->GetChunkCount()) {
            ECS::Chunk* chunk = archetype->GetChunk(iterator->chunkIndex);
            
            if (iterator->entityIndex < chunk->Count()) {
                // Found an entity
                ECS::Entity entity = chunk->GetEntity(iterator->entityIndex);
                *outEntityId = ECS::EntityUtils::Pack(entity);
                
                // Advance to next entity
                iterator->entityIndex++;

                if (iterator->componentCount == 1) {
                    const ECS::ComponentTypeId queryId = iterator->componentTypeIds[0];
                    if (queryId == collisionId || queryId == collisionExitId) {
                        const bool hasComponent = world->HasById(entity, queryId);
                        if (!hasComponent) {
                            LOG_WARNING("[WorldInterop_QueryNext] Entity " << *outEntityId
                                << " missing expected event component id=" << queryId);
                        } else if (queryId == collisionId) {
                            if (auto* buffer = world->TryGet<ECS::Events::CollisionEventBuffer>(entity)) {
                                LOG_INFO("[WorldInterop_QueryNext] CollisionEventBuffer e=" << *outEntityId
                                    << " count=" << buffer->Count);
                            }
                        } else if (queryId == collisionExitId) {
                            if (auto* buffer = world->TryGet<ECS::Events::CollisionExitEventBuffer>(entity)) {
                                LOG_INFO("[WorldInterop_QueryNext] CollisionExitEventBuffer e=" << *outEntityId
                                    << " count=" << buffer->Count);
                            }
                        }
                    }
                }
                
                return true;
            }
            
            // Move to next chunk
            iterator->chunkIndex++;
            iterator->entityIndex = 0;
        }
        
        // Move to next archetype
        iterator->archetypeIndex++;
        iterator->chunkIndex = 0;
        iterator->entityIndex = 0;
    }
    
    return false; // No more entities
}

/**
 * @brief Get a required component for the current query item.
 */
INTEROP_API void* WorldInterop_QueryGetComponent(QueryIterator* iterator, int componentIndex) {
    if (!iterator || !iterator->archetypes || componentIndex < 0 || componentIndex >= static_cast<int>(iterator->componentCount)) {
        return nullptr;
    }

    if (iterator->archetypeIndex == 0 && iterator->chunkIndex == 0 && iterator->entityIndex == 0) {
        return nullptr; // Query hasn't started yet
    }

    const auto& archetypes = *static_cast<const std::vector<ECS::Archetype*>*>(iterator->archetypes);
    
    // Get current archetype and chunk (iterator was already advanced, so we need previous positions)
    uint32_t archIdx = iterator->archetypeIndex;
    uint32_t chunkIdx = iterator->chunkIndex;
    uint32_t entityIdx = iterator->entityIndex - 1; // Was incremented after finding entity
    
    // Adjust indices if entity index wrapped
    if (entityIdx == UINT32_MAX) {
        if (chunkIdx > 0) {
            chunkIdx--;
            ECS::Archetype* arch = archetypes[archIdx];
            if (arch) {
                ECS::Chunk* chunk = arch->GetChunk(chunkIdx);
                entityIdx = chunk->Count() - 1;
            }
        } else if (archIdx > 0) {
            archIdx--;
            ECS::Archetype* arch = archetypes[archIdx];
            if (arch && arch->GetChunkCount() > 0) {
                chunkIdx = arch->GetChunkCount() - 1;
                ECS::Chunk* chunk = arch->GetChunk(chunkIdx);
                entityIdx = chunk->Count() - 1;
            }
        }
    }
    
    if (archIdx >= archetypes.size()) {
        return nullptr;
    }
    
    ECS::Archetype* archetype = archetypes[archIdx];
    if (!archetype || chunkIdx >= archetype->GetChunkCount()) {
        return nullptr;
    }
    
    ECS::ComponentTypeId componentId = iterator->componentTypeIds[componentIndex];
    return archetype->GetRaw(componentId, chunkIdx, entityIdx);
}

/**
 * @brief Get an optional component for the current query item.
 */
INTEROP_API void* WorldInterop_QueryGetOptionalComponent(QueryIterator* iterator, uint32_t componentTypeHash) {
    if (!iterator || !iterator->archetypes) {
        return nullptr;
    }

    // ECS::World* world = GetWorld(iterator->worldPtr);
    const auto& archetypes = *static_cast<const std::vector<ECS::Archetype*>*>(iterator->archetypes);

    // Get current archetype and chunk (iterator was already advanced, so we need previous positions)
    uint32_t archIdx = iterator->archetypeIndex;
    uint32_t chunkIdx = iterator->chunkIndex;
    uint32_t entityIdx = iterator->entityIndex - 1; // Was incremented after finding entity

    // Adjust indices if entity index wrapped
    if (entityIdx == UINT32_MAX) {
        if (chunkIdx > 0) {
            chunkIdx--;
            ECS::Archetype* arch = archetypes[archIdx];
            if (arch) {
                ECS::Chunk* chunk = arch->GetChunk(chunkIdx);
                entityIdx = chunk->Count() - 1;
            }
        } else if (archIdx > 0) {
            archIdx--;
            ECS::Archetype* arch = archetypes[archIdx];
            if (arch && arch->GetChunkCount() > 0) {
                chunkIdx = arch->GetChunkCount() - 1;
                ECS::Chunk* chunk = arch->GetChunk(chunkIdx);
                entityIdx = chunk->Count() - 1;
            }
        }
    }

    if (archIdx >= archetypes.size()) {
        return nullptr;
    }

    ECS::Archetype* archetype = archetypes[archIdx];
    if (!archetype || chunkIdx >= archetype->GetChunkCount()) {
        return nullptr;
    }

    // Convert hash to component id
    ECS::ComponentTypeId componentId = GetComponentIdFromHash(componentTypeHash);
    if (componentId == ECS::NULL_COMPONENT_ID) {
        LOG_WARNING("[WorldInterop] Unknown optional component type hash in query: " << componentTypeHash);
        return nullptr;
    }

    // If archetype doesn't have the component, return null
    if (!archetype->Has(componentId)) {
        return nullptr;
    }

    return archetype->GetRaw(componentId, chunkIdx, entityIdx);
}

// ============================================================================
// Component Registration Operations
// ============================================================================

namespace {
    // Generic component wrapper for C# components that don't have C++ equivalents
    // This stores raw byte data and handles construction/destruction
    template<uint32_t TypeHash, size_t Size, size_t Alignment>
    struct GenericComponent {
        alignas(Alignment) uint8_t data[Size];
        
        // Register generic component accessors.
        GenericComponent() {
            std::memset(data, 0, Size);
        }
        
        ~GenericComponent() = default;
        
        // Register generic component accessors.
        GenericComponent(const GenericComponent& other) {
            std::memcpy(data, other.data, Size);
        }
        
        GenericComponent& operator=(const GenericComponent& other) {
            if (this != &other) {
                // Copy raw component data.
                std::memcpy(data, other.data, Size);
            }
            return *this;
        }
    };
    
    // Metadata for C# components
    struct CSharpComponentMeta {
        int Size;
        int Alignment;
    };
    
    // Registry for tracking which component types have been registered from C#
    struct ComponentRegistrationTracker {
        std::unordered_map<uint32_t, CSharpComponentMeta> registered;
        std::mutex mutex;
    };
    
    // Return registration tracker.
    ComponentRegistrationTracker& GetRegistrationTracker() {
        static ComponentRegistrationTracker tracker;
        return tracker;
    }

    // --- Managed serialization and deserialization callback support ---
    namespace {
        using SerializeCallback = const char*(__cdecl*)(uint32_t typeHash, const void* data, int size);
        using DeserializeCallback = void(__cdecl*)(uint32_t typeHash, void* data, int size, const char* jsonStr);
        SerializeCallback g_serializeCallback = nullptr;
        DeserializeCallback g_deserializeCallback = nullptr;
        std::mutex g_serializerMutex;
    }

    /**
     * @brief Register a serialization callback for a component.
     */
    INTEROP_API void WorldInterop_RegisterSerializeCallback(void* fnPtr) {
        std::lock_guard<std::mutex> lock(g_serializerMutex);
        g_serializeCallback = reinterpret_cast<SerializeCallback>(fnPtr);
        LOG_INFO("[WorldInterop] Registered managed serialize callback: " << fnPtr);
    }

    /**
     * @brief Register a deserialization callback for a component.
     */
    INTEROP_API void WorldInterop_RegisterDeserializeCallback(void* fnPtr) {
        std::lock_guard<std::mutex> lock(g_serializerMutex);
        g_deserializeCallback = reinterpret_cast<DeserializeCallback>(fnPtr);
        LOG_INFO("[WorldInterop] Registered managed deserialize callback: " << fnPtr);
    }

    /**
     * @brief Serialize a component instance to JSON.
     */
    INTEROP_API const char* WorldInterop_SerializeComponentToJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash) {
        if (!worldPtr) return nullptr;
        ECS::World* world = GetWorld(worldPtr);
        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) return nullptr;

        ECS::ComponentTypeId id = GetComponentIdFromHash(componentTypeHash);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[WorldInterop] Serialize: Unknown component type hash: " << componentTypeHash);
            return nullptr;
        }

        const auto& meta = ECS::ComponentRegistry::Meta(id);
        void* ptr = world->GetRawComponentPtr(entity, id);
        if (!ptr) return nullptr;

        std::lock_guard<std::mutex> lock(g_serializerMutex);
        if (!g_serializeCallback) {
            LOG_WARNING("[WorldInterop] No managed serializer callback registered");
            return nullptr;
        }

        // Call managed callback which must return a CoTaskMem-allocated UTF8 C string
        const char* result = g_serializeCallback(meta.TypeHash, ptr, static_cast<int>(meta.Size));
        return result;
    }

    /**
     * @brief Free heap memory for serialized JSON strings.
     */
    INTEROP_API void WorldInterop_FreeSerializedString(const char* s) {
        if (!s) return;
        CoTaskMemFree(const_cast<char*>(s));
    }

    /**
     * @brief Deserialize a component instance from JSON.
     */
    INTEROP_API void WorldInterop_DeserializeComponentFromJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, const char* jsonStr) {
        if (!worldPtr || !jsonStr) return;

        ECS::World* world = GetWorld(worldPtr);
        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) {
            LOG_WARNING("[WorldInterop] DeserializeComponentFromJson: Entity is not alive");
            return;
        }

        ECS::ComponentTypeId id = GetComponentIdFromHash(componentTypeHash);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[WorldInterop] DeserializeComponentFromJson: Unknown component type hash: " << componentTypeHash);
            return;
        }

        const auto& meta = ECS::ComponentRegistry::Meta(id);
        void* ptr = world->GetRawComponentPtr(entity, id);
        if (!ptr) {
            LOG_WARNING("[WorldInterop] DeserializeComponentFromJson: Component not found on entity");
            return;
        }

        std::lock_guard<std::mutex> lock(g_serializerMutex);
        if (!g_deserializeCallback) {
            LOG_WARNING("[WorldInterop] DeserializeComponentFromJson: No managed deserializer callback registered");
            return;
        }

        // Call managed callback to deserialize JSON into component memory
        g_deserializeCallback(meta.TypeHash, ptr, static_cast<int>(meta.Size), jsonStr);
    }
}

/**
 * @brief Register a component type for scripting.
 */
INTEROP_API bool WorldInterop_RegisterComponent(uint32_t typeNameHash, int size, int alignment, const char* typeName) {
    LOG_INFO("[WorldInterop_RegisterComponent] CALLED with hash=0x" << std::hex << typeNameHash << std::dec << ", size=" << size << ", alignment=" << alignment << ", name=" << (typeName ? typeName : "null"));
    
    // Check if already registered in the C++ ComponentRegistry
    ECS::ComponentTypeId id = ECS::ComponentRegistry::GetComponentIdFromHash(typeNameHash);
    if (id != ECS::NULL_COMPONENT_ID) {
        const auto& existingMeta = ECS::ComponentRegistry::Meta(id);

        if (existingMeta.IsManaged) {
            // Hot reload path: refresh size/alignment for an existing managed hash.
            if (typeName && typeName[0] != '\0') {
                ECS::ComponentRegistry::RegisterCSharpComponentWithName(typeNameHash, size, alignment, typeName);
            } else {
                ECS::ComponentRegistry::RegisterCSharpComponent(typeNameHash, size, alignment);
            }

            const auto& updatedMeta = ECS::ComponentRegistry::Meta(id);
            LOG_INFO("[WorldInterop_RegisterComponent] Updated existing managed component ID " << id
                << " (size " << existingMeta.Size << " -> " << updatedMeta.Size
                << ", align " << existingMeta.Align << " -> " << updatedMeta.Align << ")");
            return true;
        }

        LOG_INFO("[WorldInterop_RegisterComponent] Already registered with native component ID " << id << ", keeping existing metadata");
        return true;
    }
    
    // Register as a C# component in the native ComponentRegistry
    // This makes it available to the editor and all engine systems
    if (typeName && typeName[0] != '\0') {
        // Register with name
        std::string name = typeName;
        ECS::ComponentTypeId newId = ECS::ComponentRegistry::RegisterCSharpComponentWithName(typeNameHash, size, alignment, name);
        LOG_INFO("[WorldInterop_RegisterComponent] Registered C# component '" << name << "' with ID " << newId);
    } else {
        // Register without name (fallback for legacy calls)
        ECS::ComponentTypeId newId = ECS::ComponentRegistry::RegisterCSharpComponent(typeNameHash, size, alignment);
        LOG_INFO("[WorldInterop_RegisterComponent] Registered new C# component (no name) with ID " << newId);
    }
    
    return true;
}

/**
 * @brief Check whether a component type is registered.
 */
INTEROP_API bool WorldInterop_IsComponentRegistered(uint32_t typeNameHash) {
    // Check if component is registered in ComponentRegistry
    ECS::ComponentTypeId id = ECS::ComponentRegistry::GetComponentIdFromHash(typeNameHash);
    if (id != ECS::NULL_COMPONENT_ID) {
        return true;
    }
    
    // Check if registered via WorldInterop_RegisterComponent
    auto& tracker = GetRegistrationTracker();
    std::lock_guard<std::mutex> lock(tracker.mutex);
    return tracker.registered.find(typeNameHash) != tracker.registered.end();
}

/**
 * @brief Return all registered component hashes.
 */
INTEROP_API int WorldInterop_GetAllRegisteredComponentHashes(uint32_t* outHashes, int maxCount) {
    auto& tracker = GetRegistrationTracker();
    std::lock_guard<std::mutex> lock(tracker.mutex);
    
    int count = 0;
    for (const auto& [hash, meta] : tracker.registered) {
        if (count >= maxCount) break;
        if (outHashes) {
            outHashes[count] = hash;
        }
        count++;
    }
    return count;
}

/**
 * @brief Return component metadata from a type hash.
 */
INTEROP_API bool WorldInterop_GetComponentMetaFromHash(uint32_t typeNameHash, int* outSize, int* outAlignment) {
    auto& tracker = GetRegistrationTracker();
    std::lock_guard<std::mutex> lock(tracker.mutex);
    
    auto it = tracker.registered.find(typeNameHash);
    if (it == tracker.registered.end()) {
        return false;
    }
    
    if (outSize) *outSize = it->second.Size;
    if (outAlignment) *outAlignment = it->second.Alignment;
    return true;
}
