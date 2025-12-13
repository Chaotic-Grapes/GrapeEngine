/* Start Header *****************************************************************/
/*!
\file    JobProfiler.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of JobProfiler - performance profiling and
metrics collection for the job system.

Tracks job execution times, queue wait times, and other performance metrics
to enable performance analysis, bottleneck identification, and optimization.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef JOB_PROFILER_H
#define JOB_PROFILER_H

#include <chrono>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

namespace ECS::Jobs {

    /**
     * @brief Performance metrics for a single job execution.
     */
    struct JobMetrics {
        // Job identification
        std::string JobName;
        
        // Timing information
        std::chrono::microseconds ExecutionTime{0};      // How long the job took to run
        std::chrono::microseconds ScheduleTime{0};       // When the job was scheduled
        std::chrono::microseconds StartTime{0};          // When execution started
        std::chrono::microseconds EndTime{0};            // When execution completed
        std::chrono::microseconds WaitTime{0};           // Time spent waiting to execute

        // Execution context
        uint32_t ExecutingThreadId = 0;                  // Which thread executed the job
        uint32_t Priority = 0;                           // Job priority
        bool WasStolen = false;                          // Whether job was stolen by another thread

        // Statistics
        size_t ComponentsAccessed = 0;                   // Number of components accessed
        size_t EntitiesProcessed = 0;                   // Number of entities/items processed
    };

    /**
     * @brief Aggregated statistics for a job type.
     */
    struct JobTypeStats {
        std::string JobName;
        
        uint64_t ExecutionCount = 0;                     // Total executions
        std::chrono::microseconds TotalExecutionTime{0}; // Sum of all execution times
        std::chrono::microseconds MinExecutionTime{std::numeric_limits<int64_t>::max()};
        std::chrono::microseconds MaxExecutionTime{0};
        std::chrono::microseconds AvgExecutionTime{0};

        uint64_t TotalWaitTime = 0;                      // Total time spent waiting
        std::chrono::microseconds AvgWaitTime{0};

        uint64_t StolenCount = 0;                        // How many times this job was stolen
        uint64_t EntitiesProcessedTotal = 0;             // Total entities processed

        // Calculate derived statistics
        void UpdateAverages() {
            if (ExecutionCount > 0) {
                AvgExecutionTime = std::chrono::microseconds(
                    TotalExecutionTime.count() / ExecutionCount
                );
                AvgWaitTime = std::chrono::microseconds(
                    TotalWaitTime / ExecutionCount
                );
            }
        }
    };

    /**
     * @brief Profiler for job system performance metrics.
     * 
     * Collects detailed performance metrics for job execution including:
     * - Job execution time
     * - Queue wait time
     * - Work stealing statistics
     * - Thread utilization
     * - Entity processing throughput
     * 
     * Can be enabled/disabled at runtime without recompilation.
     * Thread-safe for concurrent access from worker threads.
     * 
     * Example usage:
     * @code
     * JobProfiler profiler;
     * profiler.RecordJobStart(threadId, jobName);
     * // ... job execution ...
     * profiler.RecordJobEnd(threadId, executedJob);
     * 
     * auto stats = profiler.GetStats("MyJob");
     * std::cout << "Avg execution time: " << stats.AvgExecutionTime.count() << "us\n";
     * @endcode
     */
    class JobProfiler {
    public:
        /**
         * @brief Create a job profiler.
         * @param enabled Whether profiling is enabled initially
         * @param maxMetricsPerJob Maximum metrics to keep per job type (for memory)
         */
        explicit JobProfiler(bool enabled = true, size_t maxMetricsPerJob = 1000);

        ~JobProfiler() = default;

        // Non-copyable (contains mutex)
        JobProfiler(const JobProfiler&) = delete;
        JobProfiler& operator=(const JobProfiler&) = delete;

        /**
         * @brief Enable or disable profiling.
         * 
         * Disabling profiling stops metric collection without clearing existing data.
         * 
         * @param enabled true to enable, false to disable
         */
        void SetEnabled(bool enabled) { m_enabled = enabled; }

        /**
         * @brief Check if profiling is currently enabled.
         * @return true if profiling is active
         */
        bool IsEnabled() const { return m_enabled; }

        /**
         * @brief Record the start of a job execution.
         * 
         * Should be called just before job execution begins.
         * Thread-safe.
         * 
         * @param threadId The ID of the executing thread
         * @param jobName The name of the job being executed
         */
        void RecordJobStart(uint32_t threadId, const std::string& jobName);

        /**
         * @brief Record the end of a job execution.
         * 
         * Should be called immediately after job execution ends.
         * Thread-safe.
         * 
         * @param threadId The ID of the executing thread
         * @param jobName The name of the job that executed
         * @param entitiesProcessed Number of entities/items the job processed
         * @param wasStolen Whether the job was stolen from another thread
         */
        void RecordJobEnd(uint32_t threadId, const std::string& jobName,
                         size_t entitiesProcessed = 0, bool wasStolen = false);

        /**
         * @brief Record job scheduling information.
         * 
         * Should be called when a job is scheduled.
         * 
         * @param jobName The name of the job being scheduled
         * @param priority The job priority
         */
        void RecordJobScheduled(const std::string& jobName, uint32_t priority = 0);

        /**
         * @brief Get aggregated statistics for a specific job type.
         * 
         * @param jobName The name of the job type
         * @return Statistics for that job type, or empty stats if not found
         */
        JobTypeStats GetStats(const std::string& jobName) const;

        /**
         * @brief Get statistics for all job types.
         * @return Vector of statistics for all tracked job types
         */
        std::vector<JobTypeStats> GetAllStats() const;

        /**
         * @brief Get all recorded metrics.
         * 
         * For detailed analysis and visualization.
         * 
         * @return Vector of all individual job metrics
         */
        std::vector<JobMetrics> GetAllMetrics() const;

        /**
         * @brief Reset all profiling data.
         * 
         * Clears all metrics and statistics.
         */
        void Reset();

        /**
         * @brief Generate a text report of job statistics.
         * 
         * Useful for performance analysis and debugging.
         * 
         * @return Formatted string with profiling report
         */
        std::string GenerateReport() const;

        /**
         * @brief Get frame-by-frame statistics.
         * 
         * Returns statistics grouped by frame for temporal analysis.
         * 
         * @return Vector of per-frame statistics
         */
        struct FrameStats {
            uint64_t FrameNumber = 0;
            std::chrono::microseconds TotalFrameTime{0};
            std::vector<JobTypeStats> JobStats;
        };

        std::vector<FrameStats> GetFrameStats() const;

        /**
         * @brief Mark the start of a frame for statistics collection.
         */
        void MarkFrameStart();

        /**
         * @brief Mark the end of a frame and finalize statistics.
         */
        void MarkFrameEnd();

        /**
         * @brief Get current frame number.
         * @return The current frame being profiled
         */
        uint64_t GetCurrentFrame() const { return m_currentFrame; }

    private:
        /**
         * @brief Update aggregated statistics from a job metric.
         * @param metrics The metrics to aggregate
         */
        void _updateStats(const JobMetrics& metrics);

        /**
         * @brief Get or create statistics entry for a job name.
         * @param jobName The job name
         * @return Reference to the stats entry
         */
        JobTypeStats& _getOrCreateStats(const std::string& jobName);

        // Profiling state
        std::atomic<bool> m_enabled{true};

        // Metrics storage
        std::vector<JobMetrics> m_metrics;
        size_t m_maxMetricsPerJob = 0;

        // Aggregated statistics per job type
        std::unordered_map<std::string, JobTypeStats> m_stats;

        // Frame tracking
        uint64_t m_currentFrame = 0;
        std::vector<FrameStats> m_frameStats;
        std::chrono::high_resolution_clock::time_point m_frameStartTime;

        // Thread-local execution context
        thread_local static std::string t_currentJobName;
        thread_local static std::chrono::high_resolution_clock::time_point t_jobStartTime;

        // Synchronization
        mutable std::mutex m_mutex;
    };

}

#endif
