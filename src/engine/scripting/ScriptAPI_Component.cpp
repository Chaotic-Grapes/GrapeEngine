/* Start Header *****************************************************************/
/*!
\file   ScriptAPI_Component.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   20th November 2025
\brief
C# scripting API exports for generic component operations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/World.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"
#include <cstring>
#include "core/Logger.h"
#include "ecs/systems/BoundaryCheckSystem.h"

// Export macro
#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

// Global world pointer for script API access
// TODO: Replace with proper singleton or dependency injection
ECS::World* g_scriptWorld = nullptr;

namespace {
    // FNV-1a hash algorithm - must match C# implementation exactly
    uint32_t FNV1aHash(const char* str) {
        if (!str) return 0;
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str++);
            hash *= 16777619u;
        }
        return hash;
    }

    ECS::World* GetScriptWorld() {
        return g_scriptWorld;
    }

    // Generic component access using raw memory operations
    bool GetComponentGeneric(uint64_t entityId, uint32_t typeHash, void* outBuffer, int bufferSize) {
        ECS::World* world = GetScriptWorld();
        if (!world) {
            LOG_ERROR("[ScriptAPI] World not set");
            return false;
        }

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) {
            std::cerr << "[ScriptAPI] Entity not alive" << '\n';
            LOG_ERROR("[ScriptAPI] Entity not alive");
            return false;
        }

        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                auto* comp = world->TryGet<ECS::Components::ComponentType>(entity); \
                if (!comp) return false; \
                if (bufferSize < static_cast<int>(sizeof(ECS::Components::ComponentType))) { \
                    LOG_ERROR("[ScriptAPI] Buffer too small"); \
                    return false; \
                } \
                std::memcpy(outBuffer, comp, sizeof(ECS::Components::ComponentType)); \
                return true; \
            }

        HANDLE_COMPONENT_TYPE(LocalTransform, "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform, "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D, "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D, "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D, "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D, "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D, "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D, "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D, "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity, "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration, "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity, "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody, "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider, "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider, "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D, "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D, "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D, "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D, "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D, "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Layer, "Layer")
        HANDLE_COMPONENT_TYPE(Camera3D, "Camera3D")
        HANDLE_COMPONENT_TYPE(CameraMatrices, "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active, "Active")
        HANDLE_COMPONENT_TYPE(Name, "Name")
        HANDLE_COMPONENT_TYPE(TagMask, "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime, "Lifetime")
        HANDLE_COMPONENT_TYPE(UIClickable, "UIClickable")

        #undef HANDLE_COMPONENT_TYPE

        LOG_ERROR("[ScriptAPI] Unknown component type hash: " << typeHash);
        return false;
    }

    void* GetComponentPtr(uint64_t entityId, uint32_t typeHash) {
        ECS::World* world = GetScriptWorld();
        if (!world) return nullptr;

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) return nullptr;

        #define HANDLE_COMPONENT_TYPE_PTR(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                auto* comp = world->TryGet<ECS::Components::ComponentType>(entity); \
                return comp ? static_cast<void*>(comp) : nullptr; \
            }

        HANDLE_COMPONENT_TYPE_PTR(LocalTransform, "LocalTransform")
        HANDLE_COMPONENT_TYPE_PTR(WorldTransform, "WorldTransform")
        HANDLE_COMPONENT_TYPE_PTR(LinearVelocity2D, "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE_PTR(Acceleration2D, "Acceleration2D")
        HANDLE_COMPONENT_TYPE_PTR(AngularVelocity2D, "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE_PTR(Rigidbody2D, "Rigidbody2D")
        HANDLE_COMPONENT_TYPE_PTR(PhysicsMaterial2D, "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE_PTR(BoxCollider2D, "BoxCollider2D")
        HANDLE_COMPONENT_TYPE_PTR(CircleCollider2D, "CircleCollider2D")
        HANDLE_COMPONENT_TYPE_PTR(Velocity, "Velocity")
        HANDLE_COMPONENT_TYPE_PTR(Acceleration, "Acceleration")
        HANDLE_COMPONENT_TYPE_PTR(AngularVelocity, "AngularVelocity")
        HANDLE_COMPONENT_TYPE_PTR(Rigidbody, "Rigidbody")
        HANDLE_COMPONENT_TYPE_PTR(BoxCollider, "BoxCollider")
        HANDLE_COMPONENT_TYPE_PTR(SphereCollider, "SphereCollider")
        HANDLE_COMPONENT_TYPE_PTR(SpriteRenderer2D, "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE_PTR(ShapeCircle2D, "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE_PTR(ShapeBox2D, "ShapeBox2D")
        HANDLE_COMPONENT_TYPE_PTR(ShapeLine2D, "ShapeLine2D")
        HANDLE_COMPONENT_TYPE_PTR(ZIndex2D, "ZIndex2D")
        HANDLE_COMPONENT_TYPE_PTR(Layer, "Layer")
        HANDLE_COMPONENT_TYPE_PTR(Camera3D, "Camera3D")
        HANDLE_COMPONENT_TYPE_PTR(CameraMatrices, "CameraMatrices")
        HANDLE_COMPONENT_TYPE_PTR(Active, "Active")
        HANDLE_COMPONENT_TYPE_PTR(Name, "Name")
        HANDLE_COMPONENT_TYPE_PTR(TagMask, "TagMask")
        HANDLE_COMPONENT_TYPE_PTR(Lifetime, "Lifetime")
        HANDLE_COMPONENT_TYPE_PTR(UIClickable, "UIClickable")

        #undef HANDLE_COMPONENT_TYPE_PTR

        return nullptr;
    }

    bool AddComponentGeneric(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize, void* outBuffer) {
        ECS::World* world = GetScriptWorld();
        if (!world) {
            LOG_ERROR("[ScriptAPI] World not set");
            return false;
        }

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) {
            LOG_ERROR("[ScriptAPI] Entity not alive");
            return false;
        }

        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                if (dataSize != sizeof(ECS::Components::ComponentType)) { \
                    LOG_ERROR("[ScriptAPI] Component size mismatch"); \
                    return false; \
                } \
                ECS::Components::ComponentType comp; \
                std::memcpy(&comp, componentData, sizeof(ECS::Components::ComponentType)); \
                world->Add<ECS::Components::ComponentType>(entity, comp); \
                if (outBuffer) { \
                    std::memcpy(outBuffer, &comp, sizeof(ECS::Components::ComponentType)); \
                } \
                return true; \
            }

        HANDLE_COMPONENT_TYPE(LocalTransform, "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform, "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D, "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D, "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D, "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D, "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D, "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D, "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D, "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity, "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration, "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity, "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody, "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider, "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider, "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D, "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D, "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D, "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D, "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D, "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Layer, "Layer")
        HANDLE_COMPONENT_TYPE(Camera3D, "Camera3D")
        HANDLE_COMPONENT_TYPE(CameraMatrices, "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active, "Active")
        HANDLE_COMPONENT_TYPE(Name, "Name")
        HANDLE_COMPONENT_TYPE(TagMask, "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime, "Lifetime")
        HANDLE_COMPONENT_TYPE(UIClickable, "UIClickable")

        #undef HANDLE_COMPONENT_TYPE

        LOG_ERROR("[ScriptAPI] Unknown component type hash: " << typeHash);
        return false;
    }

    void SetComponentGeneric(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize) {
        ECS::World* world = GetScriptWorld();
        if (!world) {
            LOG_ERROR("[ScriptAPI] World not set");
            return;
        }

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) {
            LOG_ERROR("[ScriptAPI] Entity not alive");
            return;
        }

        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                if (dataSize != sizeof(ECS::Components::ComponentType)) { \
                    LOG_ERROR("[ScriptAPI] Component size mismatch"); \
                    return; \
                } \
                auto* existing = world->TryGet<ECS::Components::ComponentType>(entity); \
                if (existing) { \
                    std::memcpy(existing, componentData, sizeof(ECS::Components::ComponentType)); \
                } else { \
                    ECS::Components::ComponentType comp; \
                    std::memcpy(&comp, componentData, sizeof(ECS::Components::ComponentType)); \
                    world->Add<ECS::Components::ComponentType>(entity, comp); \
                } \
                return; \
            }

        HANDLE_COMPONENT_TYPE(LocalTransform, "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform, "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D, "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D, "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D, "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D, "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D, "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D, "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D, "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity, "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration, "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity, "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody, "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider, "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider, "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D, "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D, "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D, "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D, "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D, "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Layer, "Layer")
        HANDLE_COMPONENT_TYPE(Camera3D, "Camera3D")
        HANDLE_COMPONENT_TYPE(CameraMatrices, "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active, "Active")
        HANDLE_COMPONENT_TYPE(Name, "Name")
        HANDLE_COMPONENT_TYPE(TagMask, "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime, "Lifetime")
        HANDLE_COMPONENT_TYPE(UIClickable, "UIClickable")

        #undef HANDLE_COMPONENT_TYPE

        LOG_ERROR("[ScriptAPI] Unknown component type hash: " << typeHash);
    }

    bool HasComponentGeneric(uint64_t entityId, uint32_t typeHash) {
        ECS::World* world = GetScriptWorld();
        if (!world) {
            LOG_ERROR("[ScriptAPI] World not set");
            return false;
        }

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) return false;

        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                return world->Has<ECS::Components::ComponentType>(entity); \
            }

        HANDLE_COMPONENT_TYPE(LocalTransform, "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform, "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D, "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D, "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D, "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D, "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D, "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D, "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D, "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity, "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration, "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity, "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody, "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider, "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider, "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D, "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D, "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D, "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D, "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D, "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Layer, "Layer")
        HANDLE_COMPONENT_TYPE(Camera3D, "Camera3D")
        HANDLE_COMPONENT_TYPE(CameraMatrices, "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active, "Active")
        HANDLE_COMPONENT_TYPE(Name, "Name")
        HANDLE_COMPONENT_TYPE(TagMask, "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime, "Lifetime")
        HANDLE_COMPONENT_TYPE(UIClickable, "UIClickable")

        #undef HANDLE_COMPONENT_TYPE

        return false;
    }

    void RemoveComponentGeneric(uint64_t entityId, uint32_t typeHash) {
        ECS::World* world = GetScriptWorld();
        if (!world) {
            LOG_ERROR("[ScriptAPI] World not set");
            return;
        }

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity)) return;

        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                world->Remove<ECS::Components::ComponentType>(entity); \
                return; \
            }

        HANDLE_COMPONENT_TYPE(LocalTransform, "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform, "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D, "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D, "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D, "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D, "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D, "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D, "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D, "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity, "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration, "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity, "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody, "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider, "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider, "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D, "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D, "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D, "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D, "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D, "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Layer, "Layer")
        HANDLE_COMPONENT_TYPE(Camera3D, "Camera3D")
        HANDLE_COMPONENT_TYPE(CameraMatrices, "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active, "Active")
        HANDLE_COMPONENT_TYPE(Name, "Name")
        HANDLE_COMPONENT_TYPE(TagMask, "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime, "Lifetime")
        HANDLE_COMPONENT_TYPE(UIClickable, "UIClickable")
                
        #undef HANDLE_COMPONENT_TYPE
    }
}

// ============================================================================
// Component API - Exported Functions
// ============================================================================

/**
 * @brief Get a component from an entity by type hash
 * @param entityId The packed entity ID
 * @param typeHash The FNV-1a hash of the component type name
 * @param outBuffer Pointer to the output buffer to receive the component data
 * @param bufferSize Size of the output buffer in bytes
 * @return True if the component was retrieved successfully; false otherwise
 */
