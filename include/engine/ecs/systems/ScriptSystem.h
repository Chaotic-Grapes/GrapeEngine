/* Start Header *****************************************************************/
/*!
\file    ScriptSystem.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of the ScriptSystem class, which manages
C# script execution through CoreCLR hosting. The system handles script lifecycle,
interop between C++ and C#, and provides a safe scripting environment.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SCRIPTSYSTEM_H
#define SCRIPTSYSTEM_H

#include "hostfxr.h"
#include "ecs/World.h"
#include "scene/Scene.h"
#include <string>
#include <memory>
#include <unordered_map>

// CoreCLR handle
using hostfxr_handle = void*;

// Platform-specific macros
#ifdef _WIN32
    // ****************************************** //
    // Unused for now but will be used later for path handling (see TODO 9)
    // Will probably be moved to a different header for project path management
    // We don't have to define these for C# as C# has Path.DirectorySeparatorChar etc.
    #define DIRECTORY_SEPARATOR "\\"
    #define PATH_SEPARATOR ";"
    // ****************************************** //
#else
    // Assumes we are targeting Linux/macOS eventually
    // Since this one's very easy to do, I'll just define it for now
    #define DIRECTORY_SEPARATOR "/"
    #define PATH_SEPARATOR ":"
#endif

// TODO:
// 1.  Build this as a separate DLL.
// 2.  Load/unload the DLL dynamically in the engine.
// 3.  Refine this further.
// 4.  Make GetComponent similar to how Unity handles it.
// 5.  Exception handling.
// 6.  AttachScript() and DetachScript() APIs, support for script parameters (constructor args).
// 7.  Support for async/await in C# scripts (if possible).
// 8.  Project/asset path.
// 9.  Open/read etc for asset files in the project.
// 10. Proper handling of AppDomain isolation.
// 11. Hot reloading of scripts.

// Maybe TODO:
// 1.  Separate API functions into another header or in their related classes

namespace ECS {
    /**
     * @brief Manages C# script execution through CoreCLR hosting.
     * 
     * The ScriptSystem provides a bridge between the C++ ECS and C# scripting layer.
     * It handles:
     * - CoreCLR runtime initialization and shutdown
     * - Loading C# assemblies
     * - Creating and managing script instances
     * - Calling script lifecycle methods (OnStart, OnUpdate, OnDestroy)
     * - Providing C++ callbacks to C# code
     */
    class ScriptSystem {
    public:
        ScriptSystem();
        ~ScriptSystem();

        // Prevent copying
        ScriptSystem(const ScriptSystem&) = delete;
        ScriptSystem& operator=(const ScriptSystem&) = delete;

        /**
         * @brief Initialize the CoreCLR runtime.
         * @param runtimeConfigPath Path to .NET runtime config (runtimeconfig.json)
         * @return true if initialization succeeded, false otherwise
         */
        bool Initialize(const char* runtimeConfigPath = "GrapeEngine.ScriptAPI.runtimeconfig.json");

        /**
         * @brief Shutdown the CoreCLR runtime and cleanup resources.
         */
        void Shutdown();

        /**
         * @brief Load a C# assembly containing scripts.
         * @param assemblyPath Path to the .dll file
         * @return true if loaded successfully
         */
        bool LoadAssembly(const char* assemblyPath);

        /**
         * @brief Attach a script to an entity.
         * @param world The ECS world
         * @param entity The entity to attach the script to
         * @param scriptTypeName Fully qualified C# type name (e.g., "MyGame.PlayerController")
         * @return true if script was attached successfully
         */
        bool AttachScript(World& world, Entity entity, const char* scriptTypeName);

        /**
         * @brief Remove a script from an entity.
         * @param world The ECS world
         * @param entity The entity to remove the script from
         */
        void DetachScript(World& world, Entity entity);

        /**
         * @brief Callback for when the system starts.
         * @param world The ECS world
         */
        static void OnStart(World& world);

        /**
         * @brief Callback for per-frame updates.
         * @param world The ECS world
         */
        static void Update(World& world);

        /**
         * @brief Callback for fixed-timestep updates.
         * @param world The ECS world
         */
        static void FixedUpdate(World& world);

        /**
         * @brief Calls late update on scripts.
         * @param world The ECS world
         */
        static void LateUpdate(World& world);

        /**
         * @brief Calls OnEnable/OnDisable on scripts based on Active component.
         * @param world The ECS world
         */
        static void UpdateActiveState(World& world);

        /**
         * @brief Callback when an entity with a script is destroyed.
         * @param world The ECS world
         */
        static void OnDestroy(World& world);

        /**
         * @brief Check if the system is initialized.
         */
        bool IsInitialized() const { return m_initialized; }

        /**
         * @brief Get the singleton instance (for C callbacks).
         */
        static ScriptSystem* GetInstance() { return s_instance; }

    private:
        // CoreCLR runtime handles
        hostfxr_handle  m_hostfxrContext                = nullptr;
        void*           m_loadAssemblyAndGetFunctionPtr = nullptr;

        // hostfxr function pointers (loaded dynamically)
        hostfxr_initialize_for_runtime_config_fn    m_initFxr               = nullptr;
        hostfxr_get_runtime_delegate_fn             m_getRuntimeDelegate    = nullptr;
        hostfxr_close_fn                            m_closeFxr              = nullptr;

        // Function pointer types for C# delegates
        using CreateScriptInstanceFn    = uint64_t(*)   (const char* typeName, uint64_t entityId);
        using DestroyScriptInstanceFn   = void(*)       (uint64_t handle);
        using CallStartFn               = void(*)       (uint64_t handle);
        using CallUpdateFn              = void(*)       (uint64_t handle);
        using CallFixedUpdateFn         = void(*)       (uint64_t handle);
        using CallLateUpdateFn          = void(*)       (uint64_t handle);
        using CallEnableFn              = void(*)       (uint64_t handle);
        using CallDisableFn             = void(*)       (uint64_t handle);
        using LoadGameAssemblyFn        = int(*)        (const char* assemblyPath);

        // Managed function delegates
        CreateScriptInstanceFn      m_createInstance    = nullptr;
        DestroyScriptInstanceFn     m_destroyInstance   = nullptr;
        CallStartFn                 m_callStart         = nullptr;
        CallUpdateFn                m_callUpdate        = nullptr;
        CallFixedUpdateFn           m_callFixedUpdate   = nullptr;
        CallLateUpdateFn            m_callLateUpdate    = nullptr;
        CallEnableFn                m_callEnable        = nullptr;
        CallDisableFn               m_callDisable       = nullptr;
        LoadGameAssemblyFn          m_loadGameAssembly  = nullptr;

        bool m_initialized = false;

        // Static instance for C callbacks
        static ScriptSystem* s_instance;

        // Helper methods
        bool InitializeHostFxr();
        bool LoadRuntime(const char* runtimeConfigPath);
        bool LoadManagedDelegates(const char* assemblyPath);

        // Internal cleanup
        void CleanupScript(Components::ScriptInstance& script);

        // ============================================================================
        // C++ Callbacks for C# Scripts (Generic Component API)
        // ============================================================================
        // These functions are exported and callable from C# via P/Invoke.
        // They provide safe, type-agnostic component access.
        
        friend bool ScriptAPI_GetComponent(uint64_t entityId, uint32_t typeHash, void* outBuffer, int bufferSize);
		friend bool ScriptAPI_AddComponent(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize, void* outBuffer);
        friend void ScriptAPI_SetComponent(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize);
        friend bool ScriptAPI_HasComponent(uint64_t entityId, uint32_t typeHash);
        friend void ScriptAPI_RemoveComponent(uint64_t entityId, uint32_t typeHash);
        friend void ScriptAPI_DestroyEntity(uint64_t entityId);
    };

}

