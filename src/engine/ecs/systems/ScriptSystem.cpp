/* Start Header *****************************************************************/
/*!
\file    ScriptSystem.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of the ScriptSystem class for C# CoreCLR hosting.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

// ************************ HEADERS ************************ //
// Engine headers
#include "ecs/systems/ScriptSystem.h"
#include "ecs/Components.h"
#include "ecs/ComponentRegistry.h"
#include "helpers/EntityUtils.h"

// CoreCLR hosting headers
#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

// Standard headers
#include <iostream>
#include <filesystem>
#include <cstring>

// ******************************************************** //

// Export macro
#ifdef _WIN32
    #ifdef ECS_EXPORTS
        #define ECS_API __declspec(dllexport) // This is dll EXPORT
    #else
        #define ECS_API __declspec(dllimport) // This is dll IMPORT
    #endif
#endif

#ifdef _WIN32
#include <Windows.h>
#define STR(s) L ## s
#define CHAR_T wchar_t
#define STRING_T std::wstring // std::wstring handles wide characters (UTF-16) natively on Windows.
// If we are targeting other platforms, we may need to adjust this.
#endif

namespace ECS {
    // Static instance for singleton access
    ScriptSystem* ScriptSystem::s_instance = nullptr;

    // Helper to convert string to platform-specific string
    // That is, of course, if we want to target other platforms but unsure yet
    STRING_T StringToNativeString(const char* str) {
#ifdef _WIN32
        // Get required size for conversion
        int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);

        // Allocate and convert to std::wstring for Windows
        std::wstring result(size, 0);

        // Convert the string but this time to the native string type
        MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], size);
        
        return result;
        // add #else directive here for other platforms if needed
        // but since windows needed conversion, probably returning
        // the string as-is is fine...?
        // I DO NOT HAVE A NON-WINDOWS SYSTEM TO TEST ON SO SOMEONE PLEASE HELP
#endif
    }

    ScriptSystem::ScriptSystem() {
        if (s_instance == nullptr) {
            s_instance = this;
        }
    }

    ScriptSystem::~ScriptSystem() {
        // Cleanup on destruction
        Shutdown();
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    bool ScriptSystem::Initialize(const char* runtimeConfigPath) {
        if (m_initialized) return true;

        // Use std::cout for logging rather than engine logging to avoid any dependency issues
        // If that is possible

        std::cout << "[ScriptSystem] Initializing CoreCLR hosting..." << '\n';

        if (!InitializeHostFxr()) {
            std::cerr << "[ScriptSystem] Failed to initialize hostfxr" << '\n';
            return false;
        }

        if (!LoadRuntime(runtimeConfigPath)) {
            std::cerr << "[ScriptSystem] Failed to load .NET runtime" << '\n';
            return false;
        }

        m_initialized = true;
        std::cout << "[ScriptSystem] CoreCLR hosting initialized successfully" << '\n';

        return true;
    }

    void ScriptSystem::Shutdown() {
        if (!m_initialized) {
            return;
        }

        std::cout << "[ScriptSystem] Shutting down CoreCLR..." << '\n';

        // TODO: Cleanup all active script instances
        // TODO: Close hostfxr context

        m_createInstance = nullptr;
        m_destroyInstance = nullptr;
        m_callStart = nullptr;
        m_callUpdate = nullptr;
        m_callFixedUpdate = nullptr;
        m_callLateUpdate = nullptr;
        m_callEnable = nullptr;
        m_callDisable = nullptr;
        m_loadAssemblyAndGetFunctionPtr = nullptr;
        m_hostfxrContext = nullptr;

        m_initialized = false;

        std::cout << "[ScriptSystem] Shutdown complete" << '\n';
    }

    bool ScriptSystem::InitializeHostFxr() {
        // Get path to hostfxr library
        CHAR_T buffer[1024];
        size_t buffer_size = sizeof(buffer) / sizeof(CHAR_T);
        
        int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
        if (rc != 0) {
            std::cerr << "[ScriptSystem] Failed to get hostfxr path. Error code: " << rc << '\n';
            std::cerr << "[ScriptSystem] Make sure .NET SDK/Runtime is installed" << '\n'; 
            // Unsure if the cerr above is actually true since we included CoreCLR headers which came with the SDK

            return false;
        }

        // For now, just verify we can find it
        // In a full implementation, we'd load it and get function pointers
        return true;
    }

    bool ScriptSystem::LoadRuntime(const char* runtimeConfigPath) {
        if (!runtimeConfigPath || strlen(runtimeConfigPath) == 0) {
            std::cerr << "[ScriptSystem] Invalid runtime config path" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading .NET runtime from: " << runtimeConfigPath << '\n';

        // TODO: Full implementation will:
        // 1. Call hostfxr_initialize_for_runtime_config
        // 2. Get runtime delegate for load_assembly_and_get_function_pointer
        // 3. Store the function pointer

        // For now, just verify the file exists
        if (!std::filesystem::exists(runtimeConfigPath)) {
            std::cerr << "[ScriptSystem] Runtime config file not found: " << runtimeConfigPath << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Runtime config verified" << '\n';
        return true;
    }

    bool ScriptSystem::LoadAssembly(const char* assemblyPath) {
        if (!m_initialized) {
            std::cerr << "[ScriptSystem] Cannot load assembly - system not initialized" << '\n';
            return false;
        }

        if (!assemblyPath || strlen(assemblyPath) == 0) {
            std::cerr << "[ScriptSystem] Invalid assembly path" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading assembly: " << assemblyPath << '\n';

        // Verify file exists
        if (!std::filesystem::exists(assemblyPath)) {
            std::cerr << "[ScriptSystem] Assembly file not found: " << assemblyPath << '\n';
            return false;
        }

        // Load managed delegates from the assembly
        if (!LoadManagedDelegates(assemblyPath)) {
            std::cerr << "[ScriptSystem] Failed to load managed delegates" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Assembly loaded successfully" << '\n';
        return true;
    }

    bool ScriptSystem::LoadManagedDelegates(const char* assemblyPath) {
        std::cout << "[ScriptSystem] Loading managed delegates from assembly..." << '\n';

        // TODO: Full implementation using load_assembly_and_get_function_pointer
        // 
        // This function will:
        // 1. Convert paths to native strings
        // 2. Call load_assembly_and_get_function_pointer for each ScriptHost method
        // 3. Store the function pointers
        //
        // Example (once CoreCLR hosting is complete):
        /*
        load_assembly_and_get_function_pointer_fn load_assembly_and_get_function_pointer = nullptr;
        load_assembly_and_get_function_pointer = (load_assembly_and_get_function_pointer_fn)m_loadAssemblyAndGetFunctionPtr;

        STRING_T assemblyPathNative = StringToNativeString(assemblyPath);
        STRING_T typeName = STR("GrapeEngine.Scripting.ScriptHost, GrapeEngine.ScriptAPI");

        // Load CreateScriptInstance
        STRING_T methodName = STR("CreateScriptInstance");
        STRING_T delegateType = STR("GrapeEngine.Scripting.ScriptHost+CreateScriptInstanceDelegate, GrapeEngine.ScriptAPI");
        
        int rc = load_assembly_and_get_function_pointer(
            assemblyPathNative.c_str(),
            typeName.c_str(),
            methodName.c_str(),
            delegateType.c_str(),
            nullptr,
            (void**)&m_createInstance
        );
        
        if (rc != 0 || m_createInstance == nullptr) {
            std::cerr << "[ScriptSystem] Failed to load CreateScriptInstance. Error: " << rc << '\n';
            return false;
        }

        // Similarly for DestroyScriptInstance, CallStart, CallUpdate...
        */

        // For now, just log that we're in placeholder mode
        std::cout << "[ScriptSystem] LoadManagedDelegates placeholder - TODO: Implement CoreCLR function pointer loading" << '\n';
        std::cout << "[ScriptSystem] Assembly path: " << assemblyPath << '\n';
        std::cout << "[ScriptSystem] Expected delegates:" << '\n';
        std::cout << "[ScriptSystem]   - CreateScriptInstance" << '\n';
        std::cout << "[ScriptSystem]   - DestroyScriptInstance" << '\n';
        std::cout << "[ScriptSystem]   - CallStart" << '\n';
        std::cout << "[ScriptSystem]   - CallUpdate" << '\n';
        std::cout << "[ScriptSystem]   - CallFixedUpdate" << '\n';
        std::cout << "[ScriptSystem]   - CallLateUpdate" << '\n';
        std::cout << "[ScriptSystem]   - CallEnable" << '\n';
        std::cout << "[ScriptSystem]   - CallDisable" << '\n';

        // Return true for now so the system can be tested
        // Once CoreCLR is implemented, this should return false if any delegate fails to load
        return true;
    }

    bool ScriptSystem::AttachScript(World& world, Entity entity, const char* scriptTypeName) {
        if (!m_initialized) {
            std::cerr << "[ScriptSystem] Cannot attach script - system not initialized" << '\n';
            return false;
        }

        if (!world.IsAlive(entity)) {
            std::cerr << "[ScriptSystem] Cannot attach script to invalid entity" << '\n';
            return false;
        }

        // Check if entity already has a script
        if (world.Has<Components::ScriptInstance>(entity)) {
            std::cout << "[ScriptSystem] Entity already has a script - detaching old one" << '\n';
            DetachScript(world, entity);
        }

        std::cout << "[ScriptSystem] Attaching script '" << scriptTypeName << "' to entity " 
                  << entity.Index << '\n';

        // Create script component
        Components::ScriptInstance script;
        script.ManagedHandle = 0; // Will be set when we call C#
        script.TypeHash = std::hash<std::string>{}(scriptTypeName);
        script.Initialized = false;
        
        // Copy type name (safely)
        size_t nameLen = strlen(scriptTypeName);
        if (nameLen >= sizeof(script.TypeName)) {
            nameLen = sizeof(script.TypeName) - 1;
        }
        memcpy(script.TypeName, scriptTypeName, nameLen);
        script.TypeName[nameLen] = '\0';

        // TODO: Call C# CreateScriptInstance
        // uint64_t entityPacked = EntityUtils::Pack(entity);
        // script.ManagedHandle = m_createInstance(scriptTypeName, entityPacked);
        
        // For now, just use a placeholder
        // Yes, despite the name, this is just a dummy value for placeholder
        // Whoever invented this is an actual genius
        script.ManagedHandle = 0xDEADBEEF; 

        // Add component to entity
        world.Add<Components::ScriptInstance>(entity, script);

        std::cout << "[ScriptSystem] Script attached successfully" << '\n';
        return true;
    }

    void ScriptSystem::DetachScript(World& world, Entity entity) {
        if (!world.IsAlive(entity)) {
            return;
        }

        auto* script = world.TryGet<Components::ScriptInstance>(entity);
        if (!script) {
            return;
        }

        std::cout << "[ScriptSystem] Detaching script from entity " << entity.Index << '\n';

        CleanupScript(*script);
        world.Remove<Components::ScriptInstance>(entity);
    }

    void ScriptSystem::CleanupScript(Components::ScriptInstance& script) {
        if (script.ManagedHandle == 0) {
            return;
        }

        // TODO: Call C# DestroyScriptInstance
        // m_destroyInstance(script.ManagedHandle);

        script.ManagedHandle = 0;
        script.Initialized = false;
    }

    void ScriptSystem::OnStart(World& world) {
        // Call OnStart for all uninitialized scripts
        world.Each<Components::ScriptInstance, Components::Active>(
            [](Entity entity, Components::ScriptInstance& script, const Components::Active& active) {
                if (!active.Enabled)
                    return;

                if (!script.Initialized && script.ManagedHandle != 0) {
                    std::cout << "[ScriptSystem] Calling OnStart for script on entity " 
                              << entity.Index << '\n';

                    // TODO: Call C# CallStart
                    // s_instance->m_callStart(script.ManagedHandle);

                    script.Initialized = true;
                }
            }
        );
    }

    void ScriptSystem::Update(World& world) {
        // Call OnUpdate for all initialized scripts
        world.Each<Components::ScriptInstance, Components::Active>(
            [](Entity entity, Components::ScriptInstance& script, const Components::Active& active) {
                if (!active.Enabled || !script.Initialized)
                    return;

                if (script.ManagedHandle != 0) {
                    // TODO: Call C# CallUpdate
                    // s_instance->m_callUpdate(script.ManagedHandle);
                }
            }
        );
    }

    void ScriptSystem::FixedUpdate(World& world) {
        // Call OnFixedUpdate for all initialized scripts
        world.Each<Components::ScriptInstance, Components::Active>(
            [](Entity entity, Components::ScriptInstance& script, const Components::Active& active) {
                if (!active.Enabled || !script.Initialized)
                    return;

                if (script.ManagedHandle != 0) {
                    // TODO: Call C# CallFixedUpdate
                    // s_instance->m_callFixedUpdate(script.ManagedHandle);
                }
            }
        );
    }

    void ScriptSystem::LateUpdate(World& world) {
        // Call OnLateUpdate for all initialized scripts
        world.Each<Components::ScriptInstance, Components::Active>(
            [](Entity entity, Components::ScriptInstance& script, const Components::Active& active) {
                if (!active.Enabled || !script.Initialized)
                    return;

                if (script.ManagedHandle != 0) {
                    // TODO: Call C# CallLateUpdate
                    // s_instance->m_callLateUpdate(script.ManagedHandle);
                }
            }
        );
    }

    void ScriptSystem::UpdateActiveState(World& world) {
        // Track active state changes to call OnEnable/OnDisable
        // This requires storing previous active state in ScriptInstance component
        // For now, this is a placeholder
        
        // TODO: Implement active state tracking
        // When Active.Enabled changes from false->true: call m_callEnable
        // When Active.Enabled changes from true->false: call m_callDisable
        
        std::cout << "[ScriptSystem] UpdateActiveState placeholder - TODO: Track state changes" << '\n';
    }

    void ScriptSystem::OnDestroy(World& world) {
        // Cleanup scripts on entities that are being destroyed
        // This is typically called in a cleanup phase
        
        // For now, this is a placeholder
        // In a full implementation, we'd track destroyed entities and cleanup their scripts
    }

}