SCRIPT_API bool ScriptAPI_GetComponent(uint64_t entityId, uint32_t typeHash, void* outBuffer, int bufferSize) {
    return GetComponentGeneric(entityId, typeHash, outBuffer, bufferSize);
}

/**
 * @brief Get a pointer to a component on an entity by type hash
 * @param entityId The packed entity ID
 * @param typeHash The FNV-1a hash of the component type name
 * @return Pointer to the component data, or nullptr if not found
 */
SCRIPT_API void* ScriptAPI_GetComponentPtr(uint64_t entityId, uint32_t typeHash) {
    return GetComponentPtr(entityId, typeHash);
}

/**
 * @brief Add a component to an entity by type hash
 * @param entityId The packed entity ID
 * @param typeHash The FNV-1a hash of the component type name
 * @param componentData Pointer to the component data to add
 * @param dataSize Size of the component data in bytes
 * @param outBuffer Pointer to the output buffer to receive the added component data
 * @return True if the component was added successfully; false otherwise
 */
SCRIPT_API bool ScriptAPI_AddComponent(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize, void* outBuffer) {
    return AddComponentGeneric(entityId, typeHash, componentData, dataSize, outBuffer);
}

/**
 * @brief Set (add or update) a component on an entity by type hash
 * @param entityId The packed entity ID
 * @param typeHash The FNV-1a hash of the component type name
 * @param componentData Pointer to the component data to set
 * @param dataSize Size of the component data in bytes
 */
SCRIPT_API void ScriptAPI_SetComponent(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize) {
    SetComponentGeneric(entityId, typeHash, componentData, dataSize);
}

/**
 * @brief Check if an entity has a component by type hash
 * @param entityId The packed entity ID
 * @param typeHash The FNV-1a hash of the component type name
 * @return True if the entity has the component; false otherwise
 */
SCRIPT_API bool ScriptAPI_HasComponent(uint64_t entityId, uint32_t typeHash) {
    return HasComponentGeneric(entityId, typeHash);
}

/**
 * @brief Remove a component from an entity by type hash
 * @param entityId The packed entity ID
 * @param typeHash The FNV-1a hash of the component type name
 */
SCRIPT_API void ScriptAPI_RemoveComponent(uint64_t entityId, uint32_t typeHash) {
    RemoveComponentGeneric(entityId, typeHash);
}

/**
 * @brief Set the global ECS world for script API access
 * @param world Pointer to the ECS world
 */
SCRIPT_API void ScriptAPI_SetWorld(ECS::World* world) {
    g_scriptWorld = world;
}
