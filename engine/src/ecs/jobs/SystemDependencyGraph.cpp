/* Start Header *****************************************************************/
/*!
\file    SystemDependencyGraph.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of SystemDependencyGraph - system dependency tracking and
conflict detection for parallel system execution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/jobs/SystemDependencyGraph.h"
#include <algorithm>

namespace ECS {

    void SystemDependencyGraph::AddSystem(const SystemDependencyMetadata& metadata) {
        size_t systemIndex = m_systems.size();
        
        // Store system metadata
        auto newMeta = metadata;
        m_systems.push_back(newMeta);

        // Analyze conflicts with existing systems in same group
        for (size_t i = 0; i < systemIndex; ++i) {
            if (m_systems[i].Group != metadata.Group) {
                continue;  // Skip systems in different groups
            }

            if (CanRunInParallel(m_systems[i], newMeta)) {
                // No conflict - no dependency edge
                continue;
            }

            // Conflict detected - new system depends on previous system
            m_dependencies[systemIndex].push_back(i);
            m_reverseDependencies[i].push_back(systemIndex);
        }

        // Update write and reader tracking
        for (auto compId : metadata.WriteComponents) {
            m_lastWriterIndex[compId] = systemIndex;
            m_readerIndices[compId].clear();  // Clear readers when new writer
        }

        for (auto compId : metadata.ReadComponents) {
            m_readerIndices[compId].push_back(systemIndex);
        }
    }

    void SystemDependencyGraph::Clear() {
        m_systems.clear();
        m_dependencies.clear();
        m_reverseDependencies.clear();
        m_lastWriterIndex.clear();
        m_readerIndices.clear();
    }

    bool SystemDependencyGraph::CanRunInParallel(
        const SystemDependencyMetadata& sys1,
        const SystemDependencyMetadata& sys2) const {
        
        return !_componentAccessesConflict(
            sys1.WriteComponents, sys2.WriteComponents,
            sys1.ReadComponents, sys2.ReadComponents
        );
    }

    std::vector<ISystem*> SystemDependencyGraph::GetDependencies(
        ISystem* system) const {
        
        std::vector<ISystem*> result;

        // Find system in metadata
        for (size_t i = 0; i < m_systems.size(); ++i) {
            if (m_systems[i].System == system) {
                // Get dependencies for this system
                auto it = m_dependencies.find(i);
                if (it != m_dependencies.end()) {
                    for (size_t depIdx : it->second) {
                        if (depIdx < m_systems.size()) {
                            result.push_back(m_systems[depIdx].System);
                        }
                    }
                }
                break;
            }
        }

        return result;
    }

    std::vector<std::vector<ISystem*>> SystemDependencyGraph::BuildExecutionStagesForGroup(
        SystemGroup group) const {
        
        std::vector<std::vector<ISystem*>> stages;

        // Filter systems by group
        std::vector<size_t> groupIndices;
        for (size_t i = 0; i < m_systems.size(); ++i) {
            if (m_systems[i].Group == group) {
                groupIndices.push_back(i);
            }
        }

        if (groupIndices.empty()) {
            return stages;
        }

        // Calculate in-degree for each system in group
        std::unordered_map<size_t, size_t> inDegree;
        for (size_t idx : groupIndices) {
            inDegree[idx] = 0;
        }

        // Count incoming edges
        for (size_t idx : groupIndices) {
            auto it = m_dependencies.find(idx);
            if (it != m_dependencies.end()) {
                for (size_t depIdx : it->second) {
                    if (std::find(groupIndices.begin(), groupIndices.end(), depIdx) != groupIndices.end()) {
                        inDegree[idx]++;
                    }
                }
            }
        }

        // Topological sort with stage grouping
        std::vector<bool> processed(m_systems.size(), false);

        while (true) {
            // Find systems with zero in-degree in this group
            std::vector<ISystem*> currentStage;
            std::vector<size_t> currentStageIndices;

            for (size_t idx : groupIndices) {
                if (!processed[idx] && inDegree[idx] == 0) {
                    currentStage.push_back(m_systems[idx].System);
                    currentStageIndices.push_back(idx);
                }
            }

            if (currentStage.empty()) {
                break;
            }

            stages.push_back(currentStage);

            // Mark systems as processed and reduce in-degree of dependents
            for (size_t idx : currentStageIndices) {
                processed[idx] = true;

                auto it = m_reverseDependencies.find(idx);
                if (it != m_reverseDependencies.end()) {
                    for (size_t dependent : it->second) {
                        if (inDegree.find(dependent) != inDegree.end() && inDegree[dependent] > 0) {
                            inDegree[dependent]--;
                        }
                    }
                }
            }
        }

        return stages;
    }

    bool SystemDependencyGraph::HasCircularDependencies() const {
        return _detectCycles();
    }

    const SystemDependencyMetadata* SystemDependencyGraph::GetSystemMetadata(
        ISystem* system) const {
        
        for (const auto& meta : m_systems) {
            if (meta.System == system) {
                return &meta;
            }
        }
        return nullptr;
    }

    bool SystemDependencyGraph::_componentAccessesConflict(
        const std::vector<ComponentTypeId>& writeComps1,
        const std::vector<ComponentTypeId>& writeComps2,
        const std::vector<ComponentTypeId>& readComps1,
        const std::vector<ComponentTypeId>& readComps2) const {
        
        // Check write-write conflicts
        for (auto w1 : writeComps1) {
            for (auto w2 : writeComps2) {
                if (w1 == w2) {
                    return true;  // Both writing same component
                }
            }
        }

        // Check read-write conflicts
        for (auto w1 : writeComps1) {
            for (auto r2 : readComps2) {
                if (w1 == r2) {
                    return true;  // One writes, other reads
                }
            }
        }

        for (auto w2 : writeComps2) {
            for (auto r1 : readComps1) {
                if (w2 == r1) {
                    return true;  // One writes, other reads
                }
            }
        }

        // Read-read is safe
        return false;
    }

    bool SystemDependencyGraph::_detectCycles() const {
        // DFS-based cycle detection
        std::vector<int> color(m_systems.size(), 0);  // 0=white, 1=gray, 2=black

        std::function<bool(size_t)> hasCycleDFS = [&](size_t u) -> bool {
            color[u] = 1;  // Mark as gray

            auto it = m_dependencies.find(u);
            if (it != m_dependencies.end()) {
                for (size_t v : it->second) {
                    if (color[v] == 1) {
                        return true;  // Back edge - cycle detected
                    }
                    if (color[v] == 0 && hasCycleDFS(v)) {
                        return true;
                    }
                }
            }

            color[u] = 2;  // Mark as black
            return false;
        };

        for (size_t i = 0; i < m_systems.size(); ++i) {
            if (color[i] == 0) {
                if (hasCycleDFS(i)) {
                    return true;
                }
            }
        }

        return false;
    }

}
