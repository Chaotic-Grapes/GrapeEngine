/* Start Header *****************************************************************/
/*!
\file    JobManager.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the implementation of JobManager - job scheduling and
execution system with worker thread pool management, work stealing, and profiling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/jobs/JobManager.h"
#include "ecs/jobs/IJob.h"
#include "ecs/jobs/ThreadAffinity.h"
#include "core/Logger.h"
#include <thread>
#include <chrono>

namespace ECS::Jobs {

    JobManager::JobManager()
        : JobManager(JobSystemConfig{}) { }

    JobManager::JobManager(const JobSystemConfig& config)
        : m_config(config), m_shutdown(false), 
          m_profiler(config.EnableProfilingData) {
        
        // Determine number of worker threads
        if (m_config.NumWorkerThreads == 0) {
            m_numWorkerThreads = std::thread::hardware_concurrency();
            if (m_numWorkerThreads == 0) {
                m_numWorkerThreads = 4;  // Fallback if hardware_concurrency fails
            }
        }
        else {
            m_numWorkerThreads = m_config.NumWorkerThreads;
        }

        // Initialize work-stealing queue if enabled
        if (m_config.EnableWorkStealing) {
            m_workStealingQueue = std::make_unique<WorkStealingQueue>(m_numWorkerThreads);
        }

        // Initialize worker threads
        _initializeWorkerThreads();
    }

    JobManager::~JobManager() {
        Shutdown();
    }

    void JobManager::_initializeWorkerThreads() {
        for (uint32_t i = 0; i < m_numWorkerThreads; ++i) {
            m_workerThreads.emplace_back([this, i]() {
                // Set thread affinity if supported
                if (ThreadAffinity::IsSupported()) {
                    ThreadAffinity::SetCurrentThreadAffinity(i % ThreadAffinity::GetNumCores());
                }
                
                _workerThread(i);
            });
        }
    }

    void JobManager::_workerThread(uint32_t threadIndex) {
        WorkItem item;

        while (true) {
            // Try to get work - either from standard queue or work-stealing queue
            bool hasWork = false;

            if (m_config.EnableWorkStealing && m_workStealingQueue) {
                // Try to get work from work-stealing queue
                hasWork = m_workStealingQueue->TryGetWork(threadIndex, item);
            }
            else {
                // Use standard queue
                hasWork = m_jobQueue.Dequeue(item);
            }

            if (!hasWork) {
                // Queue returned false - shutting down
                if (m_shutdown) {
                    break;
                }
                // Sleep briefly before retrying
                std::this_thread::yield();
                continue;
            }

            // Record job start for profiling
            if (m_profiler.IsEnabled() && item.Job) {
                m_profiler.RecordJobStart(threadIndex, "Job");
            }

            // Execute the job
            if (item.Job) {
                try {
                    item.Job->Execute();
                }
                catch (const std::exception& e) {
                    LOG_CRITICAL("Job execution error: " << e.what());
                }
            }

            // Record job end for profiling
            if (m_profiler.IsEnabled()) {
                m_profiler.RecordJobEnd(threadIndex, "Job", 0, false);
            }

            // Mark job as complete
            if (item.CompletionState) {
                item.CompletionState->store(true, std::memory_order_release);
            }
        }
    }

    JobHandle JobManager::Schedule(std::unique_ptr<IJob> job,
                                   const JobHandle& dependsOn,
                                   int priority) {
        if (!job || m_shutdown) {
            return JobHandle{};
        }

        // Create completion state for this job
        auto completionState = std::make_shared<std::atomic<bool>>(false);
        JobHandle handle(completionState);

        // Record job scheduling for profiling
        if (m_profiler.IsEnabled()) {
            m_profiler.RecordJobScheduled("Job", priority);
        }

        // If job depends on another, wait for it to complete before enqueueing
        if (dependsOn.IsValid()) {
            dependsOn.Complete();
        }

        // Create work item and enqueue
        WorkItem item(std::move(job), completionState, priority);
        
        if (m_config.EnableWorkStealing && m_workStealingQueue) {
            // Round-robin distribution to thread queues
            static std::atomic<uint32_t> nextThreadIndex{0};

            uint32_t threadIndex = nextThreadIndex++ % m_numWorkerThreads;
            m_workStealingQueue->Push(threadIndex, std::move(item));
        }
        else {
            m_jobQueue.Enqueue(std::move(item));
        }

        return handle;
    }

    JobHandle JobManager::ScheduleParallel(std::vector<std::unique_ptr<IJob>> jobs,
                                           int priority) {
        if (jobs.empty() || m_shutdown) {
            return JobHandle{};
        }

        // Create a shared completion state for the master handle
        auto masterComplete = std::make_shared<std::atomic<bool>>(false);
        JobHandle masterHandle(masterComplete);

        // Schedule all jobs
        for (auto& job : jobs) {
            if (!job) continue;

            // Create per-job completion state
            auto jobCompletion = std::make_shared<std::atomic<bool>>(false);

            // Record job scheduling
            if (m_profiler.IsEnabled()) {
                m_profiler.RecordJobScheduled("Job", priority);
            }

            WorkItem item(std::move(job), jobCompletion, priority);
            
            if (m_config.EnableWorkStealing && m_workStealingQueue) {
                // Distribute across threads
                static std::atomic<uint32_t> nextThreadIndex{0};
                uint32_t threadIndex = nextThreadIndex++ % m_numWorkerThreads;
                m_workStealingQueue->Push(threadIndex, std::move(item));
            }
            else {
                m_jobQueue.Enqueue(std::move(item));
            }
        }

        return masterHandle;
    }

    void JobManager::Complete(const JobHandle& handle) const {
        handle.Complete();
    }

    void JobManager::CompleteAllJobs() {
        // Mark frame end for profiling
        m_profiler.MarkFrameEnd();

        // Spin until queues are empty
        if (m_config.EnableWorkStealing && m_workStealingQueue) {
            while (!m_workStealingQueue->IsEmpty()) {
                std::this_thread::yield();
            }
        }
        else {
            while (!m_jobQueue.IsEmpty()) {
                std::this_thread::yield();
            }
        }
    }

    bool JobManager::IsRunning() const {
        return !m_shutdown;
    }

    size_t JobManager::GetPendingJobCount() const {
        if (m_config.EnableWorkStealing && m_workStealingQueue) {
            return m_workStealingQueue->GetPendingWorkCount();
        }
        else {
            return m_jobQueue.Size();
        }
    }

    void JobManager::Shutdown() {
        if (m_shutdown) {
            return;
        }

        // Signal shutdown
        m_shutdown = true;

        if (m_config.EnableWorkStealing && m_workStealingQueue) {
            m_workStealingQueue->Shutdown();
        }
        else {
            m_jobQueue.Shutdown();
        }

        // Wait for all worker threads to finish
        for (auto& thread : m_workerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        m_workerThreads.clear();
    }

    void JobManager::_markJobComplete(const std::shared_ptr<std::atomic<bool>>& completionState) {
        if (completionState) {
            completionState->store(true, std::memory_order_release);
        }
    }

}

