/* Start Header *****************************************************************/
/*!
\file    JobProfiler.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the implementation of JobProfiler - job profiling and
metrics collection for performance analysis.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/jobs/JobProfiler.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace ECS::Jobs {

    // Thread-local storage for current job context
    thread_local std::string JobProfiler::t_currentJobName;
    thread_local std::chrono::high_resolution_clock::time_point JobProfiler::t_jobStartTime;

    JobProfiler::JobProfiler(bool enabled, size_t maxMetricsPerJob)
        : m_enabled(enabled), m_maxMetricsPerJob(maxMetricsPerJob), m_currentFrame(0) {
        
        m_frameStartTime = std::chrono::high_resolution_clock::now();
    }

    void JobProfiler::RecordJobStart(uint32_t threadId, const std::string& jobName) {
        if (!m_enabled) {
            return;
        }

        t_currentJobName = jobName;
        t_jobStartTime = std::chrono::high_resolution_clock::now();
    }

    void JobProfiler::RecordJobEnd(uint32_t threadId, const std::string& jobName,
                                   size_t entitiesProcessed, bool wasStolen) {
        if (!m_enabled) {
            return;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto executionTime = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - t_jobStartTime
        );

        // Create metrics entry
        JobMetrics metrics;
        metrics.JobName = jobName;
        metrics.ExecutingThreadId = threadId;
        metrics.ExecutionTime = executionTime;
        metrics.EndTime = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime.time_since_epoch()
        );
        metrics.StartTime = std::chrono::duration_cast<std::chrono::microseconds>(
            t_jobStartTime.time_since_epoch()
        );
        metrics.EntitiesProcessed = entitiesProcessed;
        metrics.WasStolen = wasStolen;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            // Store metric
            if (m_metrics.size() < m_maxMetricsPerJob * m_stats.size() + m_maxMetricsPerJob) {
                m_metrics.push_back(metrics);
            }
            else {
                // Circular buffer: remove oldest if at capacity
                if (m_metrics.size() > 0) {
                    m_metrics.erase(m_metrics.begin());
                }
                m_metrics.push_back(metrics);
            }

            // Update statistics
            _updateStats(metrics);
        }
    }

    void JobProfiler::RecordJobScheduled(const std::string& jobName, uint32_t priority) {
        if (!m_enabled) {
            return;
        }

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            _getOrCreateStats(jobName);
        }
    }

    JobTypeStats JobProfiler::GetStats(const std::string& jobName) const {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        auto it = m_stats.find(jobName);
        if (it != m_stats.end()) {
            return it->second;
        }
        
        return JobTypeStats();
    }

    std::vector<JobTypeStats> JobProfiler::GetAllStats() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        std::vector<JobTypeStats> result;
        for (const auto& pair : m_stats) {
            result.push_back(pair.second);
        }
        
        // Sort by name for consistent output
        std::sort(result.begin(), result.end(),
                 [](const JobTypeStats& a, const JobTypeStats& b) {
                     return a.JobName < b.JobName;
                 });
        
        return result;
    }

    std::vector<JobMetrics> JobProfiler::GetAllMetrics() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_metrics;
    }

    void JobProfiler::Reset() {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        m_metrics.clear();
        m_stats.clear();
        m_frameStats.clear();
        m_currentFrame = 0;
        m_frameStartTime = std::chrono::high_resolution_clock::now();
    }

    std::string JobProfiler::GenerateReport() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        
        std::ostringstream oss;
        oss << "=== Job Profiler Report ===\n";
        oss << "Total Metrics Recorded: " << m_metrics.size() << "\n";
        oss << "Total Job Types: " << m_stats.size() << "\n\n";

        oss << std::left << std::setw(30) << "Job Name"
            << std::setw(15) << "Count"
            << std::setw(15) << "Avg Time (us)"
            << std::setw(15) << "Min Time (us)"
            << std::setw(15) << "Max Time (us)"
            << std::setw(15) << "Stolen"
            << "\n";
        oss << std::string(105, '-') << "\n";

        for (const auto& pair : m_stats) {
            const auto& stats = pair.second;
            oss << std::left << std::setw(30) << stats.JobName
                << std::setw(15) << stats.ExecutionCount
                << std::setw(15) << stats.AvgExecutionTime.count()
                << std::setw(15) << stats.MinExecutionTime.count()
                << std::setw(15) << stats.MaxExecutionTime.count()
                << std::setw(15) << stats.StolenCount
                << "\n";
        }

        return oss.str();
    }

    std::vector<JobProfiler::FrameStats> JobProfiler::GetFrameStats() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_frameStats;
    }

    void JobProfiler::MarkFrameStart() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_frameStartTime = std::chrono::high_resolution_clock::now();
    }

    void JobProfiler::MarkFrameEnd() {
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::microseconds>(
            frameEndTime - m_frameStartTime
        );

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            FrameStats frameStats;
            frameStats.FrameNumber = m_currentFrame;
            frameStats.TotalFrameTime = frameDuration;

            // Collect stats for this frame
            for (const auto& pair : m_stats) {
                frameStats.JobStats.push_back(pair.second);
            }

            m_frameStats.push_back(frameStats);
            ++m_currentFrame;
        }
    }

    void JobProfiler::_updateStats(const JobMetrics& metrics) {
        auto& stats = _getOrCreateStats(metrics.JobName);

        // Update execution count and time
        ++stats.ExecutionCount;
        stats.TotalExecutionTime += metrics.ExecutionTime;

        // Update min/max
        if (metrics.ExecutionTime < stats.MinExecutionTime) {
            stats.MinExecutionTime = metrics.ExecutionTime;
        }
        if (metrics.ExecutionTime > stats.MaxExecutionTime) {
            stats.MaxExecutionTime = metrics.ExecutionTime;
        }

        // Update wait time
        if (metrics.WaitTime.count() > 0) {
            stats.TotalWaitTime += metrics.WaitTime.count();
        }

        // Update stolen count
        if (metrics.WasStolen) {
            ++stats.StolenCount;
        }

        // Update entities processed
        stats.EntitiesProcessedTotal += metrics.EntitiesProcessed;

        // Recalculate averages
        stats.UpdateAverages();
    }

    JobTypeStats& JobProfiler::_getOrCreateStats(const std::string& jobName) {
        auto it = m_stats.find(jobName);
        
        if (it != m_stats.end()) {
            return it->second;
        }

        // Create new stats entry
        JobTypeStats newStats;
        newStats.JobName = jobName;
        newStats.MinExecutionTime = std::chrono::microseconds{std::numeric_limits<int64_t>::max()};

        auto& inserted = m_stats.emplace(jobName, newStats).first->second;
        return inserted;
    }

}
