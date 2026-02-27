/* Start Header *****************************************************************/
/*!
\file    ScriptManager.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of ScriptManager: manages C# scripting through CoreCLR hosting.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "scripting/ScriptManager.h"
#include "core/Application.h"
#include "ecs/ComponentAccessAttribute.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <mutex>
#include <coreclr/coreclr_delegates.h>

// Platform-specific includes for loading nethost
#ifdef _WIN32
    #include <Windows.h>
    #define NETHOST_LIB "nethost.dll"
    #define DOTNET_STRING(s) L##s
#endif

#ifdef ERROR
#undef ERROR
#endif

// typedef for nethost get_hostfxr_path function pointer
// Uses the project's char_t type already employed throughout this file.
using get_hostfxr_path_fn = int(*)(char_t* path, size_t* path_size, void* reserved);

// Forward-declare native callbacks so managed code registration can pass
// their addresses before the function definitions appear later.
extern "C" void __cdecl Native_CompileStatusCallback(int status, int progress, const char* message);
extern "C" void __cdecl Native_HotReloadCallback(const char* assemblyPath);

// Forward-declare ScriptManager (needed for callback wrapper)
namespace ECS { class ScriptManager; }
ECS::ScriptManager* g_scriptManagerInstance = nullptr;

// Wrapper callback that invokes the registered callback in ScriptManager
extern "C" void __cdecl Native_OnScriptsChanged(const char* scriptsDir) {
    if (g_scriptManagerInstance) {
        g_scriptManagerInstance->OnScriptsChanged(scriptsDir);
    }
}

namespace ECS {

    // ============================================================================
    // ScriptManager Implementation
    // ============================================================================

    ScriptManager::ScriptManager() {
        // Set global instance for callback access
        g_scriptManagerInstance = this;
    }

    ScriptManager::~ScriptManager() {
        ShutdownCLR();
    }

    /**
     * @brief Initialize the CoreCLR runtime.
     * @param runtimeConfigPath Path to .NET runtime config (runtimeconfig.json)
     * @return true if initialization succeeded
     * 
     * Must be called before loading any assemblies.
     */
    bool ScriptManager::InitializeCLR(const char* runtimeConfigPath) {
        if (m_initialized) {
            LOG_WARNING("[ScriptManager] Already initialized");
            return true;
        }

        LOG_INFO("[ScriptManager] Initializing CoreCLR...");

        // Step 0: Ensure runtime config file exists
        if (!EnsureRuntimeConfigExists(runtimeConfigPath)) {
            LOG_CRITICAL("[ScriptManager] Failed to ensure runtime config exists");
            return false;
        }

        // Step 1: Load hostfxr functions from nethost.dll
        if (!InitializeHostFxr()) {
            LOG_CRITICAL("[ScriptManager] Failed to initialize hostfxr");
            return false;
        }

        // Step 2: Initialize runtime with config file
        if (!LoadRuntime(runtimeConfigPath)) {
            LOG_CRITICAL("[ScriptManager] Failed to load .NET runtime");
            return false;
        }

        // Step 3: Load managed delegates for interop
        if (!LoadManagedDelegates()) {
            LOG_CRITICAL("[ScriptManager] Failed to load managed delegates");
            return false;
        }

        m_initialized = true;
        LOG_INFO("[ScriptManager] CoreCLR initialized successfully");
        return true;
    }

    /**
     * @brief Shutdown the CoreCLR runtime and cleanup.
     * 
     * Unloads all assemblies, destroys scripted systems, and shuts down CLR.
     */
    void ScriptManager::ShutdownCLR() {
        if (!m_initialized) return;

        LOG_INFO("[ScriptManager] Shutting down CoreCLR...");

        // Cleanup scripted systems
        CleanupScriptedSystems();

        // Close hostfxr context
        if (m_closeFxr && m_hostfxrContext) {
            m_closeFxr(m_hostfxrContext);
            m_hostfxrContext = nullptr;
        }

        m_initialized = false;
        LOG_INFO("[ScriptManager] CoreCLR shut down");
    }

    /**
     * @brief Load a managed assembly into the runtime.
     * @param assemblyPath Path to the managed assembly DLL
     * @return true if loading succeeded
     */
    bool ScriptManager::LoadAssembly(const std::string& assemblyPath) {
        if (!m_initialized) {
            LOG_ERROR("[ScriptManager] Cannot load assembly - CLR not initialized");
            return false;
        }

        if (!m_loadAssembly) {
            LOG_ERROR("[ScriptManager] LoadAssembly delegate not available");
            return false;
        }

        LOG_INFO("[ScriptManager] Loading assembly: " << assemblyPath.c_str());
        // Resolve to absolute path if a relative path was provided
        std::filesystem::path p(assemblyPath);
        if (!p.is_absolute()) {
            p = std::filesystem::absolute(p);
        }
        std::string resolved = p.string();
        LOG_INFO("[ScriptManager] Resolved assembly path: " << resolved.c_str());

        // Call managed LoadAssembly function with absolute path
        int result = m_loadAssembly(resolved.c_str());
        if (result != 0) {
            LOG_ERROR("[ScriptManager] Failed to load assembly: " << assemblyPath.c_str());
            return false;
        }

        // Single assembly model: enforce one active gameplay assembly
        m_loadedAssemblyPath = resolved;
        LOG_INFO("[ScriptManager] Assembly loaded successfully");
        return true;
    }

    /**
     * @brief Unload the currently loaded script assembly.
     * @return true if unloaded successfully
     * 
     * Single-assembly model: unloads the one active gameplay assembly.
     * Do not call directly during hot reload - use HotReloadAssembly() instead.
     */
    bool ScriptManager::UnloadScriptAssembly() {
        if (!m_initialized || !m_unloadAssembly) {
            LOG_ERROR("[ScriptManager] UnloadScriptAssembly not available");
            return false;
        }

        // Call managed UnloadAssembly to destroy AssemblyLoadContext
        // This will release all file locks on the DLL
        int result = m_unloadAssembly(m_loadedAssemblyPath.c_str());
        if (result == 0) {
            LOG_INFO("[ScriptManager] Successfully unloaded assembly: " << m_loadedAssemblyPath);
            m_loadedAssemblyPath.clear();
            return true;
        }
        else {
            LOG_ERROR("[ScriptManager] Failed to unload assembly: " << m_loadedAssemblyPath);
            return false;
        }
    }

    void ScriptManager::SetCurrentWorldForHotReload(World* world) {
        if (!m_initialized || !m_setCurrentWorldForHotReload) {
            LOG_WARNING("[ScriptManager] SetCurrentWorldForHotReload not available");
            return;
        }

        // Call the managed function to set the current world pointer
        // This allows UnloadAssembly to clear components from the active world
        m_setCurrentWorldForHotReload(world);
        LOG_INFO("[ScriptManager] Set current world for hot reload");
    }
    /**
     * @brief Centralized hot reload orchestration controlled by C++.
     * @param scriptDirectory Directory containing .cs files
     * @param assemblyPath Path where assembly will be written
     * @return true if hot reload succeeded
     * 
     * C++ controls the entire flow:
     * 1. Compile scripts (C# writes to temp file)
     * 2. Completely unload the old assembly
     * 3. Force garbage collection to release file locks
     * 4. Move temp DLL -> final DLL
     * 5. Load the new assembly
     * 6. Trigger hot reload callback for system re-discovery
     */
    /**
     * @brief Handle hot reload completion callback from C#.
     * @param assemblyPath Path to the reloaded assembly
     * 
     * Called when C# finishes hot reload. Calls registered callback to allow
     * editor to re-discover and re-register systems.
     */
    void ScriptManager::HandleHotReloadCompletion(const std::string& assemblyPath) {
        LOG_INFO("[ScriptManager] Hot reload completed for: " << assemblyPath);

        if (!m_initialized) {
            LOG_ERROR("[ScriptManager] Cannot handle hot reload - CLR not initialized");
            return;
        }

        // Invoke registered callback if any
        if (m_hotReloadCallback) {
            try {
                m_hotReloadCallback(assemblyPath);
            }
            catch (const std::exception& ex) {
                LOG_ERROR("[ScriptManager] Error in hot reload callback: " << ex.what());
            }
        }
        else {
            LOG_WARNING("[ScriptManager] No hot reload callback registered - systems may not be properly reinitialized");
        }
    }

    /**
     * @brief Discover ISystem implementations in all loaded assemblies.
     * @return Vector of ScriptSystemWrapper instances
     * 
     * Reflects through loaded assemblies to find all types that
     * implement ISystem interface. Creates wrappers for each.
     */
    std::vector<ScriptSystemWrapper*> ScriptManager::DiscoverScriptedSystems() {
        std::vector<ScriptSystemWrapper*> systems;

        if (!m_initialized || !m_discoverSystems) {
            LOG_ERROR("[ScriptManager] Cannot discover systems - not initialized");
            return systems;
        }

        LOG_INFO("[ScriptManager] Discovering scripted systems...");
        // Log native-tracked loaded assembly for diagnostics
        if (!m_loadedAssemblyPath.empty()) {
            LOG_INFO("[ScriptManager] Native loaded assembly: " << m_loadedAssemblyPath);
        } else {
            LOG_INFO("[ScriptManager] No native-loaded assembly recorded");
        }
        // Call managed function to discover all ISystem implementations
        // DiscoverSystems now creates instances and returns INSTANCE handles
        int systemCount = 0;
        void* systemHandlesPtr = m_discoverSystems(&systemCount);

        if (systemCount == 0 || !systemHandlesPtr) {
            LOG_INFO("[ScriptManager] No scripted systems found");
            return systems;
        }

        // systemHandlesPtr is an array of uint64_t instance handles
        uint64_t* handles = static_cast<uint64_t*>(systemHandlesPtr);

        for (int i = 0; i < systemCount; ++i) {
            uint64_t handle = handles[i];
            
            // Create wrapper for this C# system instance
            auto wrapper = std::make_unique<ScriptSystemWrapper>(handle, this, "ScriptedSystem");
            systems.push_back(wrapper.get());
            m_scriptedSystems.push_back(std::move(wrapper));
        }

        LOG_INFO("[ScriptManager] Discovered " << systemCount << " scripted systems");
        return systems;
    }

    // ------------------------------------------------------------------------
    // Native wrapper implementations
    // ------------------------------------------------------------------------

    uint64_t ScriptManager::CreateSystemInstanceFromTypeName(const char* typeName)
    {
        if (!m_initialized || !m_createSystemWrapper || typeName == nullptr) return 0;
        try {
            return m_createSystemWrapper(typeName);
        } catch (...) {
            return 0;
        }
    }

    void ScriptManager::CallSystemOnCreate(uint64_t handle, void* worldPtr)
    {
        if (!m_initialized || !m_callSystemOnCreate) return;
        try {
            m_callSystemOnCreate(handle, worldPtr);
        } catch (...) {
            // swallow exceptions at native boundary
        }
    }

    void ScriptManager::CallSystemOnDestroy(uint64_t handle, void* worldPtr)
    {
        if (!m_initialized || !m_callSystemOnDestroy) return;
        try {
            m_callSystemOnDestroy(handle, worldPtr);
        } catch (...) {}
    }

    void ScriptManager::CallSystemOnSceneStart(uint64_t handle)
    {
        if (!m_initialized || !m_callSystemOnSceneStart) return;
        try {
            m_callSystemOnSceneStart(handle);
        } catch (...) {}
    }

    void ScriptManager::CallSystemOnSceneStop(uint64_t handle)
    {
        if (!m_initialized || !m_callSystemOnSceneStop) return;
        try {
            m_callSystemOnSceneStop(handle);
        } catch (...) {}
    }

    /**
     * @brief Register discovered scripted systems with the SystemManager.
     * @param systemManager Reference to the SystemManager
     * @param world Optional World to pass for immediate OnCreate initialization
     * @return Number of systems registered
     */
    int ScriptManager::RegisterScriptedSystems(SystemManager& systemManager, ECS::World* world) {
        auto systems = DiscoverScriptedSystems();

        for (auto* system : systems) {
            systemManager.RegisterScriptedSystem(system, world);
        }

        LOG_INFO("[ScriptManager] Registered " << static_cast<int>(systems.size()) << " scripted systems with SystemManager");
        return static_cast<int>(systems.size());
    }

    bool ScriptManager::CopyScriptingAssemblyToDirectory(const std::string& targetDir) {
        try {
            // GrapeEngine.Scripting.dll is typically in the current working directory (build output)
            std::filesystem::path sourceDir = std::filesystem::current_path();
            std::filesystem::path scriptingSource = sourceDir / "GrapeEngine.Scripting.dll";
            
            if (!std::filesystem::exists(scriptingSource)) {
                LOG_WARNING("[ScriptManager] Could not find GrapeEngine.Scripting.dll at: " << scriptingSource.string());
                return false;
            }
            
            // Ensure target directory exists
            std::filesystem::path targetPath(targetDir);
            if (!std::filesystem::exists(targetPath)) {
                std::filesystem::create_directories(targetPath);
            }
            
            // Copy the assembly to the target directory
            std::filesystem::path scriptingDest = targetPath / "GrapeEngine.Scripting.dll";
            std::filesystem::copy_file(scriptingSource, scriptingDest, std::filesystem::copy_options::overwrite_existing);
            
            LOG_INFO("[ScriptManager] Copied GrapeEngine.Scripting.dll to: " << scriptingDest.string());
            return true;
        } 
        catch (const std::exception& e) {
            LOG_ERROR("[ScriptManager] Failed to copy GrapeEngine.Scripting.dll: " << e.what());
            return false;
        }
    }

    /**
     * @brief Compile scripts and return diagnostics as a string.
     * @param scriptDirectory Path to directory containing .cs files
     * @param outputAssembly Path for output assembly (used by C# to find temp location)
     * @param outDiagnostics Reference to store compilation error/warning messages
     * @return true if temp assembly was created, false otherwise
     * 
     * Success is determined by existence of temp assembly file, not diagnostics.
     * Diagnostics are informational only and never prevent reload.
     * C++ controls assembly lifecycle (unload, move, load).
     */
    bool ScriptManager::CompileScriptsWithDiagnostics(const std::string& scriptDirectory,
                                                      const std::string& outputAssembly,
                                                      std::string& outDiagnostics) {
        if (!m_initialized) {
            outDiagnostics = "CLR not initialized";
            LOG_ERROR("[ScriptManager] Cannot compile - " << outDiagnostics);
            return false;
        }

        if (!m_compileDirectory) {
            outDiagnostics = "Compile delegate not available";
            LOG_ERROR("[ScriptManager] " << outDiagnostics);
            return false;
        }

        if (scriptDirectory.empty() || outputAssembly.empty()) {
            outDiagnostics = "Invalid directory or output assembly path";
            LOG_ERROR("[ScriptManager] " << outDiagnostics);
            return false;
        }

        LOG_INFO("[ScriptManager] Compiling scripts from: " << scriptDirectory);

        // Retrieve diagnostics (informational only)
        if (m_compileDirectoryWithDiag && m_freeManagedString) {
            void* p = m_compileDirectoryWithDiag(scriptDirectory.c_str(), outputAssembly.c_str());
            
            if (p != nullptr) {
                const char* diagC = static_cast<const char*>(p);
                outDiagnostics = diagC ? std::string(diagC) : std::string("(no diagnostics)");
                m_freeManagedString(p);
                LOG_INFO("[ScriptManager] Compilation diagnostics captured (" 
                        << outDiagnostics.size() << " chars)");
            }
        }

        // Success is determined ONLY by temp assembly existence, not diagnostics
        std::string tempAssemblyPath = outputAssembly;
        if (m_getLastCompiledTempAssemblyPath) {
            void* pathPtr = m_getLastCompiledTempAssemblyPath();
            if (pathPtr != nullptr) {
                const char* tempPath = static_cast<const char*>(pathPtr);
                tempAssemblyPath = tempPath ? std::string(tempPath) : outputAssembly;
                m_freeManagedString(pathPtr);
                LOG_INFO("[ScriptManager] Temporary assembly path: " << tempAssemblyPath);
            }
        }

        if (!std::filesystem::exists(tempAssemblyPath)) {
            LOG_ERROR("[ScriptManager] Compilation failed - no temp assembly at: " << tempAssemblyPath);
            {
                std::lock_guard<std::mutex> lock(m_compileMutex);
                m_compileMessage = outDiagnostics;
            }
            return false;
        }

        LOG_INFO("[ScriptManager] Compilation succeeded - temp assembly created");
        {
            std::lock_guard<std::mutex> lock(m_compileMutex);
            m_compileMessage = "Compilation successful";
        }
        return true;
    }

    /**
     * @brief Start watching a directory for script changes to enable hot reload.
     * @param directory Path to the directory to watch
     * @return true if watching started successfully
     */
    bool ScriptManager::StartScriptWatching(const std::string& directory) {
        if (!m_initialized || !m_startWatching) {
            LOG_ERROR("[ScriptManager] StartScriptWatching not available");
            return false;
        }

        int rc = m_startWatching(directory.c_str(), nullptr);
        if (rc != 0) {
            LOG_ERROR("[ScriptManager] StartWatching returned error: " << rc);
            return false;
        }
        // Register callback for script changes (invokes registered callback in ScriptManager)
        if (m_registerScriptChangedCallback) {
            // Pass pointer to native function that will be called by managed code
            m_registerScriptChangedCallback(reinterpret_cast<void*>(&Native_OnScriptsChanged));
        }
        LOG_INFO("[ScriptManager] Started watching: " << directory.c_str());
        return true;
    }

    /**
     * @brief Stop watching for script changes.
     */
    void ScriptManager::StopScriptWatching() {
        if (!m_initialized || !m_stopWatching) return;
        m_stopWatching();
        LOG_INFO("[ScriptManager] Stopped script watching");
    }

    /**
     * @brief Callback invoked by C# file watcher when scripts change.
     * Forwards to the registered callback if one is set.
     * @param scriptsDir Directory where changes were detected
     */
    void ScriptManager::OnScriptsChanged(const std::string& scriptsDir) {
        if (m_scriptChangedCallback) {
            LOG_INFO("[ScriptManager] Scripts changed, invoking callback for: " << scriptsDir);
            m_scriptChangedCallback(scriptsDir);
        } else {
            LOG_WARNING("[ScriptManager] Scripts changed but no callback registered");
        }
    }

    // ============================================================================
    // Private Helper Methods
    // ============================================================================

    /**
     * @brief Initialize hostfxr functions from nethost library.
     * @return true if successful
     */
    bool ScriptManager::InitializeHostFxr() {
        // Platform-specific loading of nethost library
    #ifdef _WIN32
        HMODULE nethostLib = LoadLibraryA(NETHOST_LIB);
        if (!nethostLib) {
            LOG_ERROR("[ScriptManager] Failed to load " << NETHOST_LIB);
            return false;
        }

        // Get function pointers
        auto get_hostfxr_path = (get_hostfxr_path_fn)GetProcAddress(nethostLib, "get_hostfxr_path");
    #endif

        if (!get_hostfxr_path) {
            LOG_ERROR("[ScriptManager] Failed to find get_hostfxr_path");
            return false;
        }

        // Get path to hostfxr library
        char_t buffer[512];
        size_t buffer_size = sizeof(buffer) / sizeof(char_t);
        int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);

        if (rc != 0) {
            LOG_ERROR("[ScriptManager] Failed to get hostfxr path");
            return false;
        }

        // Load hostfxr library
    #ifdef _WIN32
        HMODULE hostfxrLib = LoadLibraryW(buffer);
        if (!hostfxrLib) {
            LOG_ERROR("[ScriptManager] Failed to load hostfxr library from path");
            return false;
        }

        m_initFxr = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(hostfxrLib, "hostfxr_initialize_for_runtime_config");
        m_getRuntimeDelegate = (hostfxr_get_runtime_delegate_fn)GetProcAddress(hostfxrLib, "hostfxr_get_runtime_delegate");
        m_closeFxr = (hostfxr_close_fn)GetProcAddress(hostfxrLib, "hostfxr_close");
    #endif

        if (!m_initFxr || !m_getRuntimeDelegate || !m_closeFxr) {
            LOG_ERROR("[ScriptManager] Failed to load hostfxr functions");
            return false;
        }

        return true;
    }

    /**
     * @brief Ensure the .NET runtime config file exists, generating it if needed.
     * @param runtimeConfigPath Path to the runtimeconfig.json file
     * @return true if the config exists or was created successfully
     */
    bool ScriptManager::EnsureRuntimeConfigExists(const char* runtimeConfigPath) {
        // Check if the config file already exists
        std::filesystem::path configPath(runtimeConfigPath);
        if (std::filesystem::exists(configPath)) {
            LOG_INFO("[ScriptManager] Runtime config found: " << runtimeConfigPath);
            return true;
        }

        // Create the runtime config JSON at runtime
        LOG_INFO("[ScriptManager] Runtime config not found, generating: " << runtimeConfigPath);
        
        try {
            // Create parent directories if needed
            std::filesystem::create_directories(configPath.parent_path());

            // JSON content for .NET 9.0
            const char* runtimeConfigJson = R"({
                                                "runtimeOptions": {
                                                    "tfm": "net9.0",
                                                    "framework": {
                                                    "name": "Microsoft.NETCore.App",
                                                    "version": "9.0.0"
                                                    },
                                                    "configProperties": {
                                                    "System.GC.Server": true,
                                                    "System.Runtime.TieredCompilation": true
                                                    }
                                                }
                                                })";

            // Write the config file
            std::ofstream configFile(configPath);
            if (!configFile.is_open()) {
                LOG_ERROR("[ScriptManager] Failed to open config file for writing: " << runtimeConfigPath);
                return false;
            }

            configFile << runtimeConfigJson;
            configFile.close();

            LOG_INFO("[ScriptManager] Runtime config generated successfully: " << runtimeConfigPath);
            return true;
        }
        catch (const std::exception& ex) {
            LOG_ERROR("[ScriptManager] Failed to generate runtime config: " << ex.what());
            return false;
        }
    }

    /**
     * @brief Load the .NET runtime using the specified config file.
     * @param runtimeConfigPath Path to the runtimeconfig.json file
     * @return true if successful
     */
    bool ScriptManager::LoadRuntime(const char* runtimeConfigPath) {
        if (!m_initFxr) return false;

        // Convert runtime config path to wide string on Windows
    #ifdef _WIN32
        std::wstring configPathW = std::filesystem::path(runtimeConfigPath).wstring();
        const char_t* configPath = configPathW.c_str();
    #endif

        // Initialize hostfxr context
        int rc = m_initFxr(configPath, nullptr, &m_hostfxrContext);
        if (rc != 0 || !m_hostfxrContext) {
            LOG_ERROR("[ScriptManager] Failed to initialize runtime config");
            return false;
        }

        // Get load_assembly_and_get_function_pointer delegate
        rc = m_getRuntimeDelegate(
            m_hostfxrContext,
            hdt_load_assembly_and_get_function_pointer,
            &m_loadAssemblyAndGetFunctionPtr
        );

        if (rc != 0 || !m_loadAssemblyAndGetFunctionPtr) {
            LOG_ERROR("[ScriptManager] Failed to get runtime delegate");
            return false;
        }

        return true;
    }

    /**
     * @brief Load managed delegates for interop with C# ScriptHost.
     * @return true if all delegates loaded successfully
     */
    bool ScriptManager::LoadManagedDelegates() {
        if (!m_loadAssemblyAndGetFunctionPtr) {
            LOG_ERROR("[ScriptManager] load_assembly_and_get_function_pointer not available");
            return false;
        }

        LOG_INFO("[ScriptManager] Loading managed delegates from ScriptHost...");

        // Cast delegate
        using load_assembly_and_get_function_pointer_fn = int(*)(
            const char_t* assembly_path,
            const char_t* type_name,
            const char_t* method_name,
            const char_t* delegate_type_name,
            void* reserved,
            void** delegate
        );

        auto loadAssemblyFn = reinterpret_cast<load_assembly_and_get_function_pointer_fn>(
            m_loadAssemblyAndGetFunctionPtr
        );

        // Path to ScriptHost assembly (now part of GrapeEngine.Scripting)
        std::filesystem::path scriptHostPath = std::filesystem::current_path() / "GrapeEngine.Scripting.dll";
        
        // Log the path being used for debugging
        LOG_INFO("[ScriptManager] Looking for GrapeEngine.Scripting.dll at: " << scriptHostPath.string() << '\n'
                << "  - Assembly exists: " << std::boolalpha << (std::filesystem::exists(scriptHostPath)) << std::noboolalpha);
        
#ifdef _WIN32 // Windows uses wide strings
        std::wstring scriptHostPathW = scriptHostPath.wstring();
        const char_t* scriptHostPathCStr = scriptHostPathW.c_str();
#else // POSIX
        std::string scriptHostPathStr = scriptHostPath.string();
        const char_t* scriptHostPathCStr = scriptHostPathStr.c_str();
#endif

        // Type and method names in C# ScriptHost/ScriptFileWatcher
        // Use assembly-qualified type names so hostfxr can resolve UnmanagedCallersOnly methods
        const char_t* scriptHostTypeName = DOTNET_STRING("GrapeEngine.Scripting.Internal.Hosting.ScriptHost, GrapeEngine.Scripting");
        const char_t* watcherTypeName    = DOTNET_STRING("GrapeEngine.Scripting.Internal.Hosting.ScriptFileWatcher, GrapeEngine.Scripting");
        std::stringstream sucstream{};
        std::stringstream errstream{};

        // *********************** Helper Lambda to load function pointers *********************** //
        // Helper lambda to load a method from a given managed type and reports a label
        // PS: C# uses the term "method"
        enum class LoadLabel { Function, Watcher }; // Differentiate function vs watcher methods, can add more if needed
        auto loadMethod = [&](const char* functionName, const char_t* typeName, void** outDelegate, LoadLabel label = LoadLabel::Function) -> bool {
#ifdef _WIN32
            int len = static_cast<int>(strlen(functionName) + 1);
            std::wstring methodNameW(functionName, functionName + len - 1);
            const char_t* methodName = methodNameW.c_str();
#else
            const char_t* methodName = functionName;
#endif

            int rc = loadAssemblyFn(
                scriptHostPathCStr,
                typeName,
                methodName,
                UNMANAGEDCALLERSONLY_METHOD,
                nullptr,
                outDelegate
            );

            const char* labelStr = (label == LoadLabel::Watcher) ? "watcher" : "function";

            if (rc != 0 || !*outDelegate) {
                if (errstream.str().empty()) // First error
                    errstream << "[ScriptManager] Failed to load managed delegates:\n";
                else
                    errstream << '\n'; // New line for multiple errors
                
                // Log error
                if (m_getExceptionInfoForHR) { // Try to get managed exception info
                    std::string exStr = GetManagedExceptionString(rc);
                    errstream << "  - " << labelStr << ": " << functionName << " (error: " << rc << ", exception: " << exStr << ")";
                }
                else // No exception info available
                    errstream << "  - " << labelStr << ": " << functionName << " (error: " << rc << ")";
                return false; // failure
            }

            if (sucstream.str().empty()) // First success
                sucstream << "[ScriptManager] Loaded managed delegates:\n";
            else
                sucstream << '\n'; // New line for multiple successes
            sucstream << "  - " << labelStr << ": " << functionName;
            return true;
        };
        // *************************************************************************************** //

        // Load all managed functions
        bool success = true;
        
        // Core ScriptHost methods
        success &= loadMethod("GetManagedExceptionForHResult",    scriptHostTypeName, reinterpret_cast<void**>(&m_getExceptionInfoForHR));
        success &= loadMethod("LoadAssembly",                     scriptHostTypeName, reinterpret_cast<void**>(&m_loadAssembly));
        success &= loadMethod("SetCurrentWorldForHotReload",      scriptHostTypeName, reinterpret_cast<void**>(&m_setCurrentWorldForHotReload));
        success &= loadMethod("UnloadAssembly",                   scriptHostTypeName, reinterpret_cast<void**>(&m_unloadAssembly));
        success &= loadMethod("ReloadAssembly",                   scriptHostTypeName, reinterpret_cast<void**>(&m_reloadAssembly));
        success &= loadMethod("DiscoverSystems",                  scriptHostTypeName, reinterpret_cast<void**>(&m_discoverSystems));
        success &= loadMethod("CreateSystemInstance",             scriptHostTypeName, reinterpret_cast<void**>(&m_createSystemWrapper));
        success &= loadMethod("DestroySystemInstance",            scriptHostTypeName, reinterpret_cast<void**>(&m_destroySystemWrapper));
        success &= loadMethod("GetSystemMetadata",                scriptHostTypeName, reinterpret_cast<void**>(&m_getSystemMetadata));
        success &= loadMethod("GetSystemComponentAccesses",       scriptHostTypeName, reinterpret_cast<void**>(&m_getSystemComponentAccesses));
        success &= loadMethod("HashComponentTypeName",            scriptHostTypeName, reinterpret_cast<void**>(&m_hashComponentTypeName));
        success &= loadMethod("ResolveSystemGroup",               scriptHostTypeName, reinterpret_cast<void**>(&m_resolveSystemGroup));
        success &= loadMethod("CallSystemOnCreate",               scriptHostTypeName, reinterpret_cast<void**>(&m_callSystemOnCreate));
        success &= loadMethod("CallSystemOnUpdate",               scriptHostTypeName, reinterpret_cast<void**>(&m_callSystemOnUpdate));
        success &= loadMethod("CallSystemOnDestroy",              scriptHostTypeName, reinterpret_cast<void**>(&m_callSystemOnDestroy));
        success &= loadMethod("CallSystemOnSceneStart",           scriptHostTypeName, reinterpret_cast<void**>(&m_callSystemOnSceneStart));
        success &= loadMethod("CallSystemOnSceneStop",            scriptHostTypeName, reinterpret_cast<void**>(&m_callSystemOnSceneStop));
        success &= loadMethod("CompileScriptsInDirectory",        scriptHostTypeName, reinterpret_cast<void**>(&m_compileDirectory));
        success &= loadMethod("CompileDirectoryWithDiagnostics",  scriptHostTypeName, reinterpret_cast<void**>(&m_compileDirectoryWithDiag));
        success &= loadMethod("CompileAndReload",                 scriptHostTypeName, reinterpret_cast<void**>(&m_compileAndReload));
        success &= loadMethod("GenerateCsProj",                   scriptHostTypeName, reinterpret_cast<void**>(&m_generateCsProj));
        success &= loadMethod("GetLastDiagnosticsCount",          scriptHostTypeName, reinterpret_cast<void**>(&m_getLastDiagnosticsCount));
        success &= loadMethod("GetLastDiagnosticAt",              scriptHostTypeName, reinterpret_cast<void**>(&m_getLastDiagnosticAt));
        success &= loadMethod("FreeStringFromManaged",            scriptHostTypeName, reinterpret_cast<void**>(&m_freeManagedString));
        success &= loadMethod("GetLastCompiledAssemblyPath",      scriptHostTypeName, reinterpret_cast<void**>(&m_getLastCompiledTempAssemblyPath));
        success &= loadMethod("RegisterHotReloadCallback",        scriptHostTypeName, reinterpret_cast<void**>(&m_registerHotReloadCallback));
        success &= loadMethod("DeserializeComponentFromJson",     scriptHostTypeName, reinterpret_cast<void**>(&m_deserializeComponentFromJson));

        // ScriptFileWatcher methods
        success &= loadMethod("StartWatching",                    watcherTypeName, reinterpret_cast<void**>(&m_startWatching), LoadLabel::Watcher);
        success &= loadMethod("StopWatching",                     watcherTypeName, reinterpret_cast<void**>(&m_stopWatching), LoadLabel::Watcher);
        success &= loadMethod("RegisterScriptChangedCallback",    watcherTypeName, reinterpret_cast<void**>(&m_registerScriptChangedCallback), LoadLabel::Watcher);
        
        // Hot reload support methods
        success &= loadMethod("ClearAllManagedComponentsFromEntities", scriptHostTypeName, reinterpret_cast<void**>(&m_clearAllManagedComponents));
        success &= loadMethod("RegisterRemoveComponentCallback", scriptHostTypeName, reinterpret_cast<void**>(&m_registerRemoveComponentCallback));
        success &= loadMethod("ForceGarbageCollection",          scriptHostTypeName, reinterpret_cast<void**>(&m_forceGarbageCollection));
        
        // Log results
        if (!sucstream.str().empty()) // Log any successes
            LOG_INFO(sucstream.str());
        if (!success) { // Log any errors, then return failure
            LOG_CRITICAL(errstream.str());
            return false;
        }

        // Register the C++ hot reload callback so C# can notify us when reload completes
        if (m_registerHotReloadCallback) {
            m_registerHotReloadCallback(reinterpret_cast<void*>(&Native_HotReloadCallback));
            LOG_INFO("[ScriptManager] Registered hot reload callback with managed code");
        }

        // All delegates loaded successfully, so return true
        LOG_INFO("[ScriptManager] All managed delegates loaded successfully");
        return true;
    }

    /**
     * @brief Convert a HRESULT to a managed exception string using managed helper.
     * @param hr HRESULT value
     * @return string with managed exception type and message, empty if helper not available
     */
    std::string ScriptManager::GetManagedExceptionString(int hr) {
        if (!m_getExceptionInfoForHR || !m_freeManagedString) return std::string();

        void* p = m_getExceptionInfoForHR(hr);
        if (!p) return std::string();

        const char* s = static_cast<const char*>(p);
        std::string result = s ? std::string(s) : std::string();

        // Free the unmanaged string allocated by managed helper
        m_freeManagedString(p);
        return result;
    }

    /**
     * @brief Cleanup all scripted systems and clear internal records.
     */
    void ScriptManager::CleanupScriptedSystems() {
        m_scriptedSystems.clear();
        m_systemsByAssembly.clear();
        m_loadedAssemblyPath.clear();
    }

    /**
     * @brief Force a garbage collection in the managed runtime.
     * Invokes System.GC.Collect and WaitForPendingFinalizers.
     */
    void ScriptManager::ForceGarbageCollection() {
        // Prefer calling the managed helper if available (exposed via ScriptHost)
        if (m_forceGarbageCollection) {
            try {
                m_forceGarbageCollection();
                return;
            }
            catch (...) {
                LOG_WARNING("[ScriptManager] m_forceGarbageCollection threw an exception");
            }
        }

        // Fallback: if no managed helper is available, log a warning.
        LOG_WARNING("[ScriptManager] No managed ForceGarbageCollection available; unable to force GC from native side");
    }

    // ============================================================================
    // ScriptSystemWrapper Implementation
    // ============================================================================

    /**
     * @brief Construct a ScriptSystemWrapper for a managed system instance.
     * @param managedHandle Handle to the managed system instance
     * @param scriptManager Pointer to the owning ScriptManager
     * @param typeName Name of the managed system type
     */
    ScriptSystemWrapper::ScriptSystemWrapper(uint64_t managedHandle,
                                            ScriptManager* scriptManager,
                                            const std::string& typeName)
        : m_managedHandle(managedHandle)
        , m_scriptManager(scriptManager)
        , m_typeName(typeName)
        , m_group(SystemGroup::Update)
        , m_runMode(SystemRunMode::PlayOnly)
    { }

    /**
     * @brief Destructor for ScriptSystemWrapper.
     * 
     * Note: Managed system cleanup is handled by ScriptManager.
     */
    ScriptSystemWrapper::~ScriptSystemWrapper() {
        // Managed system cleanup handled by ScriptManager
    }

    /**
     * @brief Call the OnCreate method of the managed system.
     * @param world Reference to the World instance
     */
    void ScriptSystemWrapper::OnCreate(World& world) {
        if (!m_scriptManager) {
            LOG_ERROR("[ScriptSystemWrapper] ScriptManager is null");
            return;
        }

        LOG_INFO("[ScriptSystemWrapper] OnCreate invoked (handle=0x" << std::hex << m_managedHandle << ")");
        auto callOnCreate = m_scriptManager->GetCallSystemOnCreate();
        if (callOnCreate) {
            callOnCreate(m_managedHandle, &world);
            LOG_INFO("[ScriptSystemWrapper] OnCreate forwarded to managed (handle=0x" << std::hex << m_managedHandle << ")");
        }
        else {
            LOG_ERROR("[ScriptSystemWrapper] CallSystemOnCreate delegate not available");
        }
    }

    /**
     * @brief Call the OnUpdate method of the managed system.
     * @param world Reference to the World instance
     * @param deltaTime Time elapsed since last update
     */
    void ScriptSystemWrapper::OnUpdate(World& world) {
        if (!m_scriptManager) return;

        auto callOnUpdate = m_scriptManager->GetCallSystemOnUpdate();
        if (callOnUpdate) {
            callOnUpdate(m_managedHandle, &world);
        }
    }

    /**
     * @brief Call the OnDestroy method of the managed system.
     * @param world Reference to the World instance
     */
    void ScriptSystemWrapper::OnDestroy(World& world) {
        if (!m_scriptManager) return;

        auto callOnDestroy = m_scriptManager->GetCallSystemOnDestroy();
        if (callOnDestroy) {
            callOnDestroy(m_managedHandle, &world);
        }
    }

    void ScriptSystemWrapper::OnSceneStart() {
        if (!m_scriptManager) return;

        auto callOnSceneStart = m_scriptManager->GetCallSystemOnSceneStart();
        if (callOnSceneStart) {
            callOnSceneStart(m_managedHandle);
        }
    }

    void ScriptSystemWrapper::OnSceneStop() {
        if (!m_scriptManager) return;

        auto callOnSceneStop = m_scriptManager->GetCallSystemOnSceneStop();
        if (callOnSceneStop) {
            callOnSceneStop(m_managedHandle);
        }
    }

    /**
     * @brief Get the metadata of the managed system.
     * @return SystemMetadata structure
     */
    SystemMetadata ScriptSystemWrapper::GetMetadata() const {
        if (!m_metadata) {
            CacheMetadata();
        }
        return *m_metadata;
    }

    /**
     * @brief Get the system group of the managed system.
     * @return SystemGroup enum value
     */
    SystemGroup ScriptSystemWrapper::GetSystemGroup() const {
        if (!m_metadata) {
            CacheMetadata();
        }
        return m_group;
    }

    /**
     * @brief Get the run mode of the managed system.
     * @return SystemRunMode enum value
     */
    SystemRunMode ScriptSystemWrapper::GetRunMode() const {
        if (!m_metadata) {
            CacheMetadata();
        }
        return m_runMode;
    }

    /**
     * @brief Cache the metadata of the managed system by calling into C#.
     * Uses ComponentAccessBuilder to create immutable metadata.
     */
    void ScriptSystemWrapper::CacheMetadata() const {
        std::string systemName = m_typeName;
        SystemGroup group = SystemGroup::Update;
        SystemRunMode runMode = SystemRunMode::PlayOnly;
        int executionOrder = 0;

        if (m_scriptManager) {
            auto getMetadata = m_scriptManager->GetGetSystemMetadata();
            if (getMetadata) {
                char nameBuffer[256] = {0};
                int groupInt = 0;
                int runModeInt = 0;

                getMetadata(m_managedHandle, nameBuffer, &groupInt, &runModeInt, &executionOrder);

                // Extract metadata from C# system
                if (nameBuffer[0] != '\0') {
                    systemName = std::string(nameBuffer);
                }
                group = static_cast<SystemGroup>(groupInt);
                runMode = static_cast<SystemRunMode>(runModeInt);
            }
        }

        // Build immutable metadata using ComponentAccessBuilder
        ComponentAccessBuilder builder(systemName);
        builder.SetExecutionOrder(executionOrder)
            .SetGroup(group)
            .SetRunMode(runMode)
            .SetEnabled(true)
            .SetSystemPtr(const_cast<ScriptSystemWrapper*>(this));

        // Extract component accesses from C# attributes
        if (m_scriptManager) {
            auto getComponentAccesses = m_scriptManager->GetGetSystemComponentAccesses();
            if (getComponentAccesses) {
                // Allocate buffers for component hashes (max 64 of each type)
                std::vector<uint32_t> readHashes(64);
                std::vector<uint32_t> writeHashes(64);

                int result = getComponentAccesses(m_managedHandle, readHashes.data(), writeHashes.data(), 128);
                
                int readCount = result & 0xFFFF;  // Lower 16 bits
                int writeCount = (result >> 16) & 0xFFFF;  // Upper 16 bits

                // Add read components to builder
                if (readCount > 0) {
                    std::vector<ComponentTypeId> readIds;
                    readIds.reserve(static_cast<size_t>(readCount));
                    for (int i = 0; i < readCount && i < static_cast<int>(readHashes.size()); ++i) {
                        const uint32_t hash = readHashes[i];
                        const ComponentTypeId id = ComponentRegistry::GetComponentIdFromHash(hash);
                        if (id == NULL_COMPONENT_ID) {
                            LOG_WARNING("[ScriptSystemWrapper] Unknown read component hash 0x" << std::hex << hash << std::dec
                                << " for system '" << systemName << "'");
                            continue;
                        }
                        readIds.push_back(id);
                    }
                    builder.ReadComponents(readIds);
                }

                // Add write components to builder
                if (writeCount > 0) {
                    std::vector<ComponentTypeId> writeIds;
                    writeIds.reserve(static_cast<size_t>(writeCount));
                    for (int i = 0; i < writeCount && i < static_cast<int>(writeHashes.size()); ++i) {
                        const uint32_t hash = writeHashes[i];
                        const ComponentTypeId id = ComponentRegistry::GetComponentIdFromHash(hash);
                        if (id == NULL_COMPONENT_ID) {
                            LOG_WARNING("[ScriptSystemWrapper] Unknown write component hash 0x" << std::hex << hash << std::dec
                                << " for system '" << systemName << "'");
                            continue;
                        }
                        writeIds.push_back(id);
                    }
                    builder.WriteComponents(writeIds);
                }
            }
        }

        m_metadata = builder.Build();
        m_group = group;
        m_runMode = runMode;
    }

}

