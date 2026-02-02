/* Start Header *****************************************************************/
/*!
\file    SystemDependencyGraph.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of SystemDependencyGraph that analyzes system dependencies and
determines safe parallel execution schedules. It detects access conflicts
between systems based on their component access patterns and builds a directed
acyclic graph (DAG) representing execution order constraints.

This was implemented to prepare for job scheduling of ECS systems but due to time 
constraints, the actual job scheduling is not yet implemented and might not be 
done in the near future.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/SystemDependencyGraph.h"
#include "ecs/ComponentAccessAttribute.h"
#include "ecs/ComponentAccessAttribute.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace ECS {
    namespace {
        /**
         * @brief Check if two systems have any access conflicts.
         * @param accessesA Component accesses of system A
         * @param accessesB Component accesses of system B
         * @return True if there is a conflict, false otherwise
         */
        bool HasAccessConflict(const std::vector<ComponentAccess>& accessesA,
                               const std::vector<ComponentAccess>& accessesB) {
            // Check each component access pair for conflicts
            for (const auto& accessA : accessesA) {
                for (const auto& accessB : accessesB) {
                    // Skip if different components
                    if (accessA.ComponentId != accessB.ComponentId) {
                        continue;
                    }

                    // Check if access modes conflict
                    if (ComponentAccessHelper::AccessesConflict(accessA.Mode, accessB.Mode)) {
                        return true;
                    }
                }
            }

            return false;
        }

        /**
         * @brief Check if two systems have any access conflicts.
         * @param metaA Metadata of system A
         * @param metaB Metadata of system B
         * @return True if there is a conflict, false otherwise
         */
        bool HasConflict(const SystemMetadata& metaA, const SystemMetadata& metaB) {
            // First try detailed access checks if available
            const auto& accessesA = metaA.GetComponentAccesses();
            const auto& accessesB = metaB.GetComponentAccesses();

            // Check detailed accesses if both systems provide them
            if (!accessesA.empty() && !accessesB.empty()) {
                return HasAccessConflict(accessesA, accessesB);
            }

            // Fallback to read/write component lists
            const auto writeA = metaA.GetWriteComponents();
            const auto writeB = metaB.GetWriteComponents();
            const auto readA = metaA.GetReadComponents();
            const auto readB = metaB.GetReadComponents();

            // Check write-write and write-read conflicts
            for (const auto& compA : writeA) {
                for (const auto& compB : writeB) {
                    if (compA == compB) {
                        return true; // Write-write conflict
                    }
                }
            }

            // Check write-read conflicts
            for (const auto& compA : writeA) {
                for (const auto& compB : readB) {
                    if (compA == compB) {
                        return true; // Write-read conflict
                    }
                }
            }

            for (const auto& compA : writeB) {
                for (const auto& compB : readA) {
                    if (compA == compB) {
                        return true; // Write-read conflict
                    }
                }
            }

            return false;
        }

        /**
         * @brief Get detailed conflict descriptions between two systems.
         * @param metaA Metadata of system A
         * @param metaB Metadata of system B
         * @return Vector of conflict description strings
         */
        std::vector<std::string> GetConflictDetailsForMetadata(const SystemMetadata& metaA,
                                                               const SystemMetadata& metaB) {
            std::vector<std::string> conflicts;

            // First try detailed access checks if available
            const auto& accessesA = metaA.GetComponentAccesses();
            const auto& accessesB = metaB.GetComponentAccesses();

            // Check detailed accesses if both systems provide them
            if (!accessesA.empty() && !accessesB.empty()) {
                for (const auto& accessA : accessesA) {
                    for (const auto& accessB : accessesB) {
                        // Skip if different components
                        if (accessA.ComponentId != accessB.ComponentId) {
                            continue;
                        }

                        // Check if access modes conflict
                        if (!ComponentAccessHelper::AccessesConflict(accessA.Mode, accessB.Mode)) {
                            continue;
                        }

                        // Record conflict detail
                        std::stringstream ss;
                        ss << metaA.GetName() << " " << ComponentAccessHelper::ModeToString(accessA.Mode)
                           << " component 0x" << std::hex << std::setfill('0') << std::setw(8)
                           << accessA.ComponentId << " conflicts with " << metaB.GetName()
                           << " " << ComponentAccessHelper::ModeToString(accessB.Mode) << " component 0x"
                           << std::hex << std::setfill('0') << std::setw(8) << accessB.ComponentId;
                        conflicts.push_back(ss.str());
                    }
                }

                return conflicts;
            }

            // Fallback to read/write component lists
            auto writeA = metaA.GetWriteComponents();
            auto writeB = metaB.GetWriteComponents();
            auto readA = metaA.GetReadComponents();
            auto readB = metaB.GetReadComponents();

            // Check write-write conflicts
            for (const auto& compA : writeA) {
                for (const auto& compB : writeB) {
                    // Write-write conflict
                    if (compA == compB) {
                        // Record conflict detail
                        std::stringstream ss;
                        ss << "Both " << metaA.GetName() << " and " << metaB.GetName()
                           << " write to component 0x" << std::hex << std::setfill('0')
                           << std::setw(8) << compA;
                        conflicts.push_back(ss.str());
                    }
                }
            }

            // Check write-read conflicts
            for (const auto& compA : writeA) {
                for (const auto& compB : readB) {
                    if (compA == compB) {
                        // Record conflict detail
                        std::stringstream ss;
                        ss << metaA.GetName() << " writes to component (0x" << std::hex
                           << std::setfill('0') << std::setw(8) << compA
                           << ") that " << metaB.GetName() << " reads";
                        conflicts.push_back(ss.str());
                    }
                }
            }

            // Check read-write conflicts
            for (const auto& compA : readA) {
                for (const auto& compB : writeB) {
                    if (compA == compB) {
                        std::stringstream ss;
                        ss << metaA.GetName() << " reads component (0x" << std::hex
                           << std::setfill('0') << std::setw(8) << compA
                           << ") that " << metaB.GetName() << " writes";
                        conflicts.push_back(ss.str());
                    }
                }
            }

            return conflicts;
        }

        /**
         * @brief Check if two systems have any component access conflicts.
         * @param systemA Pointer to system A
         * @param systemB Pointer to system B
         * @return True if there is a conflict, false otherwise
         */
        bool HasCycle(const std::vector<ISystem*>& nodes,
                      const std::unordered_map<ISystem*, std::vector<ISystem*>>& dependencies) {
            // Depth-First Search (DFS) based cycle detection
            enum Color { WHITE = 0, GRAY = 1, BLACK = 2 }; // unvisited, visiting, visited
            std::unordered_map<ISystem*, int> color; // node colors

            // Initialize all nodes to WHITE
            for (auto* node : nodes) {
                color[node] = WHITE;
            }

            // DFS function
            std::function<bool(ISystem*)> hasCycleDFS = [&](ISystem* node) -> bool {
                color[node] = GRAY;

                // Visit all dependencies
                auto it = dependencies.find(node);
                if (it != dependencies.end()) {
                    // Check each dependent node
                    for (auto* dep : it->second) {
                        // If back edge found
                        if (color[dep] == GRAY) {
                            return true;
                        }
                        // Recur for unvisited nodes
                        if (color[dep] == WHITE && hasCycleDFS(dep)) {
                            return true;
                        }
                    }
                }

                // Mark node as fully visited
                color[node] = BLACK;
                return false;
            };

            // Check each node
            for (auto* node : nodes) {
                if (color[node] == WHITE) {
                    // Start DFS from this node
                    if (hasCycleDFS(node)) {
                        return true;
                    }
                }
            }

            return false;
        }

        /**
         * @brief Perform a topological sort on the dependency graph.
         * @param nodes List of system nodes
         * @param dependencies Map of system dependencies
         * @return Vector of levels, each containing systems that can run in parallel
         */
        std::vector<std::vector<ISystem*>> TopologicalSort(
            const std::vector<ISystem*>& nodes,
            const std::unordered_map<ISystem*, std::vector<ISystem*>>& dependencies) {
            std::vector<std::vector<ISystem*>> levels;

            // Kahn's algorithm for topological sorting
            if (nodes.empty()) {
                return levels;
            }

            // Calculate in-degrees
            std::unordered_map<ISystem*, size_t> inDegree;
            for (auto* node : nodes) {
                inDegree[node] = 0;
            }

            // Calculate in-degrees based on dependencies
            for (auto* node : nodes) {
                auto it = dependencies.find(node);
                if (it != dependencies.end()) {
                    inDegree[node] = it->second.size();
                }
            }

            // Build reverse dependency map
            std::unordered_map<ISystem*, std::vector<ISystem*>> dependents;
            for (const auto& [node, deps] : dependencies) {
                for (auto* dep : deps) {
                    dependents[dep].push_back(node);
                }
            }

            // Process nodes with in-degree 0
            while (!inDegree.empty()) {
                std::vector<ISystem*> currentLevel;

                // Find all nodes with in-degree 0
                for (const auto& [node, degree] : inDegree) {
                    if (degree == 0) {
                        currentLevel.push_back(node);
                    }
                }

                // If no nodes found, there is a cycle
                if (currentLevel.empty()) {
                    break;
                }

                // Add current level to levels
                levels.push_back(currentLevel);

                // Remove current level nodes and update in-degrees
                for (auto* node : currentLevel) {
                    inDegree.erase(node);

                    // Decrease in-degree of dependents
                    auto depIt = dependents.find(node);
                    if (depIt != dependents.end()) {
                        // For each dependent, decrease its in-degree
                        for (auto* dependent : depIt->second) {
                            auto degIt = inDegree.find(dependent);

                            // Safely decrease in-degree if it exists
                            if (degIt != inDegree.end()) {
                                degIt->second--;
                            }
                        }
                    }
                }
            }

            return levels;
        }
    }  // namespace

    /**
     * @brief Add a system to the graph (direct system pointer).
     * @param system System to add
     */
    void SystemDependencyGraph::AddSystem(ISystem* system) {
        if (!system) return;

        // Check if already added
        auto it = std::find(m_systems.begin(), m_systems.end(), system);
        if (it == m_systems.end()) {
            m_systems.push_back(system);
            m_dependencies[system] = {};
            m_dependents[system] = {};
            
            // Create metadata entry for this system
            SystemMetadata meta = system->GetMetadata();
            m_systemMetadata.push_back(meta);
        }
    }

    /**
     * @brief Add a system to the graph using metadata (for group-based scheduling).
     * @param metadata System metadata
     */
    void SystemDependencyGraph::AddSystemMetadata(const SystemMetadata& metadata) {
        if (!metadata.GetSystemPtr()) return;

        // Check if already added
        auto it = std::find(m_systems.begin(), m_systems.end(), metadata.GetSystemPtr());
        if (it == m_systems.end()) {
            m_systems.push_back(metadata.GetSystemPtr());
            m_dependencies[metadata.GetSystemPtr()] = {};
            m_dependents[metadata.GetSystemPtr()] = {};
            m_systemMetadata.push_back(metadata);
        }
    }

    /**
     * @brief Build the dependency graph.
     */
    void SystemDependencyGraph::Build() {
        m_allDependencies.clear();

        // Analyze each pair of systems for conflicts
        for (size_t i = 0; i < m_systems.size(); ++i) {
            for (size_t j = i + 1; j < m_systems.size(); ++j) {
                ISystem* systemA = m_systems[i];
                ISystem* systemB = m_systems[j];

                if (_hasComponentConflict(systemA, systemB)) {
                    // systemB must wait for systemA
                    // (based on execution order in metadata)
                    int orderA = systemA->GetMetadata().GetExecutionOrder();
                    int orderB = systemB->GetMetadata().GetExecutionOrder();

                    if (orderA < orderB) {
                        // A comes before B, so B depends on A
                        m_dependencies[systemB].push_back(systemA);
                        m_dependents[systemA].push_back(systemB);
                        m_allDependencies.push_back({systemB, systemA, "Component conflict"});
                    }
                    else if (orderA > orderB) {
                        // B comes before A, so A depends on B
                        m_dependencies[systemA].push_back(systemB);
                        m_dependents[systemB].push_back(systemA);
                        m_allDependencies.push_back({systemA, systemB, "Component conflict"});
                    }
                    // If orderA == orderB, it's an error (systems have same order but conflict)
                }
            }
        }
    }

    /**
     * @brief Check if two systems can safely run in parallel.
     * @param systemA First system
     * @param systemB Second system
     * @return True if they have no read/write conflicts
     */
    bool SystemDependencyGraph::CanRunInParallel(const ISystem* systemA, const ISystem* systemB) const {
        if (!systemA || !systemB) return false;
        if (systemA == systemB) return false;

        // Check if they have a dependency relationship
        auto depIt = m_dependencies.find(const_cast<ISystem*>(systemA));
        if (depIt != m_dependencies.end()) {
            // systemA depends on systemB?
            auto it = std::find(depIt->second.begin(), depIt->second.end(), 
                              const_cast<ISystem*>(systemB));
            if (it != depIt->second.end()) {
                return false;  // Direct dependency
            }
        }

        depIt = m_dependencies.find(const_cast<ISystem*>(systemB));
        if (depIt != m_dependencies.end()) {
            // systemB depends on systemA?
            auto it = std::find(depIt->second.begin(), depIt->second.end(), 
                              const_cast<ISystem*>(systemA));
            if (it != depIt->second.end()) {
                return false;  // Direct dependency
            }
        }

        return true;  // No dependency relationship
    }

    /**
     * @brief Check if two systems can safely run in parallel (metadata version).
     * @param sys1 First system metadata
     * @param sys2 Second system metadata
     * @return True if they have no read/write conflicts
     */
    bool SystemDependencyGraph::CanRunInParallel(const SystemMetadata& sys1,
                                                const SystemMetadata& sys2) const {
        return !HasConflict(sys1, sys2);
    }

    /**
     * @brief Get dependencies of a system.
     * @param system System to query
     * @return Vector of systems that the given system depends on
     */
    std::vector<ISystem*> SystemDependencyGraph::GetDependencies(const ISystem* system) const {
        auto it = m_dependencies.find(const_cast<ISystem*>(system));
        if (it != m_dependencies.end()) {
            return it->second;
        }
        return {};
    }

    /**
     * @brief Get dependents of a system.
     * @param system System to query
     * @return Vector of systems that depend on the given system
     */
    std::vector<ISystem*> SystemDependencyGraph::GetDependents(const ISystem* system) const {
        auto it = m_dependents.find(const_cast<ISystem*>(system));
        if (it != m_dependents.end()) {
            return it->second;
        }
        return {};
    }

    /**
     * @brief Get execution levels (systems that can run in parallel).
     * @return Vector of levels, each containing systems that can run in parallel
     */
    std::vector<std::vector<ISystem*>> SystemDependencyGraph::GetExecutionLevels() const {
        return _topologicalSort();
    }

    /**
     * @brief Get all system dependencies.
     * @return Vector of all system dependencies
     */
    const std::vector<SystemDependency>& SystemDependencyGraph::GetAllDependencies() const {
        return m_allDependencies;
    }

    /**
     * @brief Check if the dependency graph is valid (no cycles).
     * @return True if valid, false if cycles exist
     */
    bool SystemDependencyGraph::IsValid() const {
        return !_hasCycle();
    }

    /**
     * @brief Clear all systems and dependencies from the graph.
     */
    void SystemDependencyGraph::Clear() {
        m_systems.clear();
        m_systemMetadata.clear();
        m_dependencies.clear();
        m_dependents.clear();
        m_allDependencies.clear();
    }

    /**
     * @brief Check if two systems have any component access conflicts.
     * @param systemA Pointer to system A
     * @param systemB Pointer to system B
     * @return True if there is a conflict, false otherwise
     */
    bool SystemDependencyGraph::_hasComponentConflict(const ISystem* systemA, const ISystem* systemB) const {
        auto metaA = systemA->GetMetadata();
        auto metaB = systemB->GetMetadata();

        return HasConflict(metaA, metaB);
    }

    /**
     * @brief Check if the dependency graph has cycles.
     * @return True if cycles exist, false otherwise
     */
    bool SystemDependencyGraph::_hasCycle() const {
        return HasCycle(m_systems, m_dependencies);
    }

    /**
     * @brief Perform a topological sort on the dependency graph.
     * @return Vector of levels, each containing systems that can run in parallel
     */
    std::vector<std::vector<ISystem*>> SystemDependencyGraph::_topologicalSort() const {
        return TopologicalSort(m_systems, m_dependencies);
    }

    /**
     * @brief Validate system metadata for consistency.
     * @return True if all metadata is valid, false otherwise
     */
    bool SystemDependencyGraph::ValidateSystemMetadata() const {
        auto errors = GetValidationErrors();
        return errors.empty();
    }

    /**
     * @brief Get validation errors in system metadata.
     * @return Vector of error description strings
     */
    std::vector<std::string> SystemDependencyGraph::GetValidationErrors() const {
        std::vector<std::string> errors;

        // Check for duplicate execution orders in the group
        std::unordered_map<int, std::vector<std::string>> orderMap;
        for (auto* system : m_systems) {
            auto meta = system->GetMetadata();
            orderMap[meta.GetExecutionOrder()].push_back(meta.GetName());
        }

        // Report duplicate orders
        for (const auto& [order, names] : orderMap) {
            if (names.size() > 1) {
                std::stringstream ss;
                ss << "Multiple systems share execution order " << order << ": ";
                for (size_t i = 0; i < names.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << names[i];
                }
                errors.push_back(ss.str());
            }
        }

        // Check each system's metadata for duplicate components
        for (auto* system : m_systems) {
            if (!system) continue;

            auto meta = system->GetMetadata();

            // Check for duplicate components within read/write lists
            std::unordered_set<uint32_t> seenRead;
            for (auto compId : meta.GetReadComponents()) {
                if (seenRead.count(compId)) {
                    std::stringstream ss;
                    ss << "System " << meta.GetName() << " declares same component multiple times in ReadComponents";
                    errors.push_back(ss.str());
                }
                seenRead.insert(compId);
            }

            // Check for duplicate components within write lists
            std::unordered_set<uint32_t> seenWrite;
            for (auto compId : meta.GetWriteComponents()) {
                if (seenWrite.count(compId)) {
                    std::stringstream ss;
                    ss << "System " << meta.GetName() << " declares same component multiple times in WriteComponents";
                    errors.push_back(ss.str());
                }
                seenWrite.insert(compId);
            }
        }

        return errors;
    }

    /**
     * @brief Get detailed conflict descriptions between two systems.
     * @param systemA Pointer to system A
     * @param systemB Pointer to system B
     * @return Vector of conflict description strings
     */
    std::vector<std::string> SystemDependencyGraph::GetConflictDetails(const ISystem* systemA,
                                                                       const ISystem* systemB) const {
        std::vector<std::string> conflicts;

        if (!systemA || !systemB) return conflicts;

        auto metaA = systemA->GetMetadata();
        auto metaB = systemB->GetMetadata();

        return GetConflictDetailsForMetadata(metaA, metaB);
    }

    /**
     * @brief Build execution stages for a specific system group.
     * @param group System group to build stages for
     * @return Vector of stages, each containing systems that can run in parallel
     */
    std::vector<std::vector<ISystem*>> SystemDependencyGraph::BuildExecutionStagesForGroup(SystemGroup group) const {
        std::vector<std::vector<ISystem*>> stages;

        // Filter systems by group
        std::vector<ISystem*> groupSystems;
        for (size_t i = 0; i < m_systemMetadata.size(); ++i) {
            if (m_systemMetadata[i].GetGroup() == group) {
                groupSystems.push_back(m_systems[i]);
            }
        }

        // If no systems in group, return empty stages
        if (groupSystems.empty()) {
            return stages;
        }

        // Calculate in-degree for each system in group
        std::unordered_map<ISystem*, size_t> inDegree;
        for (auto* sys : groupSystems) {
            inDegree[sys] = 0;
        }

        // Count incoming edges
        for (auto* sys : groupSystems) {
            auto it = m_dependencies.find(sys);
            if (it != m_dependencies.end()) {
                for (auto* dep : it->second) {
                    if (std::find(groupSystems.begin(), groupSystems.end(), dep) != groupSystems.end()) {
                        inDegree[sys]++;
                    }
                }
            }
        }

        // Topological sort with stage grouping
        std::unordered_set<ISystem*> processed;

        while (processed.size() < groupSystems.size()) {
            // Find systems with zero in-degree in this group
            std::vector<ISystem*> currentStage;

            for (auto* sys : groupSystems) {
                if (processed.find(sys) == processed.end() && inDegree[sys] == 0) {
                    currentStage.push_back(sys);
                }
            }

            if (currentStage.empty()) {
                break;  // Should not happen if graph is valid DAG
            }

            stages.push_back(currentStage);

            // Mark systems as processed and reduce in-degree of dependents
            for (auto* sys : currentStage) {
                processed.insert(sys);

                auto depIt = m_dependents.find(sys);
                if (depIt != m_dependents.end()) {
                    for (auto* dependent : depIt->second) {
                        if (inDegree.find(dependent) != inDegree.end()) {
                            inDegree[dependent]--;
                        }
                    }
                }
            }
        }

        return stages;
    }

    /**
     * @brief Get system metadata by system pointer.
     * @param system System pointer
     * @return Pointer to SystemMetadata if found, nullptr otherwise
     */
    const SystemMetadata* SystemDependencyGraph::GetSystemMetadata(ISystem* system) const {
        for (size_t i = 0; i < m_systems.size(); ++i) {
            if (m_systems[i] == system) {
                return &m_systemMetadata[i];
            }
        }
        return nullptr;
    }

}
