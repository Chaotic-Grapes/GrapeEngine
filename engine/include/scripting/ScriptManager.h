/* Start Header *****************************************************************/
/*!
\file    ScriptManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Manages C# scripting integration through CoreCLR hosting.

ScriptManager is a service that:
- Initializes and hosts the CoreCLR runtime
- Loads C# assemblies (pre-compiled or via Roslyn)
- Discovers ISystem implementations in C# code
- Creates ScriptSystemWrapper instances for each C# system
- Registers wrapped systems with the global SystemManager
- Handles hot reload and assembly unloading

C# scripts are full ECS systems, not components. They implement ISystem
and are registered globally, just like native C++ systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SCRIPTMANAGER_H
#define SCRIPTMANAGER_H

#include <coreclr/hostfxr.h>
#include "ecs/ISystem.h"
#include "ecs/SystemManager.h"
#include "core/Logger.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <optional>
#include <functional>

// CoreCLR handle
using hostfxr_handle = void*;

namespace ECS {

    // Forward declarations
    class ScriptSystemWrapper;

    /**
     * @brief Manages C# scripting through CoreCLR hosting.
     * 
     * This is NOT an ECS system. It's a service layer that bridges
     * C++ and C# by hosting the CLR and managing script assemblies.
     * 
     * Architecture:
     * 1. ScriptManager initializes CoreCLR
     * 2. Loads C# assemblies (GameScripts.dll, etc.)
     * 3. Reflects to find all ISystem implementations
     * 4. Creates ScriptSystemWrapper for each C# system
     * 5. Registers wrappers with SystemManager
     * 
     * C# systems then execute like any other system in the engine.
     * 
     * Example usage:
     * @code
     * ScriptManager scriptManager;
     * SystemManager systemManager;
     * 
     * // Initialize CLR
     * scriptManager.InitializeCLR("GrapeEngine.Scripting.runtimeconfig.json");
     * 
     * // Load game scripts
     * scriptManager.LoadAssembly("GameScripts.dll");
     * 
     * // Discover and register all C# systems
     * scriptManager.RegisterScriptedSystems(systemManager);
     * 
     * // Now C# systems run alongside native systems
     * systemManager.Update(world, deltaTime);
     * @endcode
     */
    class GRAPEENGINE_API ScriptManager {
    public:
        ScriptManager();
        ~ScriptManager();

        // Prevent copying
        ScriptManager(const ScriptManager&) = delete;
        ScriptManager& operator=(const ScriptManager&) = delete;

        // ====================================================================
        // CLR Initialization
        // ====================================================================

        /**
         * @brief Initialize the CoreCLR runtime.
         * @param runtimeConfigPath Path to .NET runtime config (runtimeconfig.json)
         * @return true if initialization succeeded
         * 
         * Must be called before loading any assemblies.
         */
        bool InitializeCLR(const char* runtimeConfigPath = "GrapeEngine.Scripting.runtimeconfig.json");

        /**
         * @brief Shutdown the CoreCLR runtime and cleanup.
         * 
         * Unloads all assemblies, destroys scripted systems, and shuts down CLR.
         */
        void ShutdownCLR();

        /**
         * @brief Check if CLR is initialized.
         */
        bool IsInitialized() const { return m_initialized; }

        // ====================================================================
        // Assembly Management
        // ====================================================================

        /**
         * @brief Load a C# assembly containing scripted systems.
         * @param assemblyPath Path to the .dll file
         * @return true if loaded successfully
         * 
         * After loading, call DiscoverAndRegisterSystems() to register
         * any ISystem implementations found in the assembly.
         */
        bool LoadAssembly(const std::string& assemblyPath);

        /**
         * @brief Unload a previously loaded assembly.
         * @param assemblyPath Path to the assembly to unload
         * @return true if unloaded successfully
         * 
         * Note: This will destroy all systems from that assembly.
         * For hot reload, use ReloadAssembly() instead.
         */
        bool UnloadAssembly(const std::string& assemblyPath);

        /**
         * @brief Reload an assembly for hot reload support.
         * @param assemblyPath Path to the assembly to reload
         * @return true if reloaded successfully
         * 
         * Preserves system state where possible during reload.
         */
        bool ReloadAssembly(const std::string& assemblyPath);

        /**
         * @brief Handle hot reload completion (called by C# when reload finishes).
         * @param assemblyPath Path to the reloaded assembly
         * 
         * Unregisters old systems (calling OnDestroy) and re-discovers/registers new systems.
         * This is invoked by the managed runtime when hot reload completes.
         */
        void HandleHotReloadCompletion(const std::string& assemblyPath);

        /**
         * @brief Register a callback to be invoked when hot reload completes.
         * @param callback Function pointer to call with assembly path
         * 
         * The callback receives the assembly path that was reloaded.
         * This is typically called by the editor to re-discover and re-register systems.
         */
        void SetHotReloadCallback(std::function<void(const std::string&)> callback) {
            m_hotReloadCallback = callback;
        }

        // ====================================================================
        // System Discovery and Registration
        // ====================================================================

        /**
         * @brief Discover ISystem implementations in all loaded assemblies.
         * @return Vector of ScriptSystemWrapper instances
         * 
         * Reflects through loaded assemblies to find all types that
         * implement ISystem interface. Creates wrappers for each.
         */
        std::vector<ScriptSystemWrapper*> DiscoverScriptedSystems();

        /**
         * @brief Discover and register scripted systems with SystemManager.
         * @param systemManager The global system manager
         * @param world Optional World to pass to RegisterScriptedSystem for immediate OnCreate
         * @return Number of systems registered
         * 
         * Convenience method that discovers systems and registers them
         * in one call. If world is provided, OnCreate is called immediately.
         * Otherwise, OnCreate must be called separately via CreateAll().
         */
        int RegisterScriptedSystems(SystemManager& systemManager, ECS::World* world = nullptr);

        // ====================================================================
        // Compilation Support (Roslyn integration)
        // ====================================================================

        /**
         * @brief Compile C# scripts in a directory to an assembly using Roslyn.
         * @param scriptDirectory Path to directory containing .cs files
         * @param outputAssembly Path for output assembly (e.g., "GameScripts.dll")
         * @return true if compilation succeeded, false otherwise
         * 
         * Compiles all .cs files in the specified directory using Roslyn.
         * Errors are logged but do not crash the editor.
         * Used by editor on startup and when file watcher detects changes.
         */
        bool CompileScriptsInDirectory(const std::string& scriptDirectory,
                                      const std::string& outputAssembly);

        /**
         * @brief Compile scripts and return diagnostics as a string.
         * @param scriptDirectory Path to directory containing .cs files
         * @param outputAssembly Path for output assembly
         * @param outDiagnostics Reference to store compilation error/warning messages
         * @return true if compilation succeeded
         */
        bool CompileScriptsWithDiagnostics(const std::string& scriptDirectory,
                                          const std::string& outputAssembly,
                                          std::string& outDiagnostics);

        // Start/stop the managed file watcher (used by editor Auto-Reload)
        bool StartScriptWatching(const std::string& directory);
        void StopScriptWatching();

        // Expose compile status (updated by native callback from managed watcher)
        void SetCompileStatus(int status, int progress, const char* message);
        void GetCompileStatus(int& outStatus, int& outProgress, std::string& outMessage);

        // ====================================================================
        // Debugging and Diagnostics
        // ====================================================================

        /**
         * @brief Get list of loaded assembly paths.
         */
        const std::vector<std::string>& GetLoadedAssemblies() const {
            return m_loadedAssemblies;
        }

        /**
         * @brief Get number of registered scripted systems.
         */
        size_t GetScriptedSystemCount() const {
            return m_scriptedSystems.size();
        }

        /**
         * @brief Convert a HRESULT to a managed exception string using managed helper.
         * @param hr HRESULT value
         * @return string with managed exception type and message, empty if helper not available
         */
        std::string GetManagedExceptionString(int hr);

        // Retrieve the last compilation diagnostics as an array of lines.
        void GetLastDiagnosticsLines(std::vector<std::string>& outLines);

        // ====================================================================
        // Internal Access (for ScriptSystemWrapper)
        // ====================================================================

        /**
         * @brief Get delegate function pointers (for ScriptSystemWrapper).
         * @internal Used by ScriptSystemWrapper to call managed functions.
         */
        inline auto GetCallSystemOnCreate() const { return m_callSystemOnCreate; }
        inline auto GetCallSystemOnUpdate() const { return m_callSystemOnUpdate; }
        inline auto GetCallSystemOnDestroy() const { return m_callSystemOnDestroy; }
        inline auto GetGetSystemMetadata() const { return m_getSystemMetadata; }
        inline auto GetGetSystemComponentAccesses() const { return m_getSystemComponentAccesses; }
        inline auto GetDeserializeComponentFromJson() const { return m_deserializeComponentFromJson; }
        inline auto GetSetOutputAssemblyPath() const { return m_setOutputAssemblyPath; }

        // --------------------------------------------------------------------
        // Convenience native wrappers used by engine code
        // --------------------------------------------------------------------
        uint64_t CreateSystemInstanceFromTypeName(const char* typeName);
        void CallSystemOnCreate(uint64_t handle, void* worldPtr);
        intptr_t CallSystemOnUpdateJob(uint64_t handle, void* worldPtr, intptr_t dependsOn);
        void CallSystemOnDestroy(uint64_t handle, void* worldPtr);

    private:
        // ====================================================================
        // CoreCLR Runtime State
        // ====================================================================

        hostfxr_handle  m_hostfxrContext = nullptr;
        void*           m_loadAssemblyAndGetFunctionPtr = nullptr;
        bool            m_initialized = false;

        // hostfxr function pointers (loaded dynamically from nethost.dll)
        hostfxr_initialize_for_runtime_config_fn    m_initFxr = nullptr;
        hostfxr_get_runtime_delegate_fn             m_getRuntimeDelegate = nullptr;
        hostfxr_close_fn                            m_closeFxr = nullptr;

        // ====================================================================
        // Managed Function Delegates
        // ====================================================================

        // Function pointer types for calling into C#
        using LoadAssemblyFn                    = int(*)(const char* assemblyPath);
        using UnloadAssemblyFn                  = int(*)(const char* assemblyPath);
        using DiscoverSystemsFn                 = void*(*)(int* outCount);  // Returns array of system handles
        using CreateSystemWrapperFn             = uint64_t(*)(const char* typeName);
        using DestroySystemWrapperFn            = void(*)(uint64_t handle);
        using GetSystemMetadataFn               = void(*)(uint64_t handle, char* outName, int* outGroup, int* outRunMode);
        using GetSystemComponentAccessesFn      = int(*)(uint64_t handle, uint32_t* outReadHashes, uint32_t* outWriteHashes, int maxSize);
        using GetManagedExceptionForHResultFn   = void*(*)(int hr);
        using ReloadAssemblyFn                  = int(*)(const char* assemblyPath);
        using CompileAndReloadFn                = int(*)(const char* scriptsDir, const char* outputAssemblyPath);
        using GenerateCsProjFn                  = int(*)(const char* scriptsDir, const char* projectName);
        using HashComponentTypeNameFn           = uint32_t(*)(const char* typeName);
        using ResolveSystemGroupFn              = int(*)(int attributeGroup);
        using CallSystemOnCreateFn              = void(*)(uint64_t handle, void* worldPtr);
        using CallSystemOnUpdateFn              = void(*)(uint64_t handle, void* worldPtr);
        using CallSystemOnUpdateJobFn           = intptr_t(*)(uint64_t handle, void* worldPtr, intptr_t dependsOn);
        using CallSystemOnDestroyFn             = void(*)(uint64_t handle, void* worldPtr);
        using CompileDirectoryFn                = int(*)(const char* directoryPath, const char* outputAssemblyPath);
        using CompileDirectoryWithDiagFn        = void*(*)(const char* directoryPath, const char* outputAssemblyPath);
        using GetLastDiagnosticsCountFn         = int(*)();
        using GetLastDiagnosticAtFn             = void*(*)(int index);
        using FreeManagedStringFn               = void(*)(void* ptr);
        using RegisterHotReloadCallbackFn       = void(*)(void* callbackPtr);  // Register C++ callback for reload notifications
        using DeserializeComponentFromJsonFn    = void(*)(uint32_t typeHash, void* componentPtr, int size, const char* jsonStr);  // Deserialize component from JSON
        using SetOutputAssemblyPathFn           = void(*)(const char* outputPath);  // Set the standardized output path for compiled scripts

        // Managed function delegates
        LoadAssemblyFn                      m_loadAssembly = nullptr;
        UnloadAssemblyFn                    m_unloadAssembly = nullptr;
        DiscoverSystemsFn                   m_discoverSystems = nullptr;
        CreateSystemWrapperFn               m_createSystemWrapper = nullptr;
        DestroySystemWrapperFn              m_destroySystemWrapper = nullptr;
        GetSystemMetadataFn                 m_getSystemMetadata = nullptr;
        GetSystemComponentAccessesFn        m_getSystemComponentAccesses = nullptr;
        GetManagedExceptionForHResultFn     m_getExceptionInfoForHR = nullptr;
        CallSystemOnCreateFn                m_callSystemOnCreate = nullptr;
        CallSystemOnUpdateFn                m_callSystemOnUpdate = nullptr;
        CallSystemOnUpdateJobFn             m_callSystemOnUpdateJob = nullptr;
        CallSystemOnDestroyFn               m_callSystemOnDestroy = nullptr;
        CompileDirectoryFn                  m_compileDirectory = nullptr;
        CompileDirectoryWithDiagFn          m_compileDirectoryWithDiag = nullptr;
        GetLastDiagnosticsCountFn           m_getLastDiagnosticsCount = nullptr;
        GetLastDiagnosticAtFn               m_getLastDiagnosticAt = nullptr;
        FreeManagedStringFn                 m_freeManagedString = nullptr;
        ReloadAssemblyFn                    m_reloadAssembly = nullptr;
        CompileAndReloadFn                  m_compileAndReload = nullptr;
        GenerateCsProjFn                    m_generateCsProj = nullptr;
        HashComponentTypeNameFn             m_hashComponentTypeName = nullptr;
        ResolveSystemGroupFn                m_resolveSystemGroup = nullptr;
        RegisterHotReloadCallbackFn         m_registerHotReloadCallback = nullptr;
        DeserializeComponentFromJsonFn      m_deserializeComponentFromJson = nullptr;
        SetOutputAssemblyPathFn             m_setOutputAssemblyPath = nullptr;
        
        // File-watcher managed delegates (Start/Stop) and setter for native compile callback
        using StartWatchingFn                   = int(*)(const char* directoryPath, void* userData);
        using StopWatchingFn                    = void(*)();
        using SetCompileCallbackFn              = void(*)(void* callbackPtr);

        StartWatchingFn                     m_startWatching = nullptr;
        StopWatchingFn                      m_stopWatching = nullptr;
        SetCompileCallbackFn                m_setCompileCallback = nullptr;

        // ====================================================================
        // Internal State
        // ====================================================================

        std::vector<std::string> m_loadedAssemblies;
        std::vector<std::unique_ptr<ScriptSystemWrapper>> m_scriptedSystems;
        std::unordered_map<std::string, std::vector<ScriptSystemWrapper*>> m_systemsByAssembly;

        // Compile status reported by managed watcher via native callback
        int m_compileStatus = 0; // 0 = idle, 1 = compiling, 3 = success, 4 = failure
        int m_compileProgress = -1; // -1 = indeterminate, 0-100 progress
        std::string m_compileMessage;
        std::mutex m_compileMutex;

        // Hot reload callback registered by editor
        std::function<void(const std::string&)> m_hotReloadCallback = nullptr;

        // ====================================================================
        // Helper Methods
        // ====================================================================

        bool InitializeHostFxr();
        bool EnsureRuntimeConfigExists(const char* runtimeConfigPath);
        bool LoadRuntime(const char* runtimeConfigPath);
        bool LoadManagedDelegates();
        void CleanupScriptedSystems();
    };

    // ========================================================================
    // ScriptSystemWrapper
    // ========================================================================

    /**
     * @brief Wraps a C# ISystem implementation.
     * 
     * This class implements the native ISystem interface and forwards
     * all calls to a managed C# system instance.
     * 
     * Each C# system gets wrapped in one of these, which is then
     * registered with the native SystemManager.
     */
    class ScriptSystemWrapper : public ISystem {
    public:
        ScriptSystemWrapper(uint64_t managedHandle,
                           ScriptManager* scriptManager,
                           const std::string& typeName);
        ~ScriptSystemWrapper() override;

        // ISystem interface
        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override;
        SystemRunMode GetRunMode() const override;

        // Access to managed handle (for debugging)
        uint64_t GetManagedHandle() const { return m_managedHandle; }
        const std::string& GetTypeName() const { return m_typeName; }

    private:
        uint64_t m_managedHandle;           // Handle to C# system instance
        ScriptManager* m_scriptManager;     // Parent script manager
        std::string m_typeName;             // C# type name (e.g., "MyGame.PlayerSystem")

        // Cached metadata (initialized on first call to GetMetadata)
        mutable std::optional<SystemMetadata> m_metadata;
        mutable SystemGroup m_group = SystemGroup::Update;
        mutable SystemRunMode m_runMode = SystemRunMode::PlayOnly;

        void CacheMetadata() const;
    };

}

#endif
