/* Start Header *****************************************************************/
/*!
\file    WorldInterop.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
C API exports for World/Entity operations used by C# scripting systems.
This provides the bridge between C++ ECS and managed C# systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef WORLDINTEROP_H
#define WORLDINTEROP_H

#include <cstdint>

// Export macro for C API
#ifdef _WIN32
    #ifdef BUILDING_WORLD_INTEROP
        #define WORLD_INTEROP_API extern "C" __declspec(dllexport)
    #else
        #define WORLD_INTEROP_API extern "C" __declspec(dllimport)
    #endif
#else
    #define WORLD_INTEROP_API extern "C"
#endif

namespace ECS {
    class World;
}

// ============================================================================
// Entity Lifecycle Operations
// ============================================================================

/// <summary>
/// Create a new entity in the specified world
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <returns>Packed entity ID (64-bit)</returns>
WORLD_INTEROP_API uint64_t WorldInterop_CreateEntity(void* worldPtr);

/// <summary>
/// Destroy an entity in the world
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <param name="entityId">Packed entity ID to destroy</param>
WORLD_INTEROP_API void WorldInterop_DestroyEntity(void* worldPtr, uint64_t entityId);

/// <summary>
/// Check if an entity is alive
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <param name="entityId">Packed entity ID to check</param>
/// <returns>True if entity is alive, false otherwise</returns>
WORLD_INTEROP_API bool WorldInterop_IsEntityAlive(void* worldPtr, uint64_t entityId);

// ============================================================================
// Component Operations
// ============================================================================

/// <summary>
/// Check if entity has a component of specified type
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <param name="entityId">Packed entity ID</param>
/// <param name="componentTypeHash">FNV-1a hash of component type name</param>
/// <returns>True if entity has component, false otherwise</returns>
WORLD_INTEROP_API bool WorldInterop_HasComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash);

/// <summary>
/// Get pointer to component data for reading/writing
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <param name="entityId">Packed entity ID</param>
/// <param name="componentTypeHash">FNV-1a hash of component type name</param>
/// <returns>Pointer to component data, or nullptr if not found</returns>
WORLD_INTEROP_API void* WorldInterop_GetComponentPtr(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash);

/// <summary>
/// Add a component to an entity
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <param name="entityId">Packed entity ID</param>
/// <param name="componentTypeHash">FNV-1a hash of component type name</param>
/// <param name="componentData">Pointer to component data to copy</param>
/// <param name="componentSize">Size of component data in bytes</param>
/// <returns>Pointer to the added component data</returns>
WORLD_INTEROP_API void* WorldInterop_AddComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, void* componentData, int componentSize);

/// <summary>
/// Remove a component from an entity
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <param name="entityId">Packed entity ID</param>
/// <param name="componentTypeHash">FNV-1a hash of component type name</param>
WORLD_INTEROP_API void WorldInterop_RemoveComponent(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash);

// ============================================================================
// Query Operations
// ============================================================================

/// <summary>
/// Structure representing query iteration state
/// </summary>
struct QueryIterator {
    void* worldPtr;
    const void* archetypes;      // std::vector<Archetype*>*
    uint32_t archetypeIndex;
    uint32_t chunkIndex;
    uint32_t entityIndex;
    uint32_t componentCount;
    uint32_t componentTypeIds[8]; // Max 8 components in a query
};

/// <summary>
/// Create a query iterator for entities with specified components
/// </summary>
/// <param name="worldPtr">Pointer to the World instance</param>
/// <param name="componentHashes">Array of component type hashes</param>
/// <param name="componentCount">Number of components to query</param>
/// <param name="outIterator">Output iterator structure</param>
/// <returns>True if query has any matches, false otherwise</returns>
WORLD_INTEROP_API bool WorldInterop_CreateQuery(void* worldPtr, uint32_t* componentHashes, int componentCount, QueryIterator* outIterator);

/// <summary>
/// Advance query iterator to next entity
/// </summary>
/// <param name="iterator">Query iterator</param>
/// <param name="outEntityId">Output entity ID</param>
/// <returns>True if entity found, false if iteration complete</returns>
WORLD_INTEROP_API bool WorldInterop_QueryNext(QueryIterator* iterator, uint64_t* outEntityId);

/// <summary>
/// Get component pointer for current query result
/// </summary>
/// <param name="iterator">Query iterator</param>
/// <param name="componentIndex">Index of component in query (0-based)</param>
/// <returns>Pointer to component data</returns>
WORLD_INTEROP_API void* WorldInterop_QueryGetComponent(QueryIterator* iterator, int componentIndex);

// ============================================================================
// Component Registration Operations
// ============================================================================

/// <summary>
/// Register a component type by name and metadata
/// </summary>
/// <param name="typeNameHash">FNV-1a hash of component type name</param>
/// <param name="size">Size of component in bytes</param>
/// <param name="alignment">Alignment requirement in bytes</param>
/// <returns>True if registration succeeded, false if already registered</returns>
WORLD_INTEROP_API bool WorldInterop_RegisterComponent(uint32_t typeNameHash, int size, int alignment);

/// <summary>
/// Check if a component type is registered
/// </summary>
/// <param name="typeNameHash">FNV-1a hash of component type name</param>
/// <returns>True if component is registered, false otherwise</returns>
WORLD_INTEROP_API bool WorldInterop_IsComponentRegistered(uint32_t typeNameHash);

#endif
