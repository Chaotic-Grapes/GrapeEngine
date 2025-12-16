/* Start Header *****************************************************************/
/*!
\file    SystemDependencyGraph.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file defines the SystemDependencyGraph - analyzes system component access
patterns and determines safe parallel execution schedules.

The dependency graph detects read/write conflicts between systems and builds
a directed acyclic graph (DAG) of execution dependencies. Systems with no
conflicting component access can be executed in parallel.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SYSTEM_DEPENDENCY_GRAPH_H
#define SYSTEM_DEPENDENCY_GRAPH_H

#include "Export.h"
#include "ecs/ISystem.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <string>

namespace ECS {

    /**
     * @brief Dependency between two systems in the execution graph.
     * 
     * System A depends on System B if B must complete before A can run safely.
     */
    struct SystemDependency {
        ISystem* DependentSystem;       // System that must wait
        ISystem* DependencySystem;      // System that must complete first
        std::string Reason;             // Why the dependency exists (component name, etc.)
    };

    /**
     * @brief Analyzes component access patterns and determines safe parallel execution.
     * 
     * The SystemDependencyGraph examines system metadata to determine which systems
     * can safely run in parallel. Two systems conflict if:
     * - Both write to the same component, OR
     * - One writes and the other reads the same component
     * 
     * The graph uses topological sorting to find valid execution schedules that
     * maximize parallelism while maintaining data safety.
     * 
     * Supports both direct system pointers (ISystem*) and metadata structures,
     * enabling use by both SystemManager and JobDependencyGraph.
     * 
     * Usage with ISystem*:
     * @code
     * SystemDependencyGraph graph;
     * graph.AddSystem(physicsSystem);
     * graph.AddSystem(movementSystem);
     * graph.Build();
     * auto levels = graph.GetExecutionLevels();
     * @endcode
     * 
     * Usage with metadata (for group-based scheduling):
     * @code
     * SystemDependencyGraph graph;
     * graph.AddSystemMetadata(metadata1);
     * graph.AddSystemMetadata(metadata2);
     * auto stages = graph.BuildExecutionStagesForGroup(SystemGroup::Update);
     * @endcode
     */
    class GRAPEENGINE_API SystemDependencyGraph {
    public:
        SystemDependencyGraph() = default;
        ~SystemDependencyGraph() = default;

        // Disable copying
        SystemDependencyGraph(const SystemDependencyGraph&) = delete;
        SystemDependencyGraph& operator=(const SystemDependencyGraph&) = delete;

        /**
         * @brief Add a system to the graph (direct system pointer).
         * @param system System to add
         * 
         * Systems should be added before calling Build().
         */
        void AddSystem(ISystem* system);

        /**
         * @brief Add a system to the graph using metadata (for group-based scheduling).
         * @param metadata System metadata
         * 
         * Alternative to AddSystem() for use cases that track systems by metadata.
         * Both AddSystem() and AddSystemMetadata() can be used interchangeably.
         */
        void AddSystemMetadata(const SystemMetadata& metadata);

        /**
         * @brief Build the dependency graph.
         * 
         * Analyzes component access patterns of all registered systems
         * and constructs the conflict graph. Must be called after all
         * systems are added and before queries.
         * 
         * Time complexity: O(n^2 * m) where n = systems, m = components per system
         */
        void Build();

        /**
         * @brief Check if two systems can safely run in parallel.
         * @param systemA First system
         * @param systemB Second system
         * @return True if they have no read/write conflicts
         * 
         * Two systems can run in parallel if neither writes to a component
         * that the other reads or writes to.
         */
        bool CanRunInParallel(const ISystem* systemA, const ISystem* systemB) const;

        /**
         * @brief Check if two systems can safely run in parallel (metadata version).
         * @param sys1 First system metadata
         * @param sys2 Second system metadata
         * @return True if they have no read/write conflicts
         */
        bool CanRunInParallel(const SystemMetadata& sys1,
                             const SystemMetadata& sys2) const;

        /**
         * @brief Get all systems that system B depends on.
         * @param system The system to query
         * @return Vector of systems that must complete before the given system
         */
        std::vector<ISystem*> GetDependencies(const ISystem* system) const;

        /**
         * @brief Get all systems that depend on system A.
         * @param system The system to query
         * @return Vector of systems that must wait for the given system
         */
        std::vector<ISystem*> GetDependents(const ISystem* system) const;

        /**
         * @brief Get execution levels for parallel scheduling.
         * 
         * Returns systems organized into levels where:
         * - All systems in a level can run in parallel
         * - All systems in level N must complete before level N+1 starts
         * 
         * This is a topological sort of the dependency graph.
         * 
         * @return Vector of system groups, each group represents one execution level
         */
        std::vector<std::vector<ISystem*>> GetExecutionLevels() const;

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
         * @brief Get all dependencies in the graph.
         * @return Vector of all system dependencies
         */
        const std::vector<SystemDependency>& GetAllDependencies() const;

        /**
         * @brief Check if the graph is valid (no cycles).
         * @return True if graph is acyclic, false if cycles exist
         */
        bool IsValid() const;

        /**
         * @brief Check for circular system dependencies (alias for IsValid).
         * @return true if cycle detected, false if valid DAG
         */
        bool HasCircularDependencies() const { return !IsValid(); }

        /**
         * @brief Get the total number of systems in the graph.
         */
        size_t GetSystemCount() const { return m_systems.size(); }

        /**
         * @brief Clear all systems and dependencies.
         */
        void Clear();

        /**
         * @brief Validate all system metadata for consistency.
         * 
         * Checks that:
         * - No two systems have same execution order in the group
         * - Component declarations are valid
         * - No duplicate component declarations within a system
         * 
         * @return True if all systems are valid, false if errors found
         */
        bool ValidateSystemMetadata() const;

        /**
         * @brief Get validation errors for all systems.
         * 
         * Returns human-readable error messages for any metadata issues.
         * 
         * @return Vector of error messages (empty if valid)
         */
        std::vector<std::string> GetValidationErrors() const;

        /**
         * @brief Detect and report component conflicts between specific systems.
         * 
         * Provides detailed information about why two systems conflict.
         * 
         * @param systemA First system
         * @param systemB Second system
         * @return List of conflicting components with access modes
         */
        std::vector<std::string> GetConflictDetails(const ISystem* systemA, 
                                                   const ISystem* systemB) const;

        /**
         * @brief Get metadata for a system.
         * @param system System pointer
         * @return Pointer to metadata, or nullptr if not found
         */
        const SystemMetadata* GetSystemMetadata(ISystem* system) const;

    private:
        /// All systems in the graph (by pointer)
        std::vector<ISystem*> m_systems;

        /// All system metadata (parallel to m_systems, one entry per system)
        std::vector<SystemMetadata> m_systemMetadata;

        /// Adjacency list: system -> systems it depends on
        std::unordered_map<ISystem*, std::vector<ISystem*>> m_dependencies;

        /// Reverse adjacency: system -> systems that depend on it
        std::unordered_map<ISystem*, std::vector<ISystem*>> m_dependents;

        /// All explicit dependencies for debugging
        std::vector<SystemDependency> m_allDependencies;

        /**
         * @brief Check if component access between systems conflicts.
         * @return True if the systems have a read/write conflict
         */
        bool _hasComponentConflict(const ISystem* systemA, const ISystem* systemB) const;

        /**
         * @brief Perform depth-first search to detect cycles.
         * @return True if cycle is found
         */
        bool _hasCycle() const;

        /**
         * @brief DFS helper for cycle detection.
         */
        bool _dfsHasCycle(ISystem* system, 
                         std::unordered_set<ISystem*>& visited,
                         std::unordered_set<ISystem*>& recStack) const;

        /**
         * @brief Topological sort using Kahn's algorithm.
         */
        std::vector<std::vector<ISystem*>> _topologicalSort() const;

        /**
         * @brief DFS helper for topological sort.
         */
        void _dfsTopo(ISystem* system,
                     std::unordered_set<ISystem*>& visited,
                     std::vector<ISystem*>& stack) const;
    };

}

#endif
