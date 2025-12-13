/* Start Header *****************************************************************/
/*!
\file    SystemDependencyGraph.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of SystemDependencyGraph - tracks system
dependencies based on component read/write patterns for parallel execution.

Similar to JobDependencyGraph but operates at system level, enabling
automatic detection of which systems can run in parallel.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SYSTEM_DEPENDENCY_GRAPH_H
#define SYSTEM_DEPENDENCY_GRAPH_H

#include <vector>
#include <unordered_map>
#include <string>
#include <typeinfo>
#include "ecs/ISystem.h"

namespace ECS {

    /**
     * @brief Metadata about a system for dependency tracking.
     */
    struct SystemDependencyMetadata {
        ISystem* System = nullptr;
        std::string Name;
        std::vector<ComponentTypeId> ReadComponents;
        std::vector<ComponentTypeId> WriteComponents;
        SystemGroup Group = SystemGroup::Update;
        int ExecutionOrder = 0;
    };

    /**
     * @brief Tracks dependencies between systems based on component access patterns.
     * 
     * Analyzes which systems read and write the same components to determine
     * which systems can safely run in parallel and which must execute sequentially.
     * 
     * Example:
     * @code
     * SystemDependencyGraph graph;
     * 
     * // Add systems from all groups
     * for (const auto& metadata : systemMetadatas) {
     *     graph.AddSystem(metadata);
     * }
     * 
     * // Build execution stages (systems that can run in parallel)
     * auto stages = graph.BuildExecutionStagesForGroup(SystemGroup::Update);
     * 
     * // Execute each stage
     * for (const auto& stage : stages) {
     *     // All systems in stage can run in parallel
     *     ExecuteSystemsInParallel(stage, world, deltaTime);
     * }
     * @endcode
     */
    class SystemDependencyGraph {
    public:
        SystemDependencyGraph() = default;
        ~SystemDependencyGraph() = default;

        // Delete copy operations
        SystemDependencyGraph(const SystemDependencyGraph&) = delete;
        SystemDependencyGraph& operator=(const SystemDependencyGraph&) = delete;

        /**
         * @brief Add a system to the dependency graph.
         * @param metadata System metadata including component accesses
         */
        void AddSystem(const SystemDependencyMetadata& metadata);

        /**
         * @brief Clear all systems and dependencies.
         */
        void Clear();

        /**
         * @brief Check if two systems can run in parallel.
         * @param sys1 First system
         * @param sys2 Second system
         * @return true if systems can run simultaneously
         */
        bool CanRunInParallel(const SystemDependencyMetadata& sys1,
                             const SystemDependencyMetadata& sys2) const;

        /**
         * @brief Get systems that must complete before the given system.
         * @param system System to get dependencies for
         * @return Vector of prerequisite systems
         */
        std::vector<ISystem*> GetDependencies(ISystem* system) const;

        /**
         * @brief Build execution stages for a specific system group.
         * 
         * Creates stages where all systems in a stage can run in parallel.
         * Stages must execute sequentially.
         * 
         * @param group System group to build stages for
         * @return Vector of stages, each containing systems that can run in parallel
         */
        std::vector<std::vector<ISystem*>> BuildExecutionStagesForGroup(SystemGroup group) const;

        /**
         * @brief Get number of systems in graph.
         * @return System count
         */
        size_t GetSystemCount() const { return m_systems.size(); }

        /**
         * @brief Check for circular system dependencies.
         * @return true if cycle detected, false if valid DAG
         */
        bool HasCircularDependencies() const;

        /**
         * @brief Get metadata for a system.
         * @param system System pointer
         * @return Pointer to metadata, or nullptr if not found
         */
        const SystemDependencyMetadata* GetSystemMetadata(ISystem* system) const;

    private:
        /**
         * @brief Check if two component accesses conflict.
         * @param writeComps1 Write-accessed components for system 1
         * @param writeComps2 Write-accessed components for system 2
         * @param readComps1 Read-accessed components for system 1
         * @param readComps2 Read-accessed components for system 2
         * @return true if accesses conflict
         */
        bool _componentAccessesConflict(
            const std::vector<ComponentTypeId>& writeComps1,
            const std::vector<ComponentTypeId>& writeComps2,
            const std::vector<ComponentTypeId>& readComps1,
            const std::vector<ComponentTypeId>& readComps2) const;

        /**
         * @brief Perform topological sort for cycle detection.
         * @return true if cycle detected
         */
        bool _detectCycles() const;

        // All systems in graph
        std::vector<SystemDependencyMetadata> m_systems;

        // Dependency edges: from → to (system i must complete before system j)
        std::unordered_map<size_t, std::vector<size_t>> m_dependencies;

        // Reverse dependencies for efficient lookup
        std::unordered_map<size_t, std::vector<size_t>> m_reverseDependencies;

        // Track last system writing to each component
        std::unordered_map<ComponentTypeId, size_t> m_lastWriterIndex;

        // Track all systems reading each component
        std::unordered_map<ComponentTypeId, std::vector<size_t>> m_readerIndices;
    };

}

#endif
