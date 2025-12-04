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
#include <iostream>
#include <filesystem>
#include <cstring>

// Platform-specific includes for loading nethost
#ifdef _WIN32
    #include <Windows.h>
    #define NETHOST_LIB "nethost.dll"
    #define DOTNET_STRING(s) L##s
#else
    #include <dlfcn.h>
    #define NETHOST_LIB "libnethost.so"
    #define DOTNET_STRING(s) s
#endif

namespace ECS {

    // ============================================================================
    // ScriptManager Implementation
    // ============================================================================

    ScriptManager::ScriptManager() {
        // Constructor
    }

    ScriptManager::~ScriptManager() {
        ShutdownCLR();
    }

    bool ScriptManager::InitializeCLR(const char* runtimeConfigPath) {
        if (m_initialized) {
            std::cerr << "[ScriptManager] Already initialized" << std::endl;
            return true;
        }

        std::cout << "[ScriptManager] Initializing CoreCLR..." << std::endl;

        // Step 1: Load hostfxr functions from nethost.dll
        if (!InitializeHostFxr()) {
            std::cerr << "[ScriptManager] Failed to initialize hostfxr" << std::endl;
            return false;
        }

        // Step 2: Initialize runtime with config file
        if (!LoadRuntime(runtimeConfigPath)) {
            std::cerr << "[ScriptManager] Failed to load .NET runtime" << std::endl;
            return false;
        }

        // Step 3: Load managed delegates for interop
        if (!LoadManagedDelegates()) {
            std::cerr << "[ScriptManager] Failed to load managed delegates" << std::endl;
            return false;
        }

        m_initialized = true;
        std::cout << "[ScriptManager] CoreCLR initialized successfully" << std::endl;
        return true;
    }

    void ScriptManager::ShutdownCLR() {
        if (!m_initialized) return;

        std::cout << "[ScriptManager] Shutting down CoreCLR..." << std::endl;

        // Cleanup scripted systems
        CleanupScriptedSystems();

        // Close hostfxr context
        if (m_closeFxr && m_hostfxrContext) {
            m_closeFxr(m_hostfxrContext);
            m_hostfxrContext = nullptr;
        }

        m_initialized = false;
        std::cout << "[ScriptManager] CoreCLR shut down" << std::endl;
    }

    bool ScriptManager::LoadAssembly(const std::string& assemblyPath) {
        if (!m_initialized) {
            std::cerr << "[ScriptManager] Cannot load assembly - CLR not initialized" << std::endl;
            return false;
        }

        if (!m_loadAssembly) {
            std::cerr << "[ScriptManager] LoadAssembly delegate not available" << std::endl;
            return false;
        }

        std::cout << "[ScriptManager] Loading assembly: " << assemblyPath << std::endl;

        // Call managed LoadAssembly function
        int result = m_loadAssembly(assemblyPath.c_str());
        if (result != 0) {
            std::cerr << "[ScriptManager] Failed to load assembly: " << assemblyPath << std::endl;
            return false;
        }

        m_loadedAssemblies.push_back(assemblyPath);
        std::cout << "[ScriptManager] Assembly loaded successfully" << std::endl;
        return true;
    }

    bool ScriptManager::UnloadAssembly(const std::string& assemblyPath) {
        if (!m_initialized || !m_unloadAssembly) {
            return false;
        }

        // TODO: Implement AssemblyLoadContext unloading
        std::cerr << "[ScriptManager] UnloadAssembly not yet implemented (requires AssemblyLoadContext)" << std::endl;
        return false;
    }

    bool ScriptManager::ReloadAssembly(const std::string& assemblyPath) {
        // TODO: Implement hot reload
        // 1. Save system state
        // 2. Unload old assembly
        // 3. Load new assembly
        // 4. Restore system state
        std::cerr << "[ScriptManager] ReloadAssembly not yet implemented (hot reload)" << std::endl;
        return false;
    }

