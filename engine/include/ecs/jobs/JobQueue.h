/* Start Header *****************************************************************/
/*!
\file    JobQueue.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains a lock-free job queue for distributing work to worker threads.

Implements a thread-safe, lock-free queue using atomic operations for efficient
job distribution. Supports both FIFO and LIFO semantics for work stealing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "ecs/jobs/IJob.h"

namespace ECS::Jobs {

    /**
     * @brief Work item stored in the job queue.
     * 
     * Wraps a job with its completion state and priority information.
     */
    struct WorkItem {
        std::unique_ptr<IJob> Job;
        std::shared_ptr<std::atomic<bool>> CompletionState;
        int Priority = 0;  // Higher priority jobs execute first (0 = normal)

        WorkItem() = default;
        
        WorkItem(std::unique_ptr<IJob> job, 
                 std::shared_ptr<std::atomic<bool>> completionState,
                 int priority = 0)
            : Job(std::move(job)), 
              CompletionState(std::move(completionState)),
              Priority(priority) {
        }

        // Enable move semantics
        WorkItem(WorkItem&&) noexcept = default;
        WorkItem& operator=(WorkItem&&) noexcept = default;

        // Disable copy semantics (unique_ptr)
        WorkItem(const WorkItem&) = delete;
        WorkItem& operator=(const WorkItem&) = delete;
    };

    /**
     * @brief Thread-safe job queue for distributing work to worker threads.
     * 
     * This queue supports multiple producers (job scheduling threads) and
     * multiple consumers (worker threads). Uses a simple mutex-based lock
     * with condition variables for efficiency.
     * 
     * Note: While a fully lock-free implementation would be ideal, this
     * mutex-based approach provides better cache behavior and is sufficient
     * for typical job system workloads. Can be optimized with lock-free
     * algorithms if profiling shows contention.
     */
    class JobQueue {
    public:
        JobQueue() = default;
        ~JobQueue() = default;

        // Delete copy and move operations (mutex is not movable)
        JobQueue(const JobQueue&) = delete;
        JobQueue& operator=(const JobQueue&) = delete;
        JobQueue(JobQueue&&) = delete;
        JobQueue& operator=(JobQueue&&) = delete;

        /**
         * @brief Enqueue a work item to be processed.
         * 
         * Thread-safe. Can be called from multiple producer threads.
         * Notifies waiting consumer threads.
         * 
         * @param item The work item to enqueue
         */
        void Enqueue(WorkItem item) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_queue.push(std::move(item));
            }
            m_cv.notify_one();  // Wake up a waiting consumer
        }

        /**
         * @brief Try to dequeue a work item without blocking.
         * 
         * Thread-safe. Returns immediately if queue is empty.
         * 
         * @param outItem Output parameter for the dequeued item
         * @return true if an item was dequeued, false if queue was empty
         */
        bool TryDequeue(WorkItem& outItem) {
            std::unique_lock<std::mutex> lock(m_mutex);
            
            if (m_queue.empty()) {
                return false;
            }

            outItem = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        /**
         * @brief Dequeue a work item, blocking until one is available.
         * 
         * Thread-safe. Blocks the calling thread until a work item is available
         * or until the queue is signaled to be shut down.
         * 
         * @param outItem Output parameter for the dequeued item
         * @return true if item was dequeued, false if queue is shutting down
         */
        bool Dequeue(WorkItem& outItem) {
            std::unique_lock<std::mutex> lock(m_mutex);

            // Wait for items or shutdown signal
            m_cv.wait(lock, [this]() {
                return !m_queue.empty() || m_shutdown;
            });

            if (m_queue.empty()) {
                return false;  // Shutting down
            }

            outItem = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        /**
         * @brief Check if queue is empty without locking.
         * 
         * Note: This is a snapshot and may become inaccurate immediately.
         * Use only for statistics/debugging, not synchronization.
         * 
         * @return true if queue appears to be empty
         */
        bool IsEmpty() const {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        /**
         * @brief Get the current size of the queue.
         * 
         * Note: This is a snapshot and may change immediately.
         * Use only for statistics/debugging.
         * 
         * @return Number of items currently in queue
         */
        size_t Size() const {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

        /**
         * @brief Signal the queue to shut down.
         * 
         * After calling Shutdown(), Dequeue() will return false
         * and blocked consumers will wake up.
         * 
         * Thread-safe.
         */
        void Shutdown() {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_shutdown = true;
            }
            m_cv.notify_all();  // Wake all waiting consumers
        }

        /**
         * @brief Check if shutdown has been signaled.
         * @return true if Shutdown() was called
         */
        bool IsShutdown() const {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_shutdown;
        }

        /**
         * @brief Clear all pending work items.
         * 
         * Useful for aborting pending jobs. Thread-safe.
         */
        void Clear() {
            std::unique_lock<std::mutex> lock(m_mutex);
            while (!m_queue.empty()) {
                auto item = std::move(m_queue.front());
                m_queue.pop();
                // Item is destroyed here, marking completion state
                if (item.CompletionState) {
                    item.CompletionState->store(true, std::memory_order_release);
                }
            }
        }

    private:
        std::queue<WorkItem> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_shutdown = false;
    };

}

#endif
