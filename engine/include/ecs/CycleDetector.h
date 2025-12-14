/* Start Header *****************************************************************/
/*!
\file    CycleDetector.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Generic cycle detection for dependency graphs using depth-first search.
Extracted from both ecs/SystemDependencyGraph and jobs/SystemDependencyGraph
implementations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ECS_CYCLE_DETECTOR_H
#define ECS_CYCLE_DETECTOR_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace ECS {

    /**
     * @class CycleDetector
     * @brief Generic cycle detection for directed dependency graphs.
     * 
     * Detects cycles in dependency graphs using depth-first search with
     * vertex coloring (white/gray/black). Used by both system-level and
     * job-level dependency graphs.
     * 
     * @tparam T The type of nodes in the graph (e.g., ISystem*, size_t)
     */
    template<typename T>
    class CycleDetector {
    public:
        /**
         * @brief Detects if the graph contains any cycles.
         * 
         * @param nodes All nodes in the graph
         * @param dependencies Map from each node to its direct dependencies
         * @return true if a cycle exists, false otherwise
         * 
         * Uses DFS with three colors:
         * - WHITE (0): Unvisited
         * - GRAY (1): Currently visiting (in recursion stack)
         * - BLACK (2): Finished processing
         * 
         * A back edge (to a GRAY node) indicates a cycle.
         */
        static bool HasCycle(
            const std::vector<T>& nodes,
            const std::unordered_map<T, std::vector<T>>& dependencies
        );

        /**
         * @brief Finds a cycle if one exists.
         * 
         * @param nodes All nodes in the graph
         * @param dependencies Map from each node to its direct dependencies
         * @param outCycle Output: vector containing the cycle (empty if none found)
         * @return true if a cycle exists, false otherwise
         * 
         * The outCycle vector will contain the nodes forming the cycle,
         * with the cycle repeating at the end (e.g., [A, B, C, A] for A->B->C->A).
         */
        static bool FindCycle(
            const std::vector<T>& nodes,
            const std::unordered_map<T, std::vector<T>>& dependencies,
            std::vector<T>& outCycle
        );

        /**
         * @brief Detects cycles with custom hash and equality.
         * 
         * For node types that don't have std::hash<T> specialized.
         * 
         * @param nodes All nodes in the graph
         * @param dependencies Dependency edges
         * @param hasher Custom hash function
         * @param equals Custom equality function
         * @return true if a cycle exists, false otherwise
         */
        template<typename Hash, typename Equals>
        static bool HasCycleCustom(
            const std::vector<T>& nodes,
            const std::vector<std::pair<T, std::vector<T>>>& dependencies,
            const Hash& hasher,
            const Equals& equals
        );
    };

    // Template implementation
    template<typename T>
    bool CycleDetector<T>::HasCycle(
        const std::vector<T>& nodes,
        const std::unordered_map<T, std::vector<T>>& dependencies)
    {
        enum Color { WHITE = 0, GRAY = 1, BLACK = 2 };
        std::unordered_map<T, int> color;

        // Initialize all nodes as WHITE
        for (const auto& node : nodes) {
            color[node] = WHITE;
        }

        // DFS-based cycle detection
        std::function<bool(const T&)> hasCycleDFS = [&](const T& node) -> bool {
            color[node] = GRAY;

            // Visit all dependencies of this node
            auto it = dependencies.find(node);
            if (it != dependencies.end()) {
                for (const auto& dep : it->second) {
                    if (color[dep] == GRAY) {
                        // Back edge found - cycle detected
                        return true;
                    }
                    if (color[dep] == WHITE && hasCycleDFS(dep)) {
                        return true;
                    }
                }
            }

            color[node] = BLACK;
            return false;
        };

        // Check each unvisited node
        for (const auto& node : nodes) {
            if (color[node] == WHITE) {
                if (hasCycleDFS(node)) {
                    return true;
                }
            }
        }

        return false;
    }

    template<typename T>
    bool CycleDetector<T>::FindCycle(
        const std::vector<T>& nodes,
        const std::unordered_map<T, std::vector<T>>& dependencies,
        std::vector<T>& outCycle)
    {
        outCycle.clear();

        enum Color { WHITE = 0, GRAY = 1, BLACK = 2 };
        std::unordered_map<T, int> color;
        std::unordered_map<T, T> parent;

        // Initialize all nodes as WHITE
        for (const auto& node : nodes) {
            color[node] = WHITE;
        }

        // DFS-based cycle detection and tracking
        std::function<bool(const T&)> hasCycleDFS = [&](const T& node) -> bool {
            color[node] = GRAY;

            // Visit all dependencies of this node
            auto it = dependencies.find(node);
            if (it != dependencies.end()) {
                for (const auto& dep : it->second) {
                    if (color[dep] == GRAY) {
                        // Back edge found - construct the cycle
                        outCycle.clear();
                        T current = node;
                        while (current != dep) {
                            outCycle.push_back(current);
                            auto pIt = parent.find(current);
                            if (pIt == parent.end()) break;
                            current = pIt->second;
                        }
                        outCycle.push_back(dep);
                        outCycle.push_back(node);  // Close the cycle
                        return true;
                    }
                    if (color[dep] == WHITE) {
                        parent[dep] = node;
                        if (hasCycleDFS(dep)) {
                            return true;
                        }
                    }
                }
            }

            color[node] = BLACK;
            return false;
        };

        // Check each unvisited node
        for (const auto& node : nodes) {
            if (color[node] == WHITE) {
                if (hasCycleDFS(node)) {
                    return true;
                }
            }
        }

        return false;
    }

}  // namespace ECS

#endif  // ECS_CYCLE_DETECTOR_H