// ============================================================================
// Generic Component API Implementation
// ============================================================================
// These functions provide C# scripts with type-safe, generic component access
// using FNV-1a hash matching between C++ and C# type names.
// ============================================================================

namespace {
    // FNV-1a hash algorithm - must match C# implementation exactly
    uint32_t FNV1aHash(const char* str) {
        const uint32_t FNV_PRIME = 0x01000193;
        const uint32_t FNV_OFFSET = 0x811C9DC5;

        uint32_t hash = FNV_OFFSET;
        while (*str) {
            hash ^= static_cast<uint8_t>(*str++);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    // Get the world instance - for now we'll use a global pointer
    // TODO: Replace with proper singleton or dependency injection
    ECS::World* g_scriptWorld = nullptr;

    void SetScriptWorld(ECS::World* world) {
        g_scriptWorld = world;
    }

    ECS::World* GetScriptWorld() {
        return g_scriptWorld;
    }

    // Generic component access using raw memory operations
    bool GetComponentGeneric(uint64_t entityId, uint32_t typeHash, void* outBuffer, int bufferSize) {
        ECS::World* world = GetScriptWorld();
        if (!world) {
            std::cerr << "[ScriptAPI] No world set for script access" << '\n';
            return false;
        }

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity))
            return false;

        // Use a dispatch table based on component type
        // This is necessary because World::TryGet is templated
        // We need to instantiate it for each concrete component type
        
        // Macro to generate case statement for each component
        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                auto* comp = world->TryGet<ECS::Components::ComponentType>(entity); \
                if (!comp) return false; \
                if (bufferSize < static_cast<int>(sizeof(ECS::Components::ComponentType))) { \
                    std::cerr << "[ScriptAPI] Buffer too small" << '\n'; \
                    return false; \
                } \
                std::memcpy(outBuffer, comp, sizeof(ECS::Components::ComponentType)); \
                return true; \
            }