void ECS::ScriptManager::GetLastDiagnosticsLines(std::vector<std::string>& outLines) {
    outLines.clear();

    // If managed indexed accessors are available, use them
    if (m_getLastDiagnosticsCount && m_getLastDiagnosticAt && m_freeManagedString) {
        int count = 0;
        try {
            count = m_getLastDiagnosticsCount();
        } catch (...) {
            count = 0;
        }

        for (int i = 0; i < count; ++i) {
            void* p = nullptr;
            try {
                p = m_getLastDiagnosticAt(i);
            } catch (...) {
                p = nullptr;
            }

            if (p != nullptr) {
                const char* s = static_cast<const char*>(p);
                outLines.emplace_back(s ? std::string(s) : std::string());
                m_freeManagedString(p);
            }
        }
        if (!outLines.empty()) return;
    }

    // Fallback: split stored compile message by lines
    std::lock_guard<std::mutex> lock(m_compileMutex);
    if (m_compileMessage.empty()) return;
    std::string temp = m_compileMessage;
    size_t pos = 0;
    while (pos < temp.size()) {
        size_t nl = temp.find_first_of('\n', pos);
        if (nl == std::string::npos) {
            outLines.push_back(temp.substr(pos));
            break;
        }
        outLines.push_back(temp.substr(pos, nl - pos));
        pos = nl + 1;
    }
}

