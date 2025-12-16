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
#include "ecs/ComponentConflictDetector.h"
#include "ecs/CycleDetector.h"
#include "ecs/DependencyTopologicalSort.h"
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
            
            // Create metadata entry for this system
            SystemMetadata meta = system->GetMetadata();
            m_systemMetadata.push_back(meta);
        }
    }

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

    bool SystemDependencyGraph::CanRunInParallel(const SystemMetadata& sys1,
                                                const SystemMetadata& sys2) const {
        // Use unified conflict detector with computed read/write components
        return !ComponentConflictDetector::HasConflict(
            sys1.GetWriteComponents(), sys2.GetWriteComponents(),
            sys1.GetReadComponents(), sys2.GetReadComponents()
        );
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
        m_systemMetadata.clear();
        m_dependencies.clear();
        m_dependents.clear();
        m_allDependencies.clear();
    }

    bool SystemDependencyGraph::_hasComponentConflict(const ISystem* systemA, const ISystem* systemB) const {
        auto metaA = systemA->GetMetadata();
        auto metaB = systemB->GetMetadata();

        // Use unified conflict detector
        return ComponentConflictDetector::HasConflict(metaA, metaB);
    }

    bool SystemDependencyGraph::_hasCycle() const {
        // Use unified cycle detector
        return CycleDetector<ISystem*>::HasCycle(m_systems, m_dependencies);
    }

    std::vector<std::vector<ISystem*>> SystemDependencyGraph::_topologicalSort() const {
        // Use unified topological sorter
        return TopologicalSorter<ISystem*>::Sort(m_systems, m_dependencies);
    }

    void SystemDependencyGraph::_dfsTopo(ISystem* system,
                                         std::unordered_set<ISystem*>& visited,
                                         std::vector<ISystem*>& stack) const {
        // This method is now unused as _topologicalSort uses TopologicalSorter.
        // Kept for potential backward compatibility if external code references it.
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
            orderMap[meta.GetExecutionOrder()].push_back(meta.GetName());
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
            for (auto compId : meta.GetReadComponents()) {
                if (seenRead.count(compId)) {
                    std::stringstream ss;
                    ss << "System " << meta.GetName() << " declares same component multiple times in ReadComponents";
                    errors.push_back(ss.str());
                }
                seenRead.insert(compId);
            }

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

    std::vector<std::string> SystemDependencyGraph::GetConflictDetails(const ISystem* systemA,
                                                                       const ISystem* systemB) const {
        std::vector<std::string> conflicts;

        if (!systemA || !systemB) return conflicts;

        auto metaA = systemA->GetMetadata();
        auto metaB = systemB->GetMetadata();

        // Use unified conflict detector for details
        return ComponentConflictDetector::GetConflictDetails(metaA, metaB);
    }

    std::vector<std::vector<ISystem*>> SystemDependencyGraph::BuildExecutionStagesForGroup(SystemGroup group) const {
        std::vector<std::vector<ISystem*>> stages;

        // Filter systems by group
        std::vector<ISystem*> groupSystems;
        for (size_t i = 0; i < m_systemMetadata.size(); ++i) {
            if (m_systemMetadata[i].GetGroup() == group) {
                groupSystems.push_back(m_systems[i]);
            }
        }

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

    const SystemMetadata* SystemDependencyGraph::GetSystemMetadata(ISystem* system) const {
        for (size_t i = 0; i < m_systems.size(); ++i) {
            if (m_systems[i] == system) {
                return &m_systemMetadata[i];
            }
        }
        return nullptr;
    }

}