    std::vector<ScriptSystemWrapper*> ScriptManager::DiscoverScriptedSystems() {
        std::vector<ScriptSystemWrapper*> systems;

        if (!m_initialized || !m_discoverSystems) {
            std::cerr << "[ScriptManager] Cannot discover systems - not initialized" << std::endl;
            return systems;
        }

        std::cout << "[ScriptManager] Discovering scripted systems..." << std::endl;

        // Call managed function to discover all ISystem implementations
        int systemCount = 0;
        void* systemHandlesPtr = m_discoverSystems(&systemCount);

        if (systemCount == 0 || !systemHandlesPtr) {
            std::cout << "[ScriptManager] No scripted systems found" << std::endl;
            return systems;
        }

        // systemHandlesPtr is an array of uint64_t handles
        uint64_t* handles = static_cast<uint64_t*>(systemHandlesPtr);

        for (int i = 0; i < systemCount; ++i) {
            uint64_t handle = handles[i];
            
            // Create wrapper for this C# system
            auto wrapper = std::make_unique<ScriptSystemWrapper>(handle, this, "ScriptedSystem");
            systems.push_back(wrapper.get());
            m_scriptedSystems.push_back(std::move(wrapper));
        }

        std::cout << "[ScriptManager] Discovered " << systemCount << " scripted systems" << std::endl;
        return systems;
    }

    int ScriptManager::RegisterScriptedSystems(SystemManager& systemManager) {
        auto systems = DiscoverScriptedSystems();

        for (auto* system : systems) {
            systemManager.RegisterScriptedSystem(system);
        }

        std::cout << "[ScriptManager] Registered " << systems.size() << " scripted systems with SystemManager" << std::endl;
        return static_cast<int>(systems.size());
    }

    bool ScriptManager::CompileScripts(const std::vector<std::string>& scriptPaths,
                                      const std::string& outputAssembly) {
        // TODO: Implement Roslyn compilation
        std::cerr << "[ScriptManager] CompileScripts not yet implemented (Roslyn integration pending)" << std::endl;
        return false;
    }

    // ============================================================================
    // Private Helper Methods
    // ============================================================================

    bool ScriptManager::InitializeHostFxr() {
        // Platform-specific loading of nethost library
    #ifdef _WIN32
        HMODULE nethostLib = LoadLibraryA(NETHOST_LIB);
        if (!nethostLib) {
            std::cerr << "[ScriptManager] Failed to load " << NETHOST_LIB << std::endl;
            return false;
        }

        // Get function pointers
        auto get_hostfxr_path = (get_hostfxr_path_fn)GetProcAddress(nethostLib, "get_hostfxr_path");
    #else
        void* nethostLib = dlopen(NETHOST_LIB, RTLD_LAZY);
        if (!nethostLib) {
            std::cerr << "[ScriptManager] Failed to load " << NETHOST_LIB << std::endl;
            return false;
        }

        auto get_hostfxr_path = (get_hostfxr_path_fn)dlsym(nethostLib, "get_hostfxr_path");
    #endif

        if (!get_hostfxr_path) {
            std::cerr << "[ScriptManager] Failed to find get_hostfxr_path" << std::endl;
            return false;
        }

        // Get path to hostfxr library
        char_t buffer[512];
        size_t buffer_size = sizeof(buffer) / sizeof(char_t);
        int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);

        if (rc != 0) {
            std::cerr << "[ScriptManager] Failed to get hostfxr path" << std::endl;
            return false;
        }

        // Load hostfxr library
    #ifdef _WIN32
        HMODULE hostfxrLib = LoadLibraryW(buffer);
        if (!hostfxrLib) {
            std::cerr << "[ScriptManager] Failed to load hostfxr library" << std::endl;
            return false;
        }

        m_initFxr = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(hostfxrLib, "hostfxr_initialize_for_runtime_config");
        m_getRuntimeDelegate = (hostfxr_get_runtime_delegate_fn)GetProcAddress(hostfxrLib, "hostfxr_get_runtime_delegate");
        m_closeFxr = (hostfxr_close_fn)GetProcAddress(hostfxrLib, "hostfxr_close");
    #else
        void* hostfxrLib = dlopen(buffer, RTLD_LAZY);
        if (!hostfxrLib) {
            std::cerr << "[ScriptManager] Failed to load hostfxr library" << std::endl;
            return false;
        }

        m_initFxr = (hostfxr_initialize_for_runtime_config_fn)dlsym(hostfxrLib, "hostfxr_initialize_for_runtime_config");
        m_getRuntimeDelegate = (hostfxr_get_runtime_delegate_fn)dlsym(hostfxrLib, "hostfxr_get_runtime_delegate");
        m_closeFxr = (hostfxr_close_fn)dlsym(hostfxrLib, "hostfxr_close");
    #endif