// ============================================================================
// Native callback and compile-status helpers
// ============================================================================

extern "C" void __cdecl Native_CompileStatusCallback(int status, int progress, const char* message)
{
    if (Engine::CORE == nullptr) return;
    ECS::ScriptManager* mgr = Engine::CORE->GetScriptManager();
    if (!mgr) return;
    mgr->SetCompileStatus(status, progress, message ? message : "");
}

/**
 * @brief Native callback invoked when C# hot reload completes.
 * Receives the assembly path that was reloaded.
 */
extern "C" void __cdecl Native_HotReloadCallback(const char* assemblyPath)
{
    if (Engine::CORE == nullptr) {
        return;
    }

    if (assemblyPath == nullptr || assemblyPath[0] == '\0') {
        LOG_WARNING("[Native_HotReloadCallback] Invalid assembly path");
        return;
    }

    LOG_INFO("[Native_HotReloadCallback] Hot reload completed for: " << assemblyPath);

    ECS::ScriptManager* scriptMgr = Engine::CORE->GetScriptManager();
    if (!scriptMgr) {
        LOG_ERROR("[Native_HotReloadCallback] ScriptManager not available");
        return;
    }

    // Notify script manager of hot reload completion
    scriptMgr->HandleHotReloadCompletion(assemblyPath);
}

