/* Start Header *****************************************************************/
/*!
\file    WorkStealingQueue.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of WorkStealingQueue - a work-stealing queue
for efficient work distribution and load balancing in the job system.

Implements a work-stealing queue where workers can steal work from other
workers' queues when their own queue is empty, improving load balancing and
reducing idle time on multi-threaded systems.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef WORK_STEALING_QUEUE_H
#define WORK_STEALING_QUEUE_H

#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include "ecs/jobs/JobQueue.h"

namespace ECS::Jobs {

    // Forward declarations
    class IJob;
    struct WorkItem;

    /**
     * @brief Per-thread work-stealing queue for efficient load distribution.
     * 
     * Each worker thread has its own deque-based queue. Threads can:
     * - Push work to their local queue (front for LIFO scheduling)
     * - Pop from their local queue (front for cache locality)
     * - Steal from other queues (back for FIFO fairness)
     * 
     * This reduces contention on a global queue and improves cache locality
     * by allowing threads to execute work in a last-in-first-out order locally,
     * while global load balancing is maintained through work stealing.
     */
    struct PerThreadQueue {
        std::deque<WorkItem> Queue;
        std::mutex Mutex;
        std::atomic<bool> IsStolen{false};  // Flag to indicate if currently being stolen from

        PerThreadQueue() = default;
        ~PerThreadQueue() = default;

        // Non-copyable and non-movable (contains mutex)
        PerThreadQueue(const PerThreadQueue&) = delete;
        PerThreadQueue& operator=(const PerThreadQueue&) = delete;
        PerThreadQueue(PerThreadQueue&&) = delete;
        PerThreadQueue& operator=(PerThreadQueue&&) = delete;
    };

    /**
     * @brief Global work-stealing queue for distributing work to worker threads.
     * 
     * Manages per-thread queues for each worker thread. Provides:
     * - Push: Add work to a thread's queue
     * - Pop: Retrieve work from a thread's own queue
     * - Steal: Take work from another thread's queue when idle
     * 
     * Key Benefits:
     * - **Cache Locality**: Threads execute work they pushed last (LIFO cache friendly)
     * - **Load Balancing**: Idle threads steal from busy threads (back of queue for fairness)
     * - **Reduced Contention**: Per-thread locks instead of global lock
     * - **Fairness**: Work stealing from back prevents work starvation
     * 
     * Algorithm:
     * 1. Thread tries to pop from its own queue
     * 2. If empty, attempts to steal from random other thread's queue (from back)
     * 3. If all queues empty, goes to sleep or returns failure
     */
    class WorkStealingQueue {
    public:
        /**
         * @brief Initialize the work-stealing queue for a given number of threads.
         * @param numThreads Number of worker threads
         */
        explicit WorkStealingQueue(uint32_t numThreads);

        ~WorkStealingQueue();

        // Non-copyable and non-movable
        WorkStealingQueue(const WorkStealingQueue&) = delete;
        WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;
        WorkStealingQueue(WorkStealingQueue&&) = delete;
        WorkStealingQueue& operator=(WorkStealingQueue&&) = delete;

        /**
         * @brief Push work to a specific thread's queue.
         * 
         * Adds work to the front of a worker thread's queue for LIFO execution.
         * This is typically called when scheduling jobs for a specific thread
         * or during frame processing.
         * 
         * Thread-safe. Can be called from any thread.
         * 
         * @param threadIndex Index of the worker thread (0 to numThreads-1)
         * @param item The work item to push
         */
        void Push(uint32_t threadIndex, WorkItem item);

        /**
         * @brief Pop work from a thread's own queue (front for LIFO).
         * 
         * Retrieves work from the front of the specified thread's queue.
         * This provides cache locality (thread executes its own pushed work).
         * 
         * Thread-safe. Should be called from the corresponding worker thread.
         * 
         * @param threadIndex Index of the worker thread
         * @param outItem Output parameter for the retrieved work item
         * @return true if work was retrieved, false if queue is empty
         */
        bool Pop(uint32_t threadIndex, WorkItem& outItem);

        /**
         * @brief Steal work from another thread's queue (back for fairness).
         * 
         * Called when a thread's own queue is empty. Attempts to steal work
         * from another thread's queue, taking from the back for fairness.
         * 
         * Thread-safe. Can be called from any worker thread.
         * 
         * @param stealingThreadIndex Index of the thread that is stealing
         * @param outItem Output parameter for the stolen work item
         * @return true if work was stolen, false if all queues are empty
         */
        bool Steal(uint32_t stealingThreadIndex, WorkItem& outItem);

        /**
         * @brief Try to get work - pop from own queue first, then try stealing.
         * 
         * Convenience method that attempts to pop from the thread's own queue,
         * and if that fails, tries stealing from other threads.
         * 
         * @param threadIndex Index of the worker thread
         * @param outItem Output parameter for the work item
         * @return true if work was retrieved (either popped or stolen)
         */
        bool TryGetWork(uint32_t threadIndex, WorkItem& outItem);

        /**
         * @brief Get the total number of pending work items.
         * 
         * Provides approximate count of work across all queues.
         * May not be 100% accurate due to concurrent access, but good enough
         * for statistics and profiling.
         * 
         * @return Total count of work items in all queues
         */
        size_t GetPendingWorkCount() const;

        /**
         * @brief Check if all queues are empty.
         * 
         * @return true if no work is available in any queue
         */
        bool IsEmpty() const;

        /**
         * @brief Shutdown the queue (optional, for explicit cleanup).
         * 
         * Called during JobManager shutdown to ensure clean teardown.
         */
        void Shutdown();

    private:
        /**
         * @brief Select a random thread to steal from (not including stealer).
         * @param excludeIndex The thread index to exclude (the stealer)
         * @return Random thread index
         */
        uint32_t _selectRandomThread(uint32_t excludeIndex) const;

        /**
         * @brief Attempt to steal from a specific thread.
         * @param victimIndex The thread to steal from
         * @param outItem Output for the stolen item
         * @return true if successful
         */
        bool _stealFromThread(uint32_t victimIndex, WorkItem& outItem);

        // Per-thread queues for work stealing
        std::vector<std::unique_ptr<PerThreadQueue>> m_queues;

        // Total number of threads
        uint32_t m_numThreads = 0;

        // Whether queue is shut down
        std::atomic<bool> m_shutdown{false};

        // For random thread selection in stealing
        mutable std::mutex m_randomMutex;
    };

}

#endif
