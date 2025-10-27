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

// ********************************** HEADERS ********************************** //
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
// **************************************************************************** //

// ******************************* EXPORT MACRO ******************************* //
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
// *************************************************************************** //

// ******************************************* NOTE ******************************************* //
// Anything to do with char/string must be handled carefully due to platform differences.       //
// Windows uses UTF-16 wide chars. Therefore, use the following helpers:                        //
// StringToNativeString(const char* str) - converts UTF-8 C string to native string             //
// MultiByteToWideChar - Windows API function to convert UTF-8 to UTF-16 wide chars             //
// STR(s) - macro to convert string literals to native string literals                          //
// CHAR_T - platform-specific char type (wchar_t on Windows)                                    //
// STRING_T - platform-specific string type (std::wstring on Windows)                           //
//                                                                                              //
// -------------------------------------------------------------------------------------------- //
//                                                                                              //
// TODO: Add support for non-Windows platforms if needed.                                       //
// Other notes: Linux typically uses UTF-8 with char/std::string. MacOS is similar.             //
// Linux uses .so files instead of .dll. Adjust loading code accordingly.                       //
// What about macOS? Need to check.                                                             //
//                                                                                              //
// Windows: Uses UTF-16 wide chars (wchar_t/std::wstring).                                      //
// Linux: Uses UTF-8 with char/std::string.                                                     //
// MacOS: Similar to Linux, uses UTF-8 with char/std::string.                                   //
// ******************************************************************************************** //

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
        if (s_instance == nullptr)
            s_instance = this;
    }

    ScriptSystem::~ScriptSystem() {
        // Cleanup on destruction
        Shutdown();

        if (s_instance == this)
            s_instance = nullptr;
    }

    bool ScriptSystem::Initialize(const char* runtimeConfigPath) {
        if (m_initialized)
            return true;

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
        if (!m_initialized)
            return; // why shutdown when not initialized?

        std::cout << "[ScriptSystem] Shutting down CoreCLR..." << '\n';

        // Cleanup all function pointers
        m_createInstance = nullptr;
        m_destroyInstance = nullptr;
        m_callStart = nullptr;
        m_callUpdate = nullptr;
        m_callFixedUpdate = nullptr;
        m_callLateUpdate = nullptr;
        m_callEnable = nullptr;
        m_callDisable = nullptr;
        m_loadAssemblyAndGetFunctionPtr = nullptr;

        // Close hostfxr context
        if (m_hostfxrContext != nullptr && m_closeFxr != nullptr) {
            std::cout << "[ScriptSystem] Closing hostfxr context..." << '\n';
            m_closeFxr(m_hostfxrContext);
            m_hostfxrContext = nullptr;
        }

        // Clear hostfxr function pointers
        m_initFxr = nullptr;
        m_getRuntimeDelegate = nullptr;
        m_closeFxr = nullptr;

        // Mark as uninitialized
        m_initialized = false;

        std::cout << "[ScriptSystem] Shutdown successful" << '\n';
    }

    bool ScriptSystem::InitializeHostFxr() {
        // Get path to hostfxr library
        CHAR_T buffer[1024];
        size_t bufferSize = sizeof(buffer) / sizeof(CHAR_T);
        
        int rc = get_hostfxr_path(buffer, &bufferSize, nullptr);
        if (rc != 0) {
            std::cerr << "[ScriptSystem] Failed to get hostfxr path. Error code: " << rc << '\n';
            std::cerr << "[ScriptSystem] Make sure .NET SDK/Runtime is installed" << '\n'; 
            // Unsure if the cerr above is actually true since we included CoreCLR headers which came with the SDK

            return false;
        }

        // Here begins the platform-specific loading of hostfxr
        // Windows is .dll and Linux is .so
        // ...and macOS is .so? (need to check if we are targeting other platforms)
#ifdef _WIN32        
        // Load hostfxr.dll
        HMODULE hostfxrLib = LoadLibraryW(buffer);
        if (!hostfxrLib) {
            std::cerr << "[ScriptSystem] Failed to load hostfxr.dll. Error: " << GetLastError() << '\n';
            return false;
        }

        // Get function pointers
        m_initFxr            = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(hostfxrLib, "hostfxr_initialize_for_runtime_config");
        m_getRuntimeDelegate = (hostfxr_get_runtime_delegate_fn)GetProcAddress(hostfxrLib, "hostfxr_get_runtime_delegate");
        m_closeFxr           = (hostfxr_close_fn)GetProcAddress(hostfxrLib, "hostfxr_close");

        if (!m_initFxr || !m_getRuntimeDelegate || !m_closeFxr) {
            std::cerr << "[ScriptSystem] Failed to get hostfxr function pointers" << '\n';
            FreeLibrary(hostfxrLib);
            return false;
        }
        // if adding other platforms, add #elif directives here
        // Read note above about other platforms thank you
#endif

        std::cout << "[ScriptSystem] hostfxr loaded successfully" << '\n';
        return true;
    }

    bool ScriptSystem::LoadRuntime(const char* runtimeConfigPath) {
        if (!runtimeConfigPath || std::strlen(runtimeConfigPath) == 0) {
            std::cerr << "[ScriptSystem] Invalid runtime config path" << '\n';
            return false;
        }

        if (!m_initFxr || !m_getRuntimeDelegate) {
            std::cerr << "[ScriptSystem] hostfxr not initialized" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading .NET runtime from: " << runtimeConfigPath << '\n';

        // Verify the file exists
        if (!std::filesystem::exists(runtimeConfigPath)) {
            std::cerr << "[ScriptSystem] Runtime config file not found: " << runtimeConfigPath << '\n';
            return false;
        }

        // Convert to native string
        STRING_T configPath = StringToNativeString(runtimeConfigPath);

        // Initialize the .NET runtime
        hostfxr_initialize_parameters params = {};
        params.size = sizeof(hostfxr_initialize_parameters); // Must set size correctly

        // Call hostfxr_initialize_for_runtime_config
        int rc = m_initFxr(configPath.c_str(), &params, &m_hostfxrContext);
        if (rc != 0 || m_hostfxrContext == nullptr) {
            std::cerr << "[ScriptSystem] Failed to initialize runtime. Error code: "
                      << std::hex << rc << std::dec << '\n'; // std::hex for error codes apparently
                      // Don't forget to switch back to decimal after 

            return false;
        }

        std::cout << "[ScriptSystem] Runtime initialized successfully" << '\n';

        // Get the load_assembly_and_get_function_pointer delegate
        rc = m_getRuntimeDelegate(
            m_hostfxrContext,
            hdt_load_assembly_and_get_function_pointer,
            &m_loadAssemblyAndGetFunctionPtr
        );

        if (rc != 0 || m_loadAssemblyAndGetFunctionPtr == nullptr) {
            std::cerr << "[ScriptSystem] Failed to get runtime delegate. Error code: "
                      << std::hex << rc << std::dec << '\n';

            m_closeFxr(m_hostfxrContext);
            m_hostfxrContext = nullptr;
            return false;
        }

        std::cout << "[ScriptSystem] Runtime delegate acquired" << '\n';
        return true;
    }

    bool ScriptSystem::LoadAssembly(const char* assemblyPath) {
        if (!m_initialized) {
            std::cerr << "[ScriptSystem] Cannot load assembly" << '\n';
            return false;
        }

        if (!assemblyPath || std::strlen(assemblyPath) == 0) {
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

        // Load the game assembly into the AppDomain so that scripts can be instantiated
        if (m_loadGameAssembly) {
            std::cout << "[ScriptSystem] Loading game assembly into AppDomain..." << '\n';
            
            // Derive the game assembly path from the script API assembly path
            std::filesystem::path apiPath(assemblyPath);
            std::filesystem::path gameAssemblyPath = apiPath.parent_path() / "MyGame.dll"; // TODO: Change "MyGame.dll" to actual game assembly name
            
            if (!std::filesystem::exists(gameAssemblyPath)) {
                std::cerr << "[ScriptSystem] Game assembly not found: " << gameAssemblyPath << '\n';
                return false;
            }
            
            int result = m_loadGameAssembly(gameAssemblyPath.string().c_str());

            if (result == 0) {
                std::cerr << "[ScriptSystem] Failed to load game assembly into AppDomain" << '\n';
                return false;
            }

            std::cout << "[ScriptSystem] Game assembly loaded into AppDomain successfully" << '\n';
        }

        std::cout << "[ScriptSystem] Assembly loaded successfully" << '\n';
        return true;
    }

    bool ScriptSystem::LoadManagedDelegates(const char* assemblyPath) {
        std::cout << "[ScriptSystem] Loading managed delegates from assembly..." << '\n';

        if (!m_loadAssemblyAndGetFunctionPtr) {
            std::cerr << "[ScriptSystem] Runtime not loaded" << '\n';
            return false;
        }

        // Cast to the correct function pointer type
        load_assembly_and_get_function_pointer_fn loadAssemblyAndGetFunctionPointer =
            (load_assembly_and_get_function_pointer_fn)m_loadAssemblyAndGetFunctionPtr;

        STRING_T assemblyPathNative = StringToNativeString(assemblyPath);
        STRING_T typeName = STR("GrapeEngine.Scripting.ScriptHost, GrapeEngine.ScriptAPI"); // Important: namespaces MUST match

        // Helper lambda to load a single delegate
        // Using this helps to avoid tedious repetitive code into one single line
        auto loadDelegate = [&](const CHAR_T* methodName, void** outDelegate) -> bool {
            int rc = loadAssemblyAndGetFunctionPointer(
                assemblyPathNative.c_str(),
                typeName.c_str(),
                methodName,
                UNMANAGEDCALLERSONLY_METHOD,  // Using UnmanagedCallersOnly delegates
                nullptr,
                outDelegate
            );

            if (rc != 0 || *outDelegate == nullptr) {
#ifdef _WIN32
                std::wcerr << L"[ScriptSystem] Failed to load delegate: " << methodName 
                           << L". Error: " << std::hex << rc << std::dec << '\n';
#endif
                return false;
            }
            return true;
        };

        // Load all delegates necessary for scripting
        // Note: If more delegates are added in the future, add them here
        // Don't forget to add logging for each delegate load failure
        // so we know which one failed

		// First argument is the method name in C#, second is the out delegate pointer
		// Use STR macro for method names to convert to native string literals
		// The out delegates are void** so we need to cast them appropriately

        // ******************* LOAD ALL DELEGATES HERE ******************* //
        std::cout << "[ScriptSystem] Loading CreateScriptInstance..." << '\n';
        if (!loadDelegate(STR("CreateScriptInstance"), reinterpret_cast<void**>(&m_createInstance))) {
            std::cerr << "[ScriptSystem] Failed to load CreateScriptInstance delegate" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading DestroyScriptInstance..." << '\n';
        if (!loadDelegate(STR("DestroyScriptInstance"), reinterpret_cast<void**>(&m_destroyInstance))) {
            std::cerr << "[ScriptSystem] Failed to load DestroyScriptInstance delegate" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading CallStart..." << '\n';
        if (!loadDelegate(STR("CallStart"), reinterpret_cast<void**>(&m_callStart))) {
            std::cerr << "[ScriptSystem] Failed to load CallStart delegate" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading CallUpdate..." << '\n';
        if (!loadDelegate(STR("CallUpdate"), reinterpret_cast<void**>(&m_callUpdate))) {
            std::cerr << "[ScriptSystem] Failed to load CallUpdate delegate" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading CallFixedUpdate..." << '\n';
        if (!loadDelegate(STR("CallFixedUpdate"), reinterpret_cast<void**>(&m_callFixedUpdate))) {
            std::cerr << "[ScriptSystem] Failed to load CallFixedUpdate delegate" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading CallLateUpdate..." << '\n';
        if (!loadDelegate(STR("CallLateUpdate"), reinterpret_cast<void**>(&m_callLateUpdate))) {
            std::cerr << "[ScriptSystem] Failed to load CallLateUpdate delegate" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading CallEnable..." << '\n';
        if (!loadDelegate(STR("CallEnable"), reinterpret_cast<void**>(&m_callEnable))) {
            std::cerr << "[ScriptSystem] Failed to load CallEnable delegate" << '\n';
            return false;
        }

        std::cout << "[ScriptSystem] Loading CallDisable..." << '\n';
        if (!loadDelegate(STR("CallDisable"), reinterpret_cast<void**>(&m_callDisable))) {
            std::cerr << "[ScriptSystem] Failed to load CallDisable delegate" << '\n';
            return false;
        }

        // The most important part of CoreCLR hosting: loading the game assembly
        // Without this, nothing works!
        std::cout << "[ScriptSystem] Loading LoadGameAssembly..." << '\n';
        if (!loadDelegate(STR("LoadGameAssembly"), reinterpret_cast<void**>(&m_loadGameAssembly))) {
            std::cerr << "[ScriptSystem] Failed to load LoadGameAssembly delegate" << '\n';
            return false;
        }
        // **************************************************************** //

        std::cout << "[ScriptSystem] All managed delegates loaded successfully!" << '\n';
        return true;
    }

    bool ScriptSystem::AttachScript(World& world, Entity entity, const char* scriptTypeName) {
        if (!m_initialized) {
            std::cerr << "[ScriptSystem] Cannot attach script (system not initialized)" << '\n';
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
        script.ManagedHandle = 0; // Will be set when we call C#. This is just initialization.
        script.TypeHash = static_cast<uint32_t>(std::hash<std::string>{}(scriptTypeName));
        script.Initialized = false;
        
        // Copy type name (safely)
        size_t nameLen = std::strlen(scriptTypeName);
        if (nameLen >= sizeof(script.TypeName))
            nameLen = sizeof(script.TypeName) - 1;
        std::memcpy(script.TypeName, scriptTypeName, nameLen);
        script.TypeName[nameLen] = '\0';

        // Call C# CreateScriptInstance
        if (!m_createInstance) {
            std::cerr << "[ScriptSystem] CreateInstance delegate not loaded" << '\n';
            return false;
        }

        uint64_t entityPacked = EntityUtils::Pack(entity);
        script.ManagedHandle = m_createInstance(scriptTypeName, entityPacked);
        
        if (script.ManagedHandle == 0) {
            std::cerr << "[ScriptSystem] Failed to create script instance" << '\n';
            return false;
        }

        // Add component to entity
        world.Add<Components::ScriptInstance>(entity, script);

        std::cout << "[ScriptSystem] Script attached successfully" << '\n';
        return true;
    }

    void ScriptSystem::DetachScript(World& world, Entity entity) {
        if (!world.IsAlive(entity))
            return;

        auto* script = world.TryGet<Components::ScriptInstance>(entity);
        if (!script)
            return;

        std::cout << "[ScriptSystem] Detaching script from entity " << entity.Index << '\n';

        CleanupScript(*script);
        world.Remove<Components::ScriptInstance>(entity);
    }

    void ScriptSystem::CleanupScript(Components::ScriptInstance& script) {
        if (script.ManagedHandle == 0)
            return;

        // Call C# DestroyScriptInstance
        if (m_destroyInstance)
            m_destroyInstance(script.ManagedHandle);

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

                    // Call C# CallStart
                    if (s_instance && s_instance->m_callStart)
                        s_instance->m_callStart(script.ManagedHandle);

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
                    // Call C# CallUpdate
                    if (s_instance && s_instance->m_callUpdate)
                        s_instance->m_callUpdate(script.ManagedHandle);
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
                    // Call C# CallFixedUpdate
                    if (s_instance && s_instance->m_callFixedUpdate)
                        s_instance->m_callFixedUpdate(script.ManagedHandle);
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
                    // Call C# CallLateUpdate
                    if (s_instance && s_instance->m_callLateUpdate)
                        s_instance->m_callLateUpdate(script.ManagedHandle);
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
        HANDLE_COMPONENT_TYPE(Camera3D,             "Camera3D")
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
        HANDLE_COMPONENT_TYPE(Camera3D,             "Camera3D")
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
        HANDLE_COMPONENT_TYPE(Camera3D,             "Camera3D")
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
        HANDLE_COMPONENT_TYPE(Camera3D,             "Camera3D")
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
    // __declspec(dllexport) means the function is exported from DLL
	// This is necessary for P/Invoke in C# to find the functions
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

SCRIPT_API uint64_t ScriptAPI_CreateEntity() {
    ECS::World* world = GetScriptWorld();
    if (!world) {
        std::cerr << "[ScriptAPI] No world set for script access" << '\n';
        return 0;
    }

    ECS::Entity entity = world->Create();
    return ECS::EntityUtils::Pack(entity);
}

// World management - must be called before using script API
SCRIPT_API void ScriptAPI_SetWorld(ECS::World* world) {
    SetScriptWorld(world);
}
