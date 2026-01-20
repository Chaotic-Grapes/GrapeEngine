/* Start Header *****************************************************************/
/*!
\file    ISystemMetadataProvider.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Interface for systems to provide custom metadata at runtime.

This implements the Metadata Priority System pattern ported from C#:
Priority 1: ISystemMetadataProvider interface (runtime override)
Priority 2: Virtual GetSystemGroup() method (compile-time override)
Priority 3: Default behavior (SystemGroup::Update)

This allows the same system class to behave differently per instance while
maintaining type-safe, compile-time verification of metadata.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ISYSTEMMETADATAPROVIDER_H
#define ISYSTEMMETADATAPROVIDER_H

#include "Export.h"
#include "ecs/ISystem.h"

namespace ECS {

    /**
     * @brief Optional interface for systems to provide custom metadata at runtime.
     * 
     * METADATA PRIORITY SYSTEM:
     * When retrieving system metadata, the priority order is:
     * 1. ISystemMetadataProvider interface (highest priority, runtime)
     * 2. ISystem::GetSystemGroup() override (compile-time)
     * 3. Default value SystemGroup::Update (fallback)
     * 
     * This design allows:
     * - Same system class instances to have different execution groups
     * - Runtime configuration without subclassing
     * - Type-safe metadata that's compile-time verified
     * - Sensible defaults when not implemented
     * 
     * USAGE EXAMPLE:
     * @code
     * class ConfigurablePhysicsSystem : public ISystem, public ISystemMetadataProvider {
     * public:
     *     ConfigurablePhysicsSystem(SystemGroup group = SystemGroup::Physics)
     *         : m_systemGroup(group) {}
     *     
     *     // ISystemMetadataProvider - runtime configuration
     *     SystemGroup GetMetadataSystemGroup() const override {
     *         return m_systemGroup;
     *     }
     *     
     *     // ISystem - compile-time default
     *     SystemGroup GetSystemGroup() const override {
     *         // If this system doesn't implement ISystemMetadataProvider,
     *         // this would be used. But since it does, the interface takes priority.
     *         return SystemGroup::Physics;
     *     }
     *     
    *     void OnUpdate(World& world) override {
    *         // Physics logic here
    *     }
     *     
     * private:
     *     SystemGroup m_systemGroup;
     * };
     * 
     * // Usage
     * auto physics1 = std::make_unique<ConfigurablePhysicsSystem>(SystemGroup::Physics);
     * auto physics2 = std::make_unique<ConfigurablePhysicsSystem>(SystemGroup::PostPhysics);
     * // Same class, different execution groups at runtime!
     * @endcode
     * 
     * IMPLEMENTATION NOTES:
     * - Implement this interface ONLY if you need runtime metadata flexibility
     * - For most systems, just override GetSystemGroup() instead (simpler)
     * - The SystemManager checks for this interface at registration time
     * - Metadata is cached in SystemMetadata after registration
     * 
     * @see ISystem::GetSystemGroup()
     * @see SystemManager::RegisterSystem()
     */
    class GRAPEENGINE_API ISystemMetadataProvider {
    public:
        virtual ~ISystemMetadataProvider() = default;

        /**
         * @brief Get the execution group (phase) this system runs in.
         * 
         * This takes priority over ISystem::GetSystemGroup() if implemented.
         * Allows runtime configuration of execution group per instance.
         * 
         * @return SystemGroup enum value indicating when this system executes
         * 
         * @see SystemGroup for available execution phases
         * @see ISystem::GetSystemGroup() for the fallback method
         */
        virtual SystemGroup GetMetadataSystemGroup() const = 0;

        /**
         * @brief Get the system run mode (edit/play/both).
         * 
         * Determines when this system should execute based on editor state.
         * 
         * @return SystemRunMode enum value:
         *         - Always: Runs in both edit and play mode
         *         - PlayOnly: Only runs when scene is playing (default)
         *         - EditOnly: Only runs when editor is paused (editor-only systems)
         * 
         * Default implementation: SystemRunMode::PlayOnly
         */
        virtual SystemRunMode GetMetadataRunMode() const {
            return SystemRunMode::PlayOnly;
        }
    };

    /**
     * @brief Helper function to get system group using priority system.
     * 
     * METADATA PRIORITY RESOLUTION:
     * 1. If system implements ISystemMetadataProvider, use GetMetadataSystemGroup()
     * 2. Else use ISystem::GetSystemGroup()
     * 3. Default to SystemGroup::Update
     * 
     * @param system Pointer to the system instance
     * @return SystemGroup using priority resolution
     * 
     * This function is called by SystemManager during registration to resolve
     * the final execution group for the system.
     * 
     * @see ISystemMetadataProvider
     * @see ISystem::GetSystemGroup()
     */
    inline SystemGroup GetSystemMetadataGroup(ISystem* system) {
        if (!system) {
            return SystemGroup::Update;
        }

        // Priority 1: Check for ISystemMetadataProvider interface
        ISystemMetadataProvider* metadataProvider = 
            dynamic_cast<ISystemMetadataProvider*>(system);
        if (metadataProvider) {
            return metadataProvider->GetMetadataSystemGroup();
        }

        // Priority 2: Use ISystem::GetSystemGroup()
        return system->GetSystemGroup();
    }

    /**
     * @brief Helper function to get system run mode using priority system.
     * 
     * METADATA PRIORITY RESOLUTION:
     * 1. If system implements ISystemMetadataProvider, use GetMetadataRunMode()
     * 2. Else use ISystem::GetRunMode()
     * 3. Default to SystemRunMode::PlayOnly
     * 
     * @param system Pointer to the system instance
     * @return SystemRunMode using priority resolution
     */
    inline SystemRunMode GetSystemMetadataRunMode(ISystem* system) {
        if (!system) {
            return SystemRunMode::PlayOnly;
        }

        // Priority 1: Check for ISystemMetadataProvider interface
        ISystemMetadataProvider* metadataProvider = 
            dynamic_cast<ISystemMetadataProvider*>(system);
        if (metadataProvider) {
            return metadataProvider->GetMetadataRunMode();
        }

        // Priority 2: Use ISystem::GetRunMode()
        return system->GetRunMode();
    }

}

#endif
