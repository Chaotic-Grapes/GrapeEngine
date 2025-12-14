/* Start Header *****************************************************************/
/*!
\file    SystemDependencyGraph.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of SystemDependencyGraph - analyzes system dependencies and
determines safe parallel execution schedules.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/SystemDependencyGraph.h"
#include "ecs/ComponentAccessAttribute.h"
#include <algorithm>
#include <queue>
#include <iostream>
#include <sstream>

namespace ECS {

    void SystemDependencyGraph::AddSystem(ISystem* system) {
        if (!system) return;

        // Check if already added
        auto it = std::find(m_systems.begin(), m_systems.end(), system);
        if (it == m_systems.end()) {
            m_systems.push_back(system);
            m_dependencies[system] = {};
            m_dependents[system] = {};
        }
    }

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
                    int orderA = systemA->GetMetadata().ExecutionOrder;
                    int orderB = systemB->GetMetadata().ExecutionOrder;

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

    std::vector<ISystem*> SystemDependencyGraph::GetDependencies(const ISystem* system) const {
        auto it = m_dependencies.find(const_cast<ISystem*>(system));
        if (it != m_dependencies.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<ISystem*> SystemDependencyGraph::GetDependents(const ISystem* system) const {
        auto it = m_dependents.find(const_cast<ISystem*>(system));
        if (it != m_dependents.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<std::vector<ISystem*>> SystemDependencyGraph::GetExecutionLevels() const {
        return _topologicalSort();
    }

    const std::vector<SystemDependency>& SystemDependencyGraph::GetAllDependencies() const {
        return m_allDependencies;
    }

    bool SystemDependencyGraph::IsValid() const {
        return !_hasCycle();
    }

    void SystemDependencyGraph::Clear() {
        m_systems.clear();
        m_dependencies.clear();
        m_dependents.clear();
        m_allDependencies.clear();
    }

    bool SystemDependencyGraph::_hasComponentConflict(const ISystem* systemA, const ISystem* systemB) const {
        auto metaA = systemA->GetMetadata();
        auto metaB = systemB->GetMetadata();

        // Check for write-write conflicts
        for (const auto& compA : metaA.WriteComponents) {
            for (const auto& compB : metaB.WriteComponents) {
                if (compA == compB) {
                    return true;  // Both write to same component
                }
            }
        }

        // Check for write-read conflicts (A writes, B reads)
        for (const auto& compA : metaA.WriteComponents) {
            for (const auto& compB : metaB.ReadComponents) {
                if (compA == compB) {
                    return true;
                }
            }
        }

        // Check for read-write conflicts (A reads, B writes)
        for (const auto& compA : metaA.ReadComponents) {
            for (const auto& compB : metaB.WriteComponents) {
                if (compA == compB) {
                    return true;
                }
            }
        }

        return false;
    }

    bool SystemDependencyGraph::_hasCycle() const {
        std::unordered_set<ISystem*> visited;
        std::unordered_set<ISystem*> recStack;

        for (auto* system : m_systems) {
            if (visited.find(system) == visited.end()) {
                if (_dfsHasCycle(system, visited, recStack)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool SystemDependencyGraph::_dfsHasCycle(ISystem* system,
                                            std::unordered_set<ISystem*>& visited,
                                            std::unordered_set<ISystem*>& recStack) const {
        visited.insert(system);
        recStack.insert(system);

        auto it = m_dependencies.find(system);
        if (it != m_dependencies.end()) {
            for (auto* dep : it->second) {
                if (visited.find(dep) == visited.end()) {
                    if (_dfsHasCycle(dep, visited, recStack)) {
                        return true;
                    }
                }
                else if (recStack.find(dep) != recStack.end()) {
                    return true;  // Back edge - cycle found
                }
            }
        }

        recStack.erase(system);
        return false;
    }

    std::vector<std::vector<ISystem*>> SystemDependencyGraph::_topologicalSort() const {
        std::vector<std::vector<ISystem*>> levels;

        if (m_systems.empty()) {
            return levels;
        }

        // Compute in-degree for each system
        std::unordered_map<ISystem*, size_t> inDegree;
        for (auto* system : m_systems) {
            inDegree[system] = 0;
        }

        for (auto* system : m_systems) {
            auto it = m_dependencies.find(system);
            if (it != m_dependencies.end()) {
                inDegree[system] = it->second.size();
            }
        }

        // Process level by level
        while (!inDegree.empty()) {
            std::vector<ISystem*> currentLevel;

            // Find all systems with no dependencies
            for (const auto& [system, degree] : inDegree) {
                if (degree == 0) {
                    currentLevel.push_back(system);
                }
            }

            if (currentLevel.empty()) {
                // This shouldn't happen if graph is valid
                break;
            }

            levels.push_back(currentLevel);

            // Remove processed systems and update in-degrees
            for (auto* system : currentLevel) {
                inDegree.erase(system);

                // Reduce in-degree for dependents
                auto depIt = m_dependents.find(system);
                if (depIt != m_dependents.end()) {
                    for (auto* dependent : depIt->second) {
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

    void SystemDependencyGraph::_dfsTopo(ISystem* system,
                                         std::unordered_set<ISystem*>& visited,
                                         std::vector<ISystem*>& stack) const {
        visited.insert(system);

        auto it = m_dependencies.find(system);
        if (it != m_dependencies.end()) {
            for (auto* dep : it->second) {
                if (visited.find(dep) == visited.end()) {
                    _dfsTopo(dep, visited, stack);
                }
            }
        }

        stack.push_back(system);
    }

    bool SystemDependencyGraph::ValidateSystemMetadata() const {
        auto errors = GetValidationErrors();
        return errors.empty();
    }

    std::vector<std::string> SystemDependencyGraph::GetValidationErrors() const {
        std::vector<std::string> errors;

        // Check for duplicate execution orders in the group
        std::unordered_map<int, std::vector<std::string>> orderMap;
        for (auto* system : m_systems) {
            auto meta = system->GetMetadata();
            orderMap[meta.ExecutionOrder].push_back(meta.Name);
        }

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

        // Check each system's metadata
        for (auto* system : m_systems) {
            if (!system) continue;

            auto meta = system->GetMetadata();

            // Check for duplicate components within read/write lists
            std::unordered_set<uint32_t> seenRead;
            for (auto compId : meta.ReadComponents) {
                if (seenRead.count(compId.Hash)) {
                    std::stringstream ss;
                    ss << "System " << meta.Name << " declares same component multiple times in ReadComponents";
                    errors.push_back(ss.str());
                }
                seenRead.insert(compId.Hash);
            }

            std::unordered_set<uint32_t> seenWrite;
            for (auto compId : meta.WriteComponents) {
                if (seenWrite.count(compId.Hash)) {
                    std::stringstream ss;
                    ss << "System " << meta.Name << " declares same component multiple times in WriteComponents";
                    errors.push_back(ss.str());
                }
                seenWrite.insert(compId.Hash);
            }
        }

        return errors;
    }

    std::vector<std::string> SystemDependencyGraph::GetConflictDetails(const ISystem* systemA,
                                                                       const ISystem* systemB) const {
        std::vector<std::string> conflicts;

        if (!systemA || !systemB) return conflicts;

        auto metaA = systemA->GetMetadata();
        auto metaB = systemB->GetMetadata();

        // Check write-write conflicts
        for (const auto& compA : metaA.WriteComponents) {
            for (const auto& compB : metaB.WriteComponents) {
                if (compA == compB) {
                    std::stringstream ss;
                    ss << "Both " << metaA.Name << " and " << metaB.Name 
                       << " write to component 0x" << std::hex << compA.Hash;
                    conflicts.push_back(ss.str());
                }
            }
        }

        // Check write-read conflicts
        for (const auto& compA : metaA.WriteComponents) {
            for (const auto& compB : metaB.ReadComponents) {
                if (compA == compB) {
                    std::stringstream ss;
                    ss << metaA.Name << " writes to component (0x" << std::hex << compA.Hash 
                       << ") that " << metaB.Name << " reads";
                    conflicts.push_back(ss.str());
                }
            }
        }

        // Check read-write conflicts
        for (const auto& compA : metaA.ReadComponents) {
            for (const auto& compB : metaB.WriteComponents) {
                if (compA == compB) {
                    std::stringstream ss;
                    ss << metaA.Name << " reads component (0x" << std::hex << compA.Hash 
                       << ") that " << metaB.Name << " writes";
                    conflicts.push_back(ss.str());
                }
            }
        }

        return conflicts;
    }

}
