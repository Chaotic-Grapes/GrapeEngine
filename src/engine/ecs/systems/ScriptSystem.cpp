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

#include "ecs/systems/ScriptSystem.h"
#include "ecs/Components.h"
#include "helpers/EntityUtils.h"

// CoreCLR hosting headers
#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

#include <iostream>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#define STR(s) L ## s
#define CHAR_T wchar_t
#define STRING_T std::wstring
#else
#include <dlfcn.h>
#include <limits.h>
#define STR(s) s
#define CHAR_T char
#define STRING_T std::string
#endif

namespace ECS {

    // Static instance for singleton access
    ScriptSystem* ScriptSystem::s_instance = nullptr;

    // Helper to convert string to platform-specific string
    // That is, of course, if we want to target other platforms but unsure yet
    STRING_T StringToNativeString(const char* str) {
#ifdef _WIN32
        int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], size);
        return result;
#else
        return std::string(str);
#endif
    }

    ScriptSystem::ScriptSystem() {
        if (s_instance == nullptr) {
            s_instance = this;
        }
    }

    ScriptSystem::~ScriptSystem() {
        Shutdown();
        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    bool ScriptSystem::Initialize(const char* runtimeConfigPath) {
        if (m_initialized) {
            std::cout << "[ScriptSystem] Already initialized" << '\n';
            return true;
        }

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
            return false;
        }

#ifdef _WIN32
        std::wcout << L"[ScriptSystem] Found hostfxr at: " << buffer << '\n';
#else
        std::cout << "[ScriptSystem] Found hostfxr at: " << buffer << '\n';
#endif

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
        script.ManagedHandle = 0xDEADBEEF; // Placeholder

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
                if (!active.Enabled) {
                    return;
                }

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

    void ScriptSystem::Update(World& world, float dt) {
        // Call OnUpdate for all initialized scripts
        world.Each<Components::ScriptInstance, Components::Active>(
            [dt](Entity entity, Components::ScriptInstance& script, const Components::Active& active) {
                if (!active.Enabled || !script.Initialized) {
                    return;
                }

                if (script.ManagedHandle != 0) {
                    // TODO: Call C# CallUpdate
                    // s_instance->m_callUpdate(script.ManagedHandle, dt);
                }
            }
        );
    }

    void ScriptSystem::OnDestroy(World& world) {
        // Cleanup scripts on entities that are being destroyed
        // This is typically called in a cleanup phase
        
        // For now, this is a placeholder
        // In a full implementation, we'd track destroyed entities and cleanup their scripts
    }

} // namespace ECS
