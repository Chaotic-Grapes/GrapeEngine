/* Start Header *****************************************************************/
/*!
\file    WorldInterop.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of World/Entity interop functions for C# scripting systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#define BUILDING_WORLD_INTEROP
#include "scripting/WorldInterop.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include "core/Logger.h"
#include <cstring>

namespace {
    // Convert World pointer
    ECS::World* GetWorld(void* worldPtr) {
        return static_cast<ECS::World*>(worldPtr);
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

WORLD_INTEROP_API uint64_t WorldInterop_CreateEntity(void* worldPtr) {
    if (!worldPtr) {
        LOG_ERROR("[WorldInterop] World pointer is null");
        return 0;
    }

    ECS::World* world = GetWorld(worldPtr);
    ECS::Entity entity = world->Create();
    
    // Add default LocalTransform component
    world->Add<ECS::Components::LocalTransform>(entity);
    
    return ECS::EntityUtils::Pack(entity);
}

WORLD_INTEROP_API void WorldInterop_DestroyEntity(void* worldPtr, uint64_t entityId) {
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

WORLD_INTEROP_API bool WorldInterop_IsEntityAlive(void* worldPtr, uint64_t entityId) {
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

WORLD_INTEROP_API bool WorldInterop_HasComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash) {
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
        LOG_WARNING("[WorldInterop] Unknown component type hash: {}", componentTypeHash);
        return false;
    }

    return world->HasById(entity, componentId);
}

WORLD_INTEROP_API void* WorldInterop_GetComponentPtr(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash) {
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
        LOG_WARNING("[WorldInterop] Unknown component type hash: {}", componentTypeHash);
        return nullptr;
    }

    // Get raw component pointer
    return world->GetRawComponentPtr(entity, componentId);
}

WORLD_INTEROP_API void* WorldInterop_AddComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, void* componentData, int componentSize) {
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
        LOG_WARNING("[WorldInterop] Unknown component type hash: {}", componentTypeHash);
        return nullptr;
    }

    // Add component and copy data
    void* addedComponentPtr = world->AddComponentById(entity, componentId, componentData, componentSize);
    
    return addedComponentPtr;
}

WORLD_INTEROP_API void WorldInterop_RemoveComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash) {
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
        LOG_WARNING("[WorldInterop] Unknown component type hash: {}", componentTypeHash);
        return;
    }

    world->RemoveById(entity, componentId);
}

// ============================================================================
// Query Operations
// ============================================================================

WORLD_INTEROP_API bool WorldInterop_CreateQuery(void* worldPtr, uint32_t* componentHashes, int componentCount, QueryIterator* outIterator) {
    if (!worldPtr || !outIterator || componentCount <= 0 || componentCount > 8) {
        return false;
    }

    ECS::World* world = GetWorld(worldPtr);
    
    // Convert hashes to component IDs
    std::vector<ECS::ComponentTypeId> componentIds;
    componentIds.reserve(componentCount);
    
    for (int i = 0; i < componentCount; ++i) {
        ECS::ComponentTypeId id = GetComponentIdFromHash(componentHashes[i]);
        if (id == ECS::NULL_COMPONENT_ID) {
            LOG_WARNING("[WorldInterop] Unknown component type hash in query: {}", componentHashes[i]);
            return false;
        }
        componentIds.push_back(id);
        outIterator->componentTypeIds[i] = id;
    }
    
    // Create signature and get matching archetypes
    ECS::Signature sig(componentIds);
    const auto& archetypes = world->GetMatchingArchetypes(sig);
    
    // Initialize iterator
    outIterator->worldPtr = worldPtr;
    outIterator->archetypes = &archetypes;
    outIterator->archetypeIndex = 0;
    outIterator->chunkIndex = 0;
    outIterator->entityIndex = 0;
    outIterator->componentCount = componentCount;
    
    return !archetypes.empty();
}

WORLD_INTEROP_API bool WorldInterop_QueryNext(QueryIterator* iterator, uint64_t* outEntityId) {
    if (!iterator || !iterator->archetypes) {
        return false;
    }

    ECS::World* world = GetWorld(iterator->worldPtr);
    const auto& archetypes = *static_cast<const std::vector<ECS::Archetype*>*>(iterator->archetypes);
    
    // Iterate through archetypes, chunks, and entities
    while (iterator->archetypeIndex < archetypes.size()) {
        ECS::Archetype* archetype = archetypes[iterator->archetypeIndex];
        
        if (!archetype) {
            iterator->archetypeIndex++;
            continue;
        }
        
        while (iterator->chunkIndex < archetype->GetChunkCount()) {
            ECS::Chunk* chunk = archetype->GetChunk(iterator->chunkIndex);
            
            if (iterator->entityIndex < chunk->Count()) {
                // Found an entity
                ECS::Entity entity = chunk->GetEntity(iterator->entityIndex);
                *outEntityId = ECS::EntityUtils::Pack(entity);
                
                // Advance to next entity
                iterator->entityIndex++;
                
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

WORLD_INTEROP_API void* WorldInterop_QueryGetComponent(QueryIterator* iterator, int componentIndex) {
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

// ============================================================================
// Component Registration Operations
// ============================================================================

namespace {
    // Generic component wrapper for C# components that don't have C++ equivalents
    // This stores raw byte data and handles construction/destruction
    template<uint32_t TypeHash, size_t Size, size_t Alignment>
    struct GenericComponent {
        alignas(Alignment) uint8_t data[Size];
        
        GenericComponent() {
            std::memset(data, 0, Size);
        }
        
        ~GenericComponent() = default;
        
        GenericComponent(const GenericComponent& other) {
            std::memcpy(data, other.data, Size);
        }
        
        GenericComponent& operator=(const GenericComponent& other) {
            if (this != &other) {
                std::memcpy(data, other.data, Size);
            }
            return *this;
        }
    };
    
    // Registry for tracking which component types have been registered
    struct ComponentRegistrationTracker {
        std::unordered_map<uint32_t, bool> registered;
        std::mutex mutex;
    };
    
    ComponentRegistrationTracker& GetRegistrationTracker() {
        static ComponentRegistrationTracker tracker;
        return tracker;
    }
}

WORLD_INTEROP_API bool WorldInterop_RegisterComponent(uint32_t typeNameHash, int size, int alignment) {
    auto& tracker = GetRegistrationTracker();
    std::lock_guard<std::mutex> lock(tracker.mutex);
    
    // Check if already registered
    if (tracker.registered.find(typeNameHash) != tracker.registered.end()) {
        return false; // Already registered
    }
    
    // Note: We can't dynamically create template instantiations at runtime
    // This function primarily serves to mark components as "known" for validation
    // The actual C++ component types should be registered separately using
    // ComponentRegistry::RegisterWithHash<T>() in engine initialization code
    
    // For now, we just track that C# requested registration
    tracker.registered[typeNameHash] = true;
    
    LOG_INFO("[WorldInterop] Component registered from C#: hash={}, size={}, alignment={}", 
             typeNameHash, size, alignment);
    
    return true;
}

WORLD_INTEROP_API bool WorldInterop_IsComponentRegistered(uint32_t typeNameHash) {
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