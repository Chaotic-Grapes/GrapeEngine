/* Start Header *****************************************************************/
/*!
\file    JobDependencyGraph.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of JobDependencyGraph. Dependency tracking and conflict detection
for parallel job execution.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/jobs/JobDependencyGraph.h"
#include <algorithm>
#include <queue>

namespace ECS::Jobs {

    void JobDependencyGraph::AddJob(const JobMetadata& metadata) {
        size_t jobIndex = m_jobs.size();
        
        // Store job metadata
        auto newMeta = metadata;
        newMeta.ScheduleOrder = jobIndex;
        m_jobs.push_back(newMeta);

        // Analyze conflicts with existing jobs
        for (size_t i = 0; i < jobIndex; ++i) {
            if (CanRunInParallel(m_jobs[i], newMeta)) {
                // No conflict - no dependency edge
                continue;
            }

            // Conflict detected - new job depends on previous job
            m_dependencies[jobIndex].push_back(i);
            m_reverseDependencies[i].push_back(jobIndex);
        }

        // Update last writer and reader tracking
        for (const auto& access : metadata.ComponentAccesses) {
            size_t compHash = static_cast<size_t>(access.ComponentId);

            if (access.Mode == ComponentAccessMode::Write ||
                access.Mode == ComponentAccessMode::ReadWrite) {
                m_lastWriterIndex[compHash] = jobIndex;
                m_readerIndices[compHash].clear();  // Clear readers when new writer scheduled
            } else if (access.Mode == ComponentAccessMode::Read) {
                m_readerIndices[compHash].push_back(jobIndex);
            }
        }
    }

    void JobDependencyGraph::Clear() {
        m_jobs.clear();
        m_dependencies.clear();
        m_reverseDependencies.clear();
        m_lastWriterIndex.clear();
        m_readerIndices.clear();
    }

    bool JobDependencyGraph::CanRunInParallel(
        const JobMetadata& job1,
        const JobMetadata& job2) const {
        
        // Two jobs can run in parallel if they don't have conflicting component accesses
        for (const auto& access1 : job1.ComponentAccesses) {
            for (const auto& access2 : job2.ComponentAccesses) {
                // Check if accessing same component
                if (access1.ComponentId == access2.ComponentId) {
                    // Conflict if either is writing
                    if (access1.Mode != ComponentAccessMode::Read ||
                        access2.Mode != ComponentAccessMode::Read) {
                        return false;  // Conflict detected
                    }
                }
            }
        }

        return true;  // No conflicts
    }

    std::vector<JobHandle> JobDependencyGraph::GetDependencies(
        const JobHandle& jobHandle) const {
        
        std::vector<JobHandle> result;

        // Find job with matching handle
        for (size_t i = 0; i < m_jobs.size(); ++i) {
            if (m_jobs[i].Handle == jobHandle) {
                // Get all dependencies for this job
                auto it = m_dependencies.find(i);
                if (it != m_dependencies.end()) {
                    for (size_t depIdx : it->second) {
                        if (depIdx < m_jobs.size()) {
                            result.push_back(m_jobs[depIdx].Handle);
                        }
                    }
                }
                break;
            }
        }

        return result;
    }

    std::vector<std::vector<JobHandle>> JobDependencyGraph::BuildExecutionStages() const {
        std::vector<std::vector<JobHandle>> stages;
        
        if (m_jobs.empty()) {
            return stages;
        }

        // Calculate in-degree for each job
        std::unordered_map<size_t, size_t> inDegree;
        for (size_t i = 0; i < m_jobs.size(); ++i) {
            inDegree[i] = 0;
        }

        for (const auto& [fromIdx, toIndices] : m_dependencies) {
            for (size_t toIdx : toIndices) {
                inDegree[toIdx]++;
            }
        }

        // Topological sort with stage grouping
        std::vector<bool> processed(m_jobs.size(), false);

        while (true) {
            // Find all jobs with zero in-degree
            std::vector<JobHandle> currentStage;
            std::vector<size_t> currentStageIndices;

            for (size_t i = 0; i < m_jobs.size(); ++i) {
                if (!processed[i] && inDegree[i] == 0) {
                    currentStage.push_back(m_jobs[i].Handle);
                    currentStageIndices.push_back(i);
                }
            }

            if (currentStage.empty()) {
                break;  // All jobs processed or circular dependency
            }

            stages.push_back(currentStage);

            // Process jobs in current stage
            for (size_t idx : currentStageIndices) {
                processed[idx] = true;

                // Reduce in-degree of dependent jobs
                auto it = m_reverseDependencies.find(idx);
                if (it != m_reverseDependencies.end()) {
                    for (size_t dependent : it->second) {
                        if (inDegree[dependent] > 0) {
                            inDegree[dependent]--;
                        }
                    }
                }
            }
        }

        return stages;
    }

    bool JobDependencyGraph::HasCircularDependencies() const {
        return _detectCycles();
    }

    const JobMetadata* JobDependencyGraph::GetJobMetadata(
        const JobHandle& jobHandle) const {
        
        for (const auto& job : m_jobs) {
            if (job.Handle == jobHandle) {
                return &job;
            }
        }
        return nullptr;
    }

    size_t JobDependencyGraph::_getComponentHash(const std::type_info& typeInfo) {
        return typeInfo.hash_code();
    }

    bool JobDependencyGraph::_detectCycles() const {
        // Use DFS to detect cycles in the dependency graph
        std::vector<int> color(m_jobs.size(), 0);  // 0=white, 1=gray, 2=black

        std::function<bool(size_t)> hasCycleDFS = [&](size_t u) -> bool {
            color[u] = 1;  // Mark as gray (in progress)

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

            color[u] = 2;  // Mark as black (complete)
            return false;
        };

        for (size_t i = 0; i < m_jobs.size(); ++i) {
            if (color[i] == 0) {
                if (hasCycleDFS(i)) {
                    return true;
                }
            }
        }

        return false;
    }

}