        // Register all component types
        // TEDIOUS WORK AHEAD !!! WATCH OUT!!!
        HANDLE_COMPONENT_TYPE(LocalTransform,       "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform,       "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D,     "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D,       "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D,    "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D,          "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D,    "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D,        "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D,     "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity,             "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration,         "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity,      "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody,            "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider,          "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider,       "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D,     "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D,        "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D,           "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D,          "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D,             "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Camera,               "Camera")
        HANDLE_COMPONENT_TYPE(CameraMatrices,       "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active,               "Active")
        HANDLE_COMPONENT_TYPE(Name,                 "Name")
        HANDLE_COMPONENT_TYPE(TagMask,              "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime,             "Lifetime")

        #undef HANDLE_COMPONENT_TYPE // Don't want to pollute global namespace

        std::cerr << "[ScriptAPI] Unknown component type hash: " << typeHash << '\n';
        return false;
    }

    void SetComponentGeneric(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize) {
        ECS::World* world = GetScriptWorld();
        if (!world) {
            std::cerr << "[ScriptAPI] No world set for script access" << '\n';
            return;
        }

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity))
            return;

        // Similar dispatch pattern for SetComponent
        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                if (dataSize != sizeof(ECS::Components::ComponentType)) { \
                    std::cerr << "[ScriptAPI] Component size mismatch" << '\n'; \
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

        HANDLE_COMPONENT_TYPE(LocalTransform,       "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform,       "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D,     "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D,       "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D,    "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D,          "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D,    "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D,        "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D,     "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity,             "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration,         "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity,      "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody,            "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider,          "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider,       "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D,     "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D,        "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D,           "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D,          "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D,             "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Camera,               "Camera")
        HANDLE_COMPONENT_TYPE(CameraMatrices,       "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active,               "Active")
        HANDLE_COMPONENT_TYPE(Name,                 "Name")
        HANDLE_COMPONENT_TYPE(TagMask,              "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime,             "Lifetime")

        #undef HANDLE_COMPONENT_TYPE

        std::cerr << "[ScriptAPI] Unknown component type hash: " << typeHash << '\n';
    }

    bool HasComponentGeneric(uint64_t entityId, uint32_t typeHash) {
        ECS::World* world = GetScriptWorld();
        if (!world)
            return false;

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity))
            return false;

        // Dispatch removal based on component type
        // If you see the backslash, it means the macro continues to the next line
        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                return world->Has<ECS::Components::ComponentType>(entity); \
            }

