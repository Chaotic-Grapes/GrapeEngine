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

#ifndef SCRIPT_SYSTEM_H
#define SCRIPT_SYSTEM_H

#include "ecs/World.h"
#include "scene/Scene.h"
#include <string>
#include <memory>
#include <unordered_map>

// Forward declarations for CoreCLR types to avoid polluting headers
typedef void* hostfxr_handle;

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
        bool Initialize(const char* runtimeConfigPath);

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
         * @brief System update - calls OnStart for new scripts.
         * Should be called before Update system.
         * @param world The ECS world
         */
        static void OnStart(World& world);

        /**
         * @brief System update - calls OnUpdate on all active scripts.
         * @param world The ECS world
         * @param dt Delta time in seconds
         */
        static void Update(World& world, float dt);

        /**
         * @brief System update - cleanup destroyed entities' scripts.
         * Should be called in cleanup phase.
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
        hostfxr_handle* m_hostfxrContext = nullptr;
        void* m_loadAssemblyAndGetFunctionPtr = nullptr;

        // Function pointer types for C# delegates
        using CreateScriptInstanceFn = uint64_t(*)(const char* typeName, uint64_t entityId);
        using DestroyScriptInstanceFn = void(*)(uint64_t handle);
        using CallStartFn = void(*)(uint64_t handle);
        using CallUpdateFn = void(*)(uint64_t handle, float dt);

        // Managed function delegates
        CreateScriptInstanceFn m_createInstance = nullptr;
        DestroyScriptInstanceFn m_destroyInstance = nullptr;
        CallStartFn m_callStart = nullptr;
        CallUpdateFn m_callUpdate = nullptr;

        bool m_initialized = false;

        // Static instance for C callbacks
        static ScriptSystem* s_instance;

        // Helper methods
        bool InitializeHostFxr();
        bool LoadRuntime(const char* runtimeConfigPath);
        bool LoadManagedDelegates(const char* assemblyPath);

        // Internal cleanup
        void CleanupScript(Components::ScriptInstance& script);
    };

}

#endif
