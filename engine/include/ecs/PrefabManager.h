/* Start Header *****************************************************************/
/*!
\file    PrefabManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\author  Samantha Leong Sher Yen
\par     s.leong@digipen.edu
\par     
\brief
This file contains the declaration of the PrefabManager class, responsible for
managing prefab registration, instantiation, and instance tracking. It provides
a hash-based registry for mapping prefab paths to deterministic hashes (FNV-1a),
enabling efficient lookup and batch operations on prefab instances.

The PrefabManager works in conjunction with PrefabInstanceMetadata components
to track prefab associations at runtime. Metadata is reconstructed during scene
loading and is NOT persisted to disk.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef PREFAB_MANAGER_H
#define PREFAB_MANAGER_H

#include "ecs/Entity.h"
#include "ecs/World.h"
#include <unordered_map>
#include <string>
#include <cstdint>
#include <vector>
#include <filesystem>
#include "Export.h"

namespace ECS {

    /**
     * @brief Manages prefab registration, instantiation, and instance tracking.
     */
    class GRAPEENGINE_API PrefabManager {
    public:
        /**
         * @brief Construct a new Prefab Manager object
         * @param world Pointer to the ECS World (can be set later)
         */
        explicit PrefabManager(World* world = nullptr);
        
        // Destructor
        ~PrefabManager() = default;

        /**
         * @brief Register a prefab path and return its deterministic FNV-1a hash.
         * If already registered, returns existing hash.
         * @param path The prefab file path
         * @return uint32_t The FNV-1a hash of the prefab path
         */
        uint32_t RegisterPrefab(const std::string& path);

        /**
         * @brief Get the prefab path associated with a given hash.
         * @param hash The FNV-1a hash of the prefab
         * @return const std::string& Reference to the prefab file path owned by the manager,
         *         or an empty string reference if not found. Returned reference is stable
         *         while the manager exists.
         */
        const std::string& GetPrefabPath(uint32_t hash) const;

        /**
         * @brief Check if a prefab hash is registered.
         * @param hash The FNV-1a hash of the prefab
         * @return true if registered, false otherwise
         */
        bool IsRegistered(uint32_t hash) const;

        /**
         * @brief Get all entity instances that reference a specific prefab.
         * @param hash The FNV-1a hash of the prefab
         * @return std::vector<Entity> List of entity instances of the prefab
         */
        std::vector<Entity> GetInstances(uint32_t hash) const;

        /**
         * @brief Get the prefab hash associated with an entity instance.
         * @param instance The entity instance
         * @return uint32_t The FNV-1a hash of the prefab, or 0 if not a prefab instance
         */
        uint32_t GetInstancePrefab(Entity instance) const;

        /**
         * @brief Track an entity instance as belonging to a specific prefab.
         * @param instance The entity instance
         * @param hash The FNV-1a hash of the prefab
         */
        void TrackInstance(Entity instance, uint32_t hash);

        /**
         * @brief Remove tracking for an entity instance.
         * @param instance The entity instance to untrack
         */
        void RemoveInstance(Entity instance);

        /**
         * @brief Untrack an entity instance (alias for RemoveInstance for C# interop).
         * @param instance The entity instance to untrack
         */
        void UntrackInstance(Entity instance) { RemoveInstance(instance); }

        /**
         * @brief Get the number of instances for a prefab.
         * @param hash The FNV-1a hash of the prefab
         * @return uint32_t The number of instances
         */
        uint32_t GetInstanceCount(uint32_t hash) const;

        /**
         * @brief Instantiate a prefab into the world.
         * @param prefabPath The path to the prefab file
         * @return Entity The instantiated entity, or NULL_ENTITY on failure
         */
        Entity Instantiate(const std::string& prefabPath);

        /**
         * @brief Instantiate a prefab as a child of another entity.
         * @param prefabPath The path to the prefab file
         * @param parent The parent entity
         * @return Entity The instantiated entity, or NULL_ENTITY on failure
         */
        Entity InstantiateAsChild(const std::string& prefabPath, Entity parent);

        /**
         * @brief Synchronize an entity with its prefab definition.
         * @param instance The entity instance to synchronize
         * @param prefabPath The path to the prefab asset
         * @return bool True on success, false on failure
         */
        bool SynchronizeInstance(Entity instance, const std::string& prefabPath);

        /**
         * @brief Detach an entity from its prefab (make it independent).
         * @param instance The entity instance to detach
         * @return bool True on success, false on failure
         */
        bool DetachInstance(Entity instance);

        /**
         * @brief Set the world reference for the PrefabManager.
         * @param world Pointer to the ECS World
         */
        void SetWorld(World* world) { m_world = world; }

        /**
         * @brief Get the current world reference.
         * @return World* Pointer to the ECS World
         */
        World* GetWorld() const { return m_world; }

        /**
         * @brief Clear all prefab registrations and instance tracking.
         */
        void Clear();

        /**
         * @brief Load all prefabs from a specified directory.
         * Recursively scans the directory for .prefab files and registers them.
         * @param prefabDirectory The directory to scan for prefabs
         */
        void LoadPrefabRegistry(const std::string& prefabDirectory);

        /**
         * @brief Compute the FNV-1a hash of a given string.
         * @param str The input string
         * @return uint32_t The computed FNV-1a hash
         */
        static uint32_t ComputeHash(const std::string& str);

        /**
         * @brief Normalize a prefab path for consistent hashing.
         * Converts to lowercase and replaces backslashes with forward slashes.
         * @param path The input prefab path
         * @return std::string The normalized prefab path
         */
        static std::string NormalizePath(const std::string& path);

    private:
        // World reference for operations like instantiation
        World* m_world;

        // Bidirectional mapping: hash <-> normalized prefab path
        std::unordered_map<uint32_t, std::string> m_hashToPath;
        std::unordered_map<std::string, uint32_t> m_pathToHash;

        // Instance tracking: EntityId.Index -> prefab hash
        std::unordered_map<uint32_t, uint32_t> m_instanceToHash;

        // Reverse lookup: prefab hash -> list of entity indices
        std::unordered_map<uint32_t, std::vector<uint32_t>> m_hashToInstances;
    };

}

#endif