void ECS::ScriptManager::SetCompileStatus(int status, int progress, const char* message)
{
    std::lock_guard<std::mutex> lock(m_compileMutex);
    m_compileStatus = status;
    m_compileProgress = progress;
    m_compileMessage = message ? message : "";
}

void ECS::ScriptManager::GetCompileStatus(int& outStatus, int& outProgress, std::string& outMessage)
{
    std::lock_guard<std::mutex> lock(m_compileMutex);
    outStatus = m_compileStatus;
    outProgress = m_compileProgress;
    outMessage = m_compileMessage;
}

void ECS::ScriptManager::RemoveComponentFromAllEntitiesByHash(ECS::World& world, uint32_t componentTypeHash)
{
    // Get the component type ID from the hash using ComponentRegistry
    ECS::ComponentTypeId componentId = ECS::ComponentRegistry::GetComponentIdFromHash(componentTypeHash);
    
    if (componentId == ECS::NULL_COMPONENT_ID)
    {
        // This can happen during hot reload if:
        // 1. The component hash was never registered (e.g., new component type)
        // 2. The hash calculation changed between versions
        // 3. ComponentDiscovery returned a type that isn't in the registry yet
        // This is not an error - just skip removal for this component type
        LOG_DEBUG("[ScriptManager] RemoveComponentFromAllEntitiesByHash: Component hash 0x" << std::hex << componentTypeHash << std::dec << " not found in registry (may not have been registered yet)");
        return;
    }
    
    LOG_INFO("[ScriptManager] Removing component ID " << componentId << " (hash: 0x" << std::hex << componentTypeHash << std::dec << ") from all entities");
    
    // Call the World method to remove this component from all entities
    world.RemoveComponentFromAllEntities(componentId);
    
    LOG_INFO("[ScriptManager] Successfully removed component ID " << componentId << " from all entities");
}