        if (!m_initFxr || !m_getRuntimeDelegate || !m_closeFxr) {
            std::cerr << "[ScriptManager] Failed to load hostfxr functions" << std::endl;
            return false;
        }

        return true;
    }

    bool ScriptManager::LoadRuntime(const char* runtimeConfigPath) {
        if (!m_initFxr) return false;

        // Convert runtime config path to wide string on Windows
    #ifdef _WIN32
        std::wstring configPathW = std::filesystem::path(runtimeConfigPath).wstring();
        const char_t* configPath = configPathW.c_str();
    #else
        const char_t* configPath = runtimeConfigPath;
    #endif

        // Initialize hostfxr context
        int rc = m_initFxr(configPath, nullptr, &m_hostfxrContext);
        if (rc != 0 || !m_hostfxrContext) {
            std::cerr << "[ScriptManager] Failed to initialize runtime config" << std::endl;
            return false;
        }

        // Get load_assembly_and_get_function_pointer delegate
        rc = m_getRuntimeDelegate(
            m_hostfxrContext,
            hdt_load_assembly_and_get_function_pointer,
            &m_loadAssemblyAndGetFunctionPtr
        );

        if (rc != 0 || !m_loadAssemblyAndGetFunctionPtr) {
            std::cerr << "[ScriptManager] Failed to get runtime delegate" << std::endl;
            return false;
        }

        return true;
    }

    bool ScriptManager::LoadManagedDelegates() {
        if (!m_loadAssemblyAndGetFunctionPtr) {
            std::cerr << "[ScriptManager] load_assembly_and_get_function_pointer not available" << std::endl;
            return false;
        }

        std::cout << "[ScriptManager] Loading managed delegates from ScriptHost..." << std::endl;

        // Cast the delegate
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

        // Path to ScriptHost assembly
        std::filesystem::path scriptHostPath = std::filesystem::current_path() / "GrapeEngine.ScriptHost.dll";
        
#ifdef _WIN32
        std::wstring scriptHostPathW = scriptHostPath.wstring();
        const char_t* scriptHostPathCStr = scriptHostPathW.c_str();
#else
        const char_t* scriptHostPathCStr = scriptHostPath.c_str();
#endif

        // Type and method names in C# ScriptHost
        const char_t* typeName = DOTNET_STRING("GrapeEngine.ScriptHost.ScriptHost, GrapeEngine.ScriptHost");

        // Helper lambda to load a function pointer
        auto loadFunction = [&](const char* functionName, void** outDelegate) -> bool {
#ifdef _WIN32
            std::wstring methodNameW = std::wstring(functionName, functionName + strlen(functionName));
            const char_t* methodName = methodNameW.c_str();
#else
            const char_t* methodName = functionName;
#endif

            int rc = loadAssemblyFn(
                scriptHostPathCStr,
                typeName,
                methodName,
                nullptr,  // delegate_type_name (let runtime infer)
                nullptr,  // reserved
                outDelegate
            );

            if (rc != 0 || !*outDelegate) {
                std::cerr << "[ScriptManager] Failed to load function: " << functionName << " (error: " << rc << ")" << std::endl;
                return false;
            }

            std::cout << "[ScriptManager]   Loaded: " << functionName << std::endl;
            return true;
        };

        // Load all managed functions
        bool success = true;
        
        success &= loadFunction("LoadAssembly", reinterpret_cast<void**>(&m_loadAssembly));
        success &= loadFunction("UnloadAssembly", reinterpret_cast<void**>(&m_unloadAssembly));
        success &= loadFunction("DiscoverSystems", reinterpret_cast<void**>(&m_discoverSystems));
        success &= loadFunction("CreateSystemInstance", reinterpret_cast<void**>(&m_createSystemWrapper));
        success &= loadFunction("DestroySystemInstance", reinterpret_cast<void**>(&m_destroySystemWrapper));
        success &= loadFunction("GetSystemMetadata", reinterpret_cast<void**>(&m_getSystemMetadata));
        success &= loadFunction("CallSystemOnCreate", reinterpret_cast<void**>(&m_callSystemOnCreate));
        success &= loadFunction("CallSystemOnUpdate", reinterpret_cast<void**>(&m_callSystemOnUpdate));
        success &= loadFunction("CallSystemOnDestroy", reinterpret_cast<void**>(&m_callSystemOnDestroy));

        if (!success) {
            std::cerr << "[ScriptManager] Failed to load all managed delegates" << std::endl;
            return false;
        }

        std::cout << "[ScriptManager] All managed delegates loaded successfully" << std::endl;
        return true;
    }

    void ScriptManager::CleanupScriptedSystems() {
        m_scriptedSystems.clear();
        m_systemsByAssembly.clear();
        m_loadedAssemblies.clear();
    }

    // ============================================================================
    // ScriptSystemWrapper Implementation
    // ============================================================================

    ScriptSystemWrapper::ScriptSystemWrapper(uint64_t managedHandle,
                                            ScriptManager* scriptManager,
                                            const std::string& typeName)
        : m_managedHandle(managedHandle)
        , m_scriptManager(scriptManager)
        , m_typeName(typeName)
        , m_group(SystemGroup::Update)
        , m_runMode(SystemRunMode::PlayOnly)
    {
    }

    ScriptSystemWrapper::~ScriptSystemWrapper() {
        // Managed system cleanup handled by ScriptManager
    }

    void ScriptSystemWrapper::OnCreate(World& world) {
        if (!m_scriptManager) {
            std::cerr << "[ScriptSystemWrapper] ScriptManager is null" << std::endl;
            return;
        }

        auto callOnCreate = m_scriptManager->GetCallSystemOnCreate();
        if (callOnCreate) {
            callOnCreate(m_managedHandle, &world);
        } else {
            std::cerr << "[ScriptSystemWrapper] CallSystemOnCreate delegate not available" << std::endl;
        }
    }

    void ScriptSystemWrapper::OnUpdate(World& world, float deltaTime) {
        if (!m_scriptManager) return;

        auto callOnUpdate = m_scriptManager->GetCallSystemOnUpdate();
        if (callOnUpdate) {
            callOnUpdate(m_managedHandle, &world, deltaTime);
        }
    }

    void ScriptSystemWrapper::OnDestroy(World& world) {
        if (!m_scriptManager) return;

        auto callOnDestroy = m_scriptManager->GetCallSystemOnDestroy();
        if (callOnDestroy) {
            callOnDestroy(m_managedHandle, &world);
        }
    }

    SystemMetadata ScriptSystemWrapper::GetMetadata() const {
        if (!m_metadataCached) {
            CacheMetadata();
        }
        return m_metadata;
    }

    SystemGroup ScriptSystemWrapper::GetSystemGroup() const {
        if (!m_metadataCached) {
            CacheMetadata();
        }
        return m_group;
    }

    SystemRunMode ScriptSystemWrapper::GetRunMode() const {
        if (!m_metadataCached) {
            CacheMetadata();
        }
        return m_runMode;
    }

    void ScriptSystemWrapper::CacheMetadata() const {
        if (!m_scriptManager) {
            // Fallback to defaults
            m_metadata.name = m_typeName;
            m_metadata.executionOrder = 0;
            m_metadata.enabled = true;
            m_group = SystemGroup::Update;
            m_runMode = SystemRunMode::PlayOnly;
            m_metadataCached = true;
            return;
        }

        auto getMetadata = m_scriptManager->GetGetSystemMetadata();
        if (getMetadata) {
            char nameBuffer[256] = {0};
            int group = 0;
            int runMode = 0;

            getMetadata(m_managedHandle, nameBuffer, &group, &runMode);

            // Set metadata from C# system
            m_metadata.name = nameBuffer[0] != '\0' ? std::string(nameBuffer) : m_typeName;
            m_metadata.executionOrder = 0;  // TODO: Get from C#
            m_metadata.enabled = true;

            m_group = static_cast<SystemGroup>(group);
            m_runMode = static_cast<SystemRunMode>(runMode);
        } else {
            // Fallback to defaults
            m_metadata.name = m_typeName;
            m_metadata.executionOrder = 0;
            m_metadata.enabled = true;
            m_group = SystemGroup::Update;
            m_runMode = SystemRunMode::PlayOnly;
        }
        
        m_metadataCached = true;
    }

}