// ============================================================================
// Exported C Functions for C# P/Invoke
// ============================================================================

#ifndef SCRIPT_API
#ifdef _WIN32
    #define SCRIPT_API extern "C" __declspec(dllexport)
#endif
#endif

/**
 * @brief Get a component from an entity by type hash.
 * @param entityId Entity to query
 * @param typeHash FNV-1a hash of component type name
 * @param outBuffer Buffer to write component data to
 * @param bufferSize Size of the buffer
 * @return true if component exists and was copied, false otherwise
 */
SCRIPT_API bool ScriptAPI_GetComponent(uint64_t entityId, uint32_t typeHash, void* outBuffer, int bufferSize);

/**
 * @brief Get a component from an entity by type hash.
 * @param entityId Entity to modify
 * @param typeHash FNV-1a hash of component type name
 * @param componentData Pointer to component data to add
 * @param dataSize Size of the component data
 * @param outBuffer Buffer to write the added component data to
 * @return true if component was added successfully, false otherwise
 */
SCRIPT_API bool ScriptAPI_AddComponent(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize, void* outBuffer);

/**
 * @brief Set a component on an entity by type hash.
 * @param entityId Entity to modify
 * @param typeHash FNV-1a hash of component type name
 * @param componentData Pointer to component data to copy
 * @param dataSize Size of the component data
 */
SCRIPT_API void ScriptAPI_SetComponent(uint64_t entityId, uint32_t typeHash, const void* componentData, int dataSize);

/**
 * @brief Check if an entity has a component.
 * @param entityId Entity to query
 * @param typeHash FNV-1a hash of component type name
 * @return true if the component exists
 */
SCRIPT_API bool ScriptAPI_HasComponent(uint64_t entityId, uint32_t typeHash);

/**
 * @brief Remove a component from an entity.
 * @param entityId Entity to modify
 * @param typeHash FNV-1a hash of component type name
 */
SCRIPT_API void ScriptAPI_RemoveComponent(uint64_t entityId, uint32_t typeHash);

/**
 * @brief Destroy an entity.
 * @param entityId Entity to destroy
 */
SCRIPT_API void ScriptAPI_DestroyEntity(uint64_t entityId);

/**
 * @brief Set the world instance for script API access.
 * @param world Pointer to the ECS world
 */
SCRIPT_API void ScriptAPI_SetWorld(ECS::World* world);

#endif