        HANDLE_COMPONENT_TYPE(LocalTransform,       "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform,       "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D,     "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D,       "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D,    "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D,          "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D,    "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D,        "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D,     "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity,             "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration,         "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity,      "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody,            "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider,          "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider,       "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D,     "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D,        "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D,           "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D,          "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D,             "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Camera,               "Camera")
        HANDLE_COMPONENT_TYPE(CameraMatrices,       "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active,               "Active")
        HANDLE_COMPONENT_TYPE(Name,                 "Name")
        HANDLE_COMPONENT_TYPE(TagMask,              "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime,             "Lifetime")

        #undef HANDLE_COMPONENT_TYPE

        return false;
    }

    void RemoveComponentGeneric(uint64_t entityId, uint32_t typeHash) {
        ECS::World* world = GetScriptWorld();
        if (!world)
            return;

        ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
        if (!world->IsAlive(entity))
            return;

        #define HANDLE_COMPONENT_TYPE(ComponentType, HashName) \
            if (typeHash == FNV1aHash(HashName)) { \
                world->Remove<ECS::Components::ComponentType>(entity); \
                return; \
            }

        HANDLE_COMPONENT_TYPE(LocalTransform,       "LocalTransform")
        HANDLE_COMPONENT_TYPE(WorldTransform,       "WorldTransform")
        HANDLE_COMPONENT_TYPE(LinearVelocity2D,     "LinearVelocity2D")
        HANDLE_COMPONENT_TYPE(Acceleration2D,       "Acceleration2D")
        HANDLE_COMPONENT_TYPE(AngularVelocity2D,    "AngularVelocity2D")
        HANDLE_COMPONENT_TYPE(Rigidbody2D,          "Rigidbody2D")
        HANDLE_COMPONENT_TYPE(PhysicsMaterial2D,    "PhysicsMaterial2D")
        HANDLE_COMPONENT_TYPE(BoxCollider2D,        "BoxCollider2D")
        HANDLE_COMPONENT_TYPE(CircleCollider2D,     "CircleCollider2D")
        HANDLE_COMPONENT_TYPE(Velocity,             "Velocity")
        HANDLE_COMPONENT_TYPE(Acceleration,         "Acceleration")
        HANDLE_COMPONENT_TYPE(AngularVelocity,      "AngularVelocity")
        HANDLE_COMPONENT_TYPE(Rigidbody,            "Rigidbody")
        HANDLE_COMPONENT_TYPE(BoxCollider,          "BoxCollider")
        HANDLE_COMPONENT_TYPE(SphereCollider,       "SphereCollider")
        HANDLE_COMPONENT_TYPE(SpriteRenderer2D,     "SpriteRenderer2D")
        HANDLE_COMPONENT_TYPE(ShapeCircle2D,        "ShapeCircle2D")
        HANDLE_COMPONENT_TYPE(ShapeBox2D,           "ShapeBox2D")
        HANDLE_COMPONENT_TYPE(ShapeLine2D,          "ShapeLine2D")
        HANDLE_COMPONENT_TYPE(ZIndex2D,             "ZIndex2D")
        HANDLE_COMPONENT_TYPE(Camera,               "Camera")
        HANDLE_COMPONENT_TYPE(CameraMatrices,       "CameraMatrices")
        HANDLE_COMPONENT_TYPE(Active,               "Active")
        HANDLE_COMPONENT_TYPE(Name,                 "Name")
        HANDLE_COMPONENT_TYPE(TagMask,              "TagMask")
        HANDLE_COMPONENT_TYPE(Lifetime,             "Lifetime")

        #undef HANDLE_COMPONENT_TYPE // Same
    }
}

// ============================================================================
// Exported C Functions for P/Invoke
// ============================================================================

#ifndef SCRIPT_API
#ifdef _WIN32
    // __declspec(dllexport) means the function is exported from the DLL
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

SCRIPT_API bool ScriptAPI_GetComponent(uint64_t entityId, uint32_t typeHash, void* outBuffer, int bufferSize) {
    return GetComponentGeneric(entityId, typeHash, outBuffer, bufferSize);
}

SCRIPT_API void ScriptAPI_SetComponent(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize) {
    SetComponentGeneric(entityId, typeHash, componentData, dataSize);
}

SCRIPT_API bool ScriptAPI_HasComponent(uint64_t entityId, uint32_t typeHash) {
    return HasComponentGeneric(entityId, typeHash);
}

SCRIPT_API void ScriptAPI_RemoveComponent(uint64_t entityId, uint32_t typeHash) {
    RemoveComponentGeneric(entityId, typeHash);
}

SCRIPT_API void ScriptAPI_DestroyEntity(uint64_t entityId) {
    ECS::World* world = GetScriptWorld();
    if (!world) {
        std::cerr << "[ScriptAPI] No world set for script access" << '\n';
        return;
    }

    ECS::Entity entity = ECS::EntityUtils::Unpack(entityId);
    if (world->IsAlive(entity)) {
        world->Destroy(entity);
    }
}

// World management - must be called before using script API
SCRIPT_API void ScriptAPI_SetWorld(ECS::World* world) {
    SetScriptWorld(world);
}
