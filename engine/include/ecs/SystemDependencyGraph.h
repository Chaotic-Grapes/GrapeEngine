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
     * Usage:
     * @code
     * SystemDependencyGraph graph;
     * 
     * // Add systems to analyze
     * graph.AddSystem(physicsSystem);
     * graph.AddSystem(movementSystem);
     * graph.AddSystem(animationSystem);
     * 
     * // Build the dependency graph
     * graph.Build();
     * 
     * // Check if two systems can run in parallel
     * if (graph.CanRunInParallel(physicsSystem, movementSystem)) {
     *     // Schedule them in parallel
     * }
     * 
     * // Get all systems that must complete before another system
     * auto deps = graph.GetDependencies(animationSystem);
     * for (auto* depSystem : deps) {
     *     // Wait for depSystem to complete
     * }
     * 
     * // Get execution levels for parallel scheduling
     * auto levels = graph.GetExecutionLevels();
     * for (const auto& level : levels) {
     *     // All systems in level can run in parallel
     *     // Wait for all to complete before next level
     * }
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
         * @brief Add a system to the graph.
         * @param system System to add
         * 
         * Systems should be added before calling Build().
         */
        void AddSystem(ISystem* system);

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

    private:
        /// All systems in the graph
        std::vector<ISystem*> m_systems;

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
         * @brief Topological sort using DFS.
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
