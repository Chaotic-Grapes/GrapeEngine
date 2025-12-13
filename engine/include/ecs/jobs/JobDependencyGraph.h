/* Start Header *****************************************************************/
/*!
\file    JobDependencyGraph.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of JobDependencyGraph - a system for tracking
job dependencies and detecting component access conflicts.

Builds a directed acyclic graph (DAG) of job dependencies based on component
read/write patterns, enabling automatic parallelization detection.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef JOB_DEPENDENCY_GRAPH_H
#define JOB_DEPENDENCY_GRAPH_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <typeinfo>
#include "ecs/jobs/JobHandle.h"
#include "ecs/jobs/IJob.h"

namespace ECS::Jobs {

    /**
     * @brief Metadata about a scheduled job for dependency tracking.
     */
    struct JobMetadata {
        JobHandle Handle;
        std::string Name;
        std::vector<ComponentAccess> ComponentAccesses;
        size_t ScheduleOrder = 0;  // Order in which job was scheduled
    };

    /**
     * @brief Tracks dependencies between jobs based on component access patterns.
     * 
     * The JobDependencyGraph analyzes read/write patterns of scheduled jobs
     * to construct a DAG showing which jobs must complete before others can start.
     * 
     * Conflict Rules:
     * - Write conflicts: Two jobs writing same component → sequential
     * - Read-write conflicts: One job writes, another reads same component → sequential
     * - Read-read: Multiple jobs reading same component → can run in parallel
     * 
     * Example:
     * @code
     * JobDependencyGraph graph;
     * 
     * // Add jobs with their component accesses
     * auto meta1 = JobMetadata{handle1, "Job1", {accesses1}};
     * auto meta2 = JobMetadata{handle2, "Job2", {accesses2}};
     * 
     * graph.AddJob(meta1);
     * graph.AddJob(meta2);
     * 
     * // Check if jobs can run in parallel
     * if (graph.CanRunInParallel(meta1, meta2)) {
     *     // Schedule both simultaneously
     * }
     * 
     * // Get execution stages
     * auto stages = graph.BuildExecutionStages();
     * @endcode
     */
    class JobDependencyGraph {
    public:
        JobDependencyGraph() = default;
        ~JobDependencyGraph() = default;

        // Delete copy operations
        JobDependencyGraph(const JobDependencyGraph&) = delete;
        JobDependencyGraph& operator=(const JobDependencyGraph&) = delete;

        /**
         * @brief Add a job to the dependency graph.
         * 
         * Analyzes the job's component accesses and updates dependencies
         * relative to previously added jobs.
         * 
         * @param metadata Job metadata including handle and component accesses
         */
        void AddJob(const JobMetadata& metadata);

        /**
         * @brief Clear all jobs and dependencies.
         * 
         * Useful for resetting between frames.
         */
        void Clear();

        /**
         * @brief Check if two jobs can run in parallel.
         * 
         * Two jobs can run in parallel if they have no conflicting
         * component access patterns.
         * 
         * @param job1 First job
         * @param job2 Second job
         * @return true if jobs can run simultaneously, false if sequential required
         */
        bool CanRunInParallel(const JobMetadata& job1, const JobMetadata& job2) const;

        /**
         * @brief Get all jobs that must complete before a given job can start.
         * 
         * @param jobHandle The job to get dependencies for
         * @return Vector of job handles that must complete first
         */
        std::vector<JobHandle> GetDependencies(const JobHandle& jobHandle) const;

        /**
         * @brief Build execution stages from job dependencies.
         * 
         * Groups jobs into stages where:
         * - All jobs in a stage can run in parallel
         * - All jobs in a stage must complete before next stage
         * 
         * @return Vector of stages, each containing job handles that can run in parallel
         */
        std::vector<std::vector<JobHandle>> BuildExecutionStages() const;

        /**
         * @brief Get number of jobs in the graph.
         * @return Number of jobs added
         */
        size_t GetJobCount() const { return m_jobs.size(); }

        /**
         * @brief Check for circular dependencies.
         * 
         * @return true if a cycle is detected (invalid state), false if DAG is valid
         */
        bool HasCircularDependencies() const;

        /**
         * @brief Get metadata for a job.
         * @param jobHandle The job handle
         * @return Pointer to metadata, or nullptr if not found
         */
        const JobMetadata* GetJobMetadata(const JobHandle& jobHandle) const;

    private:
        /**
         * @brief Check if two component access patterns conflict.
         * 
         * @param access1 First component access
         * @param access2 Second component access
         * @param sameComponent Whether both accesses are to the same component
         * @return true if accesses conflict, false if compatible
         */
        bool _accessesConflict(
            const ComponentAccess& access1,
            const ComponentAccess& access2) const;

        /**
         * @brief Get component type hash for quick lookup.
         * @param typeInfo Type information
         * @return Hash of component type
         */
        static size_t _getComponentHash(const std::type_info& typeInfo);

        /**
         * @brief Perform topological sort to detect cycles.
         * @return true if cycle detected, false if valid DAG
         */
        bool _detectCycles() const;

        // Storage of all jobs
        std::vector<JobMetadata> m_jobs;

        // Dependency edges: from → to (job i must complete before job j)
        std::unordered_map<size_t, std::vector<size_t>> m_dependencies;

        // Reverse dependencies for efficient lookup
        std::unordered_map<size_t, std::vector<size_t>> m_reverseDependencies;

        // Track last job writing to each component
        std::unordered_map<size_t, size_t> m_lastWriterIndex;

        // Track all jobs reading each component
        std::unordered_map<size_t, std::vector<size_t>> m_readerIndices;
    };

}

#endif
