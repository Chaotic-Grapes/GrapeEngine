/* Start Header *****************************************************************/
/*!
\file    DependencyTopologicalSort.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Generic topological sorting utility for dependency graphs. Extracted from both
ecs/SystemDependencyGraph and jobs/SystemDependencyGraph implementations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ECS_DEPENDENCY_TOPOLOGICAL_SORT_H
#define ECS_DEPENDENCY_TOPOLOGICAL_SORT_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <functional>

namespace ECS {

    /**
     * @class TopologicalSorter
     * @brief Generic topological sort implementation for dependency graphs.
     * 
     * Provides level-based topological sorting (Kahn's algorithm) for
     * dependency graphs of arbitrary type T. Used by both system-level
     * and job-level dependency graphs.
     * 
     * @tparam T The type of nodes in the dependency graph (e.g., ISystem*)
     */
    template<typename T>
    class TopologicalSorter {
    public:
        /**
         * @brief Performs level-based topological sort.
         * 
         * @param nodes All nodes in the graph
         * @param dependencies Map from each node to its direct dependencies
         * @return Vector of levels, where each level contains nodes with no
         *         unprocessed dependencies (can run in parallel)
         * 
         * Uses Kahn's algorithm with in-degree counting. Each level represents
         * nodes that can execute in parallel without dependency conflicts.
         */
        static std::vector<std::vector<T>> Sort(
            const std::vector<T>& nodes,
            const std::unordered_map<T, std::vector<T>>& dependencies
        );

        /**
         * @brief Performs topological sort, returning flattened result.
         * 
         * @param nodes All nodes in the graph
         * @param dependencies Map from each node to its direct dependencies
         * @return Vector of nodes in topological order (may include multiple
         *         nodes at same level arbitrarily ordered)
         */
        static std::vector<T> SortFlat(
            const std::vector<T>& nodes,
            const std::unordered_map<T, std::vector<T>>& dependencies
        );

        /**
         * @brief Performs topological sort with custom hasher.
         * 
         * For node types that don't have std::hash<T> specialized.
         * 
         * @param nodes All nodes in the graph
         * @param dependencies Map using custom comparator
         * @param hasher Custom hash function
         * @param equals Custom equality function
         * @return Levels of topologically sorted nodes
         */
        template<typename Hash, typename Equals>
        static std::vector<std::vector<T>> SortCustom(
            const std::vector<T>& nodes,
            const std::vector<std::pair<T, std::vector<T>>>& dependencies,
            const Hash& hasher,
            const Equals& equals
        );
    };

    // Template implementation
    template<typename T>
    std::vector<std::vector<T>> TopologicalSorter<T>::Sort(
        const std::vector<T>& nodes,
        const std::unordered_map<T, std::vector<T>>& dependencies)
    {
        std::vector<std::vector<T>> levels;

        if (nodes.empty()) {
            return levels;
        }

        // Compute in-degree for each node
        std::unordered_map<T, size_t> inDegree;
        for (const auto& node : nodes) {
            inDegree[node] = 0;
        }

        // Count incoming edges
        for (const auto& node : nodes) {
            auto it = dependencies.find(node);
            if (it != dependencies.end()) {
                inDegree[node] = it->second.size();
            }
        }

        // Build reverse dependencies (dependents)
        std::unordered_map<T, std::vector<T>> dependents;
        for (const auto& [node, deps] : dependencies) {
            for (const auto& dep : deps) {
                dependents[dep].push_back(node);
            }
        }

        // Process level by level (Kahn's algorithm)
        while (!inDegree.empty()) {
            std::vector<T> currentLevel;

            // Find all nodes with no dependencies
            for (const auto& [node, degree] : inDegree) {
                if (degree == 0) {
                    currentLevel.push_back(node);
                }
            }

            if (currentLevel.empty()) {
                // Cycle detected - this shouldn't happen if graph is valid
                break;
            }

            levels.push_back(currentLevel);

            // Remove processed nodes and reduce in-degrees
            for (const auto& node : currentLevel) {
                inDegree.erase(node);

                // Reduce in-degree for dependent nodes
                auto depIt = dependents.find(node);
                if (depIt != dependents.end()) {
                    for (const auto& dependent : depIt->second) {
                        auto degIt = inDegree.find(dependent);
                        if (degIt != inDegree.end()) {
                            degIt->second--;
                        }
                    }
                }
            }
        }

        return levels;
    }

    template<typename T>
    std::vector<T> TopologicalSorter<T>::SortFlat(
        const std::vector<T>& nodes,
        const std::unordered_map<T, std::vector<T>>& dependencies)
    {
        auto levels = Sort(nodes, dependencies);
        std::vector<T> result;

        for (const auto& level : levels) {
            for (const auto& node : level) {
                result.push_back(node);
            }
        }

        return result;
    }

}

#endif
