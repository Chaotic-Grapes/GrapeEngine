/* Start Header *****************************************************************/
/*!
\file    PrefabManager.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\author  Samantha Leong Sher Yen
\par     s.leong@digipen.edu
\par
\brief
This file contains the implementation of the PrefabManager class, providing
prefab registration, instantiation, and instance tracking functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#include "ecs/PrefabManager.h"
#include "ecs/World.h"
#include <algorithm>
#include <cctype>

namespace ECS {

    PrefabManager::PrefabManager(World* world)
        : m_world(world) { }

    /**
     * @brief Normalize and register a prefab path, assigning it a stable FNV-1a hash.
     * @param path File path of the prefab to register.
     * @return The hash identifier for the registered prefab, or the existing hash if already registered.
     */
    uint32_t PrefabManager::RegisterPrefab(const std::string& path) {
        // Normalize path for consistent hashing
        std::string normalized = NormalizePath(path);

        // Check if already registered
        auto it = m_pathToHash.find(normalized);
        if (it != m_pathToHash.end()) {
            return it->second;
        }

        // Compute hash
        uint32_t hash = ComputeHash(normalized);

        // Store bidirectional mapping
        m_pathToHash[normalized] = hash;
        m_hashToPath[hash] = normalized;

        return hash;
    }

    /**
     * @brief Look up the file path for a registered prefab by its hash.
     * @param hash The hash identifying the prefab.
     * @return The registered path string, or an empty string if not found.
     */
    const std::string& PrefabManager::GetPrefabPath(uint32_t hash) const {
        auto it = m_hashToPath.find(hash);
        if (it != m_hashToPath.end()) {
            return it->second;
        }

        static const std::string emptyString = "";
        return emptyString;
    }

    /**
     * @brief Check whether a prefab hash has been registered.
     * @param hash The hash to query.
     * @return True if the hash is registered, false otherwise.
     */
    bool PrefabManager::IsRegistered(uint32_t hash) const {
        return m_hashToPath.find(hash) != m_hashToPath.end();
    }

    /**
     * @brief Return all tracked entity instances spawned from the given prefab.
     * @param hash The prefab hash to query.
     * @return Vector of Entity objects whose indices are tracked for this prefab.
     */
    std::vector<Entity> PrefabManager::GetInstances(uint32_t hash) const {
        std::vector<Entity> result;

        auto it = m_hashToInstances.find(hash);
        if (it != m_hashToInstances.end()) {
            for (uint32_t entityIndex : it->second) {
                // Only return valid entities (have valid index)
                if (entityIndex != Entity::NPOS32) {
                    Entity e;
                    e.Index = entityIndex;
                    // Generation is not tracked; caller must verify
                    result.push_back(e);
                }
            }
        }

        return result;
    }

    /**
     * @brief Return the prefab hash associated with a given entity instance.
     * @param instance The entity to query.
     * @return The prefab hash, or 0 if the entity is not a tracked prefab instance.
     */
    uint32_t PrefabManager::GetInstancePrefab(Entity instance) const {
        auto it = m_instanceToHash.find(instance.Index);
        if (it != m_instanceToHash.end()) {
            return it->second;
        }
        return 0; // 0 = not a prefab instance
    }

    /**
     * @brief Record an entity as an instance of the specified prefab.
     * @param instance The entity to track.
     * @param hash The prefab hash the entity was spawned from.
     */
    void PrefabManager::TrackInstance(Entity instance, uint32_t hash) {
        if (!IsRegistered(hash)) {
            return; // Hash must be registered first
        }

        // Store instance -> hash mapping
        m_instanceToHash[instance.Index] = hash;

        // Store hash -> instances mapping (for bulk queries)
        m_hashToInstances[hash].push_back(instance.Index);
    }

    /**
     * @brief Remove an entity from instance tracking and clean up reverse-lookup entries.
     * @param instance The entity to stop tracking.
     */
    void PrefabManager::RemoveInstance(Entity instance) {
        auto it = m_instanceToHash.find(instance.Index);
        if (it != m_instanceToHash.end()) {
            uint32_t hash = it->second;

            // Remove from instance tracking
            m_instanceToHash.erase(it);

            // Remove from reverse lookup
            auto hashIt = m_hashToInstances.find(hash);
            if (hashIt != m_hashToInstances.end()) {
                auto& instances = hashIt->second;
                instances.erase(
                    std::remove(instances.begin(), instances.end(), instance.Index),
                    instances.end()
                );

                // Clean up empty list
                if (instances.empty()) {
                    m_hashToInstances.erase(hashIt);
                }
            }
        }
    }

    /**
     * @brief Clear all instance-to-hash and hash-to-instances mappings without removing prefab registrations.
     */
    void PrefabManager::ClearInstanceTracking() {
        m_instanceToHash.clear();
        m_hashToInstances.clear();
    }

    /**
     * @brief Clear all prefab registrations and instance tracking data.
     */
    void PrefabManager::Clear() {
        m_hashToPath.clear();
        m_pathToHash.clear();
        ClearInstanceTracking();
    }

    /**
     * @brief Scan a directory recursively and register every `.prefab` file found.
     * @param prefabDirectory Absolute or relative path to the root prefab directory.
     */
    void PrefabManager::LoadPrefabRegistry(const std::string& prefabDirectory) {
        if (!std::filesystem::exists(prefabDirectory)) {
            return;
        }

        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(prefabDirectory)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();

                    // Only register .prefab files
                    if (filename.length() > 7 &&
                        filename.substr(filename.length() - 7) == ".prefab") {

                        // Store relative path from prefab directory
                        std::string relativePath = std::filesystem::relative(entry.path(), prefabDirectory).string();
                        RegisterPrefab(relativePath);
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error&) {
            // Silently ignore filesystem errors
        }
    }

    /**
     * @brief Compute an FNV-1a 32-bit hash for the given string.
     * @param str The string to hash.
     * @return 32-bit FNV-1a hash value.
     */
    uint32_t PrefabManager::ComputeHash(const std::string& str) {
        uint32_t hash = 2166136261u; // FNV offset basis

        for (unsigned char c : str) {
            hash ^= c;
            hash *= 16777619u; // FNV prime
        }

        return hash;
    }

    /**
     * @brief Normalize a file path to lowercase forward-slash form for consistent hashing.
     * @param path The raw path to normalize.
     * @return Normalized path string.
     */
    std::string PrefabManager::NormalizePath(const std::string& path) {
        std::string normalized = path;

        // Convert backslashes to forward slashes
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        // Convert to lowercase for case-insensitive comparison
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char c) { return std::tolower(c); });

        return normalized;
    }

    /**
     * @brief Return the number of tracked instances for a given prefab hash.
     * @param hash The prefab hash to query.
     * @return Count of currently tracked instances.
     */
    uint32_t PrefabManager::GetInstanceCount(uint32_t hash) const {
        auto it = m_hashToInstances.find(hash);
        if (it != m_hashToInstances.end()) {
            return static_cast<uint32_t>(it->second.size());
        }
        return 0;
    }

    /**
     * @brief Create a new entity in the world, register the prefab path, and track the entity as an instance.
     * @param prefabPath Path to the prefab file.
     * @return The newly created Entity, or NULL_ENTITY if the world is not set.
     */
    Entity PrefabManager::Instantiate(const std::string& prefabPath) {
        if (!m_world) {
            return NULL_ENTITY;
        }

        // Register the prefab path
        uint32_t hash = RegisterPrefab(prefabPath);

        // Create a new entity in the world
        // TODO: Load prefab data from file and apply to entity
        // For now, just create an empty entity
        Entity instance = m_world->Create();

        // Track the instance
        TrackInstance(instance, hash);

        return instance;
    }

    /**
     * @brief Instantiate a prefab and attach the resulting entity as a child of the given parent.
     * @param prefabPath Path to the prefab file.
     * @param parent The parent entity to attach to.
     * @return The newly created child Entity, or NULL_ENTITY if the world is not set.
     */
    Entity PrefabManager::InstantiateAsChild(const std::string& prefabPath, Entity parent) {
        if (!m_world) {
            return NULL_ENTITY;
        }

        // Instantiate the prefab
        Entity instance = Instantiate(prefabPath);

        if (!instance.IsNull() && !parent.IsNull()) {
            // Attach as child
            m_world->Attach(instance, parent);
        }

        return instance;
    }

    /**
     * @brief Update the prefab tracking for an existing entity, re-associating it with the given prefab path.
     * @param instance The entity whose prefab association should be updated.
     * @param prefabPath The new prefab path to associate.
     * @return True on success, false if the world is null or the entity is not alive.
     */
    bool PrefabManager::SynchronizeInstance(Entity instance, const std::string& prefabPath) {
        if (!m_world || !m_world->IsAlive(instance)) {
            return false;
        }

        // Register the prefab path
        uint32_t hash = RegisterPrefab(prefabPath);

        // Update the instance tracking
        uint32_t oldHash = GetInstancePrefab(instance);
        if (oldHash != hash) {
            RemoveInstance(instance);
            TrackInstance(instance, hash);
        }

        // TODO: Load prefab data and sync components to instance

        return true;
    }

    /**
     * @brief Remove an entity from prefab instance tracking, making it independent from its prefab.
     * @param instance The entity to detach.
     * @return True on success, false if the world is null or the entity is not alive.
     */
    bool PrefabManager::DetachInstance(Entity instance) {
        if (!m_world || !m_world->IsAlive(instance)) {
            return false;
        }

        // Remove from tracking
        RemoveInstance(instance);

        // TODO: Remove PrefabInstanceMetadata component if present
        // This will make the entity independent from its prefab

        return true;
    }

}
