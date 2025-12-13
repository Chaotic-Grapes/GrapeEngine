/* Start Header *****************************************************************/
/*!
\file    JobManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of JobManager - the central job scheduling
and execution system for the ECS job system.

JobManager handles:
- Job scheduling with dependency tracking
- Worker thread pool management
- Job completion synchronization
- Thread pool configuration and lifecycle

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

#include "ecs/jobs/JobHandle.h"
#include "ecs/jobs/IJob.h"
#include "ecs/jobs/JobQueue.h"
#include "ecs/jobs/WorkStealingQueue.h"
#include "ecs/jobs/JobProfiler.h"
#include <memory>
#include <vector>
#include <thread>
#include <cstdint>
#include "Export.h"

namespace ECS::Jobs {

    /**
     * @brief Configuration for the job system.
     */
    struct JobSystemConfig {
        // Number of worker threads (0 = use hardware concurrency)
        uint32_t NumWorkerThreads = 0;

        // Enable work stealing for load balancing
        bool EnableWorkStealing = true;

        // Enable profiling and timing data collection
        bool EnableProfilingData = false;

        // Enable strict dependency validation
        bool ValidateDependencies = true;

        // Default job priority for scheduled jobs
        int DefaultJobPriority = 0;
    };

    /**
     * @brief Central manager for job scheduling, execution, and synchronization.
     * 
     * The JobManager is responsible for:
     * - Scheduling jobs with optional dependencies
     * - Managing a pool of worker threads
     * - Distributing work via job queue
     * - Synchronizing job completion
     * - Configuration and lifecycle management
     * 
     * Thread-safe for job scheduling from any thread, but work distribution
     * happens to worker threads.
     * 
     * Example usage:
     * @code
     * // Create and configure job manager
     * JobSystemConfig config;
     * config.NumWorkerThreads = 4;
     * 
     * JobManager jobManager(config);
     * 
     * // Schedule jobs
     * auto job1 = std::make_unique<MyJob>();
     * auto handle1 = jobManager.Schedule(std::move(job1));
     * 
     * auto job2 = std::make_unique<MyOtherJob>();
     * auto handle2 = jobManager.Schedule(std::move(job2), handle1);  // Depends on job1
     * 
     * // Wait for all work to complete
     * jobManager.CompleteAllJobs();
     * @endcode
     */
    class GRAPEENGINE_API JobManager {
    public:
        /**
         * @brief Create a job manager with default configuration.
         */
        JobManager();

        /**
         * @brief Create a job manager with custom configuration.
         * @param config Configuration for the job system
         */
        explicit JobManager(const JobSystemConfig& config);

        /**
         * @brief Destructor - ensures all jobs complete and threads shut down.
         */
        ~JobManager();

        // Delete copy operations
        JobManager(const JobManager&) = delete;
        JobManager& operator=(const JobManager&) = delete;

        // Delete move operations (JobQueue is not movable due to mutex)
        JobManager(JobManager&&) = delete;
        JobManager& operator=(JobManager&&) = delete;

        /**
         * @brief Schedule a job for execution.
         * 
         * The job will be added to the work queue and executed by a worker thread.
         * Ownership of the job is transferred to the job manager.
         * 
         * @param job The job to schedule (unique_ptr ownership transferred)
         * @param dependsOn Optional job handle that this job depends on
         * @param priority Job priority (higher = earlier execution)
         * @return JobHandle for synchronization and dependency tracking
         */
        JobHandle Schedule(std::unique_ptr<IJob> job, 
                          const JobHandle& dependsOn = JobHandle{},
                          int priority = 0);

        /**
         * @brief Schedule multiple independent jobs for parallel execution.
         * 
         * Schedules all jobs to run in parallel. If dependencies exist between
         * them, use individual Schedule() calls with explicit dependencies.
         * 
         * @param jobs Vector of jobs to schedule
         * @param priority Job priority for all jobs
         * @return JobHandle representing completion of all jobs
         */
        JobHandle ScheduleParallel(std::vector<std::unique_ptr<IJob>> jobs,
                                   int priority = 0);

        /**
         * @brief Block until a specific job completes.
         * 
         * Spins until the job referenced by the handle is complete.
         * Safe to call from any thread.
         * 
         * @param handle Job handle to wait for
         */
        void Complete(const JobHandle& handle) const;

        /**
         * @brief Block until all scheduled jobs complete.
         * 
         * Processes all pending jobs and waits for completion.
         * Useful as a synchronization point between frames or phases.
         */
        void CompleteAllJobs();

        /**
         * @brief Check if job system is running.
         * @return true if worker threads are active, false if shut down
         */
        bool IsRunning() const;

        /**
         * @brief Get the number of active worker threads.
         * @return Number of worker threads
         */
        uint32_t GetNumWorkerThreads() const { return m_numWorkerThreads; }

        /**
         * @brief Get the number of pending jobs in queue.
         * @return Approximate number of unexecuted jobs
         */
        size_t GetPendingJobCount() const;

        /**
         * @brief Shutdown the job manager.
         * 
         * Signals all worker threads to stop and waits for them to finish.
         * After shutdown, no new jobs can be scheduled.
         * 
         * Called automatically in destructor.
         */
        void Shutdown();

        /**
         * @brief Get current job system configuration.
         * @return Reference to active configuration
         */
        const JobSystemConfig& GetConfig() const { return m_config; }

        /**
         * @brief Get the job profiler for performance metrics.
         * @return Reference to the profiler
         */
        JobProfiler& GetProfiler() { return m_profiler; }

        /**
         * @brief Get const access to the job profiler.
         * @return Const reference to the profiler
         */
        const JobProfiler& GetProfiler() const { return m_profiler; }

        /**
         * @brief Enable or disable job profiling.
         * @param enabled true to enable profiling, false to disable
         */
        void SetProfilingEnabled(bool enabled) { m_profiler.SetEnabled(enabled); }

        /**
         * @brief Check if job profiling is enabled.
         * @return true if profiling is active
         */
        bool IsProfilingEnabled() const { return m_profiler.IsEnabled(); }

        /**
         * @brief Reset profiling data.
         */
        void ResetProfiler() { m_profiler.Reset(); }

        /**
         * @brief Generate a profiling report.
         * @return Formatted string with profiling statistics
         */
        std::string GetProfilingReport() const { return m_profiler.GenerateReport(); }

    private:
        /**
         * @brief Worker thread entry point.
         * 
         * Called by each worker thread in the pool. Continuously pulls
         * work from the queue until shutdown is signaled.
         */
        void _workerThread(uint32_t threadIndex);

        /**
         * @brief Initialize worker threads based on config.
         */
        void _initializeWorkerThreads();

        /**
         * @brief Mark a job as complete.
         * @param completionState The completion state to mark true
         */
        void _markJobComplete(const std::shared_ptr<std::atomic<bool>>& completionState);

        // Configuration
        JobSystemConfig m_config;

        // Job queue for work distribution (can be either standard or work-stealing)
        JobQueue m_jobQueue;
        std::unique_ptr<WorkStealingQueue> m_workStealingQueue;

        // Worker threads
        std::vector<std::thread> m_workerThreads;
        uint32_t m_numWorkerThreads = 0;

        // Job profiler for performance metrics
        JobProfiler m_profiler;

        // Shutdown flag
        std::atomic<bool> m_shutdown{false};

        // Friend declaration for work item access
        friend class JobHandle;
    };

}

#endif
