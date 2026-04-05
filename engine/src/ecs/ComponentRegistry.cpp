/* Start Header *****************************************************************/
/*!
\file    ComponentRegistry.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of static storage for ComponentRegistry to ensure single instance
across all compilation units. The static maps must be defined in exactly one .cpp
file to prevent ODR violations and ensure all code sees the same registry.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/ComponentRegistry.h"
#include "core/Logger.h"
#include <atomic>
#include <unordered_map>

namespace ECS {

// Define the static member functions that return references to static maps
// These MUST be defined in exactly one .cpp file to avoid ODR (One Definition Rule) violations
// If defined in a header, each .cpp that includes the header gets its own instance!

namespace ComponentRegistryImpl {

    /**
     * @brief Return the singleton component metadata map.
     *        Single instance shared by all code.
     * @return Reference to the global unordered_map keyed by ComponentTypeId.
     */
    static std::unordered_map<ComponentTypeId, ComponentMeta>& GetMetasInstance() {
        static std::unordered_map<ComponentTypeId, ComponentMeta> m;
        return m;
    }

    /**
     * @brief Return the singleton component type ID counter.
     *        Ensures unique IDs across all registrations.
     * @return Reference to the global atomic ComponentTypeId counter.
     */
    static std::atomic<ComponentTypeId>& GetCounterInstance() {
        static std::atomic<ComponentTypeId> counter{0};
        return counter;
    }

    /**
     * @brief Return the singleton hash-to-ComponentTypeId map used for C# interop.
     * @return Reference to the global unordered_map from type hash to ComponentTypeId.
     */
    static std::unordered_map<uint32_t, ComponentTypeId>& GetHashToIdInstance() {
        static std::unordered_map<uint32_t, ComponentTypeId> map;
        return map;
    }

    /**
     * @brief Return the singleton ComponentTypeId-to-hash reverse map.
     * @return Reference to the global unordered_map from ComponentTypeId to type hash.
     */
    static std::unordered_map<ComponentTypeId, uint32_t>& GetIdToHashInstance() {
        static std::unordered_map<ComponentTypeId, uint32_t> map;
        return map;
    }

    /**
     * @brief Return the singleton type-hash-to-name map for native and managed components.
     * @return Reference to the global unordered_map from hash to component name string.
     */
    static std::unordered_map<uint32_t, std::string>& GetHashToNameInstance() {
        static std::unordered_map<uint32_t, std::string> map;
        return map;
    }

    /**
     * @brief Public accessor forwarding to the singleton component metadata map.
     * @return Reference to the global ComponentTypeId-to-ComponentMeta map.
     */
    std::unordered_map<ComponentTypeId, ComponentMeta>& _metas() {
        return GetMetasInstance();
    }

    /**
     * @brief Public accessor forwarding to the singleton ID counter.
     * @return Reference to the global atomic ComponentTypeId counter.
     */
    std::atomic<ComponentTypeId>& _nextIdCounter() {
        return GetCounterInstance();
    }

    /**
     * @brief Public accessor forwarding to the singleton hash-to-ID map.
     * @return Reference to the global uint32_t hash to ComponentTypeId map.
     */
    std::unordered_map<uint32_t, ComponentTypeId>& _hashToId() {
        return GetHashToIdInstance();
    }

    /**
     * @brief Public accessor forwarding to the singleton ID-to-hash map.
     * @return Reference to the global ComponentTypeId to uint32_t hash map.
     */
    std::unordered_map<ComponentTypeId, uint32_t>& _idToHash() {
        return GetIdToHashInstance();
    }

    /**
     * @brief Public accessor forwarding to the singleton hash-to-name map.
     * @return Reference to the global uint32_t hash to component name string map.
     */
    std::unordered_map<uint32_t, std::string>& _hashToName() {
        return GetHashToNameInstance();
    }
}

}  // namespace ECS

// Define the static member functions of ECS::ComponentRegistry so they have
// external linkage and are visible to all translation units. These forward to
// the internal ComponentRegistryImpl instances above.
namespace ECS {

    /**
     * @brief Public static accessor returning the component metadata map.
     * @return Reference to the global ComponentTypeId-to-ComponentMeta map.
     */
    std::unordered_map<ComponentTypeId, ComponentMeta>& ComponentRegistry::_metas() {
        return ComponentRegistryImpl::_metas();
    }

    /**
     * @brief Public static accessor returning the component ID counter.
     * @return Reference to the global atomic ComponentTypeId counter.
     */
    std::atomic<ComponentTypeId>& ComponentRegistry::_nextIdCounter() {
        return ComponentRegistryImpl::_nextIdCounter();
    }

    /**
     * @brief Public static accessor returning the hash-to-ID map.
     * @return Reference to the global uint32_t hash to ComponentTypeId map.
     */
    std::unordered_map<uint32_t, ComponentTypeId>& ComponentRegistry::_hashToId() {
        return ComponentRegistryImpl::_hashToId();
    }

    /**
     * @brief Public static accessor returning the ID-to-hash map.
     * @return Reference to the global ComponentTypeId to uint32_t hash map.
     */
    std::unordered_map<ComponentTypeId, uint32_t>& ComponentRegistry::_idToHash() {
        return ComponentRegistryImpl::_idToHash();
    }

    /**
     * @brief Public static accessor returning the hash-to-name map.
     * @return Reference to the global uint32_t hash to component name string map.
     */
    std::unordered_map<uint32_t, std::string>& ComponentRegistry::_hashToName() {
        return ComponentRegistryImpl::_hashToName();
    }

}
