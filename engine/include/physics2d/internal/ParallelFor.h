/* Start Header *****************************************************************/
/*!
\file   ParallelFor.h
\author Dalton Koh Shi Hao (100%)
\par    d.koh@digipen.edu
\brief  Deterministic static-partition parallel-for helper for physics stages.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_INTERNAL_PARALLELFOR_H
#define ENGINE_PHYSICS2D_INTERNAL_PARALLELFOR_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Engine::Physics2D::Internal {
    namespace Detail {
        class StaticWorkerPool {
        public:
            StaticWorkerPool() = default;

            ~StaticWorkerPool() {
                {
                    // Signal workers to stop and join them before destruction.
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_stopping = true;
                    ++m_epoch;
                }
                // Notify all workers to wake up and exit.
                m_startCv.notify_all();
                for (auto& worker : m_workers) {
                    if (worker.joinable()) {
                        worker.join(); // Wait for worker thread to finish.
                    }
                }
            }

            /**
             * @brief Execute the provided function in parallel across [0,count) with static partitioning.
             * @param count Total number of iterations to process.
             * @param maxWorkers Maximum number of worker threads to utilize (including the calling thread).
             * @param fn Function to execute for each partition, called with (begin, end, workerIndex).
             * Worker index 0 is reserved for the calling thread, and indices [1, maxWorkers) are for worker threads.
             */
            void Dispatch(
                size_t count,
                uint32_t usedWorkers,
                const std::function<void(size_t, size_t, uint32_t)>& fn)
            {
                // If only one worker is used, execute synchronously on the calling thread to avoid overhead.
                if (usedWorkers <= 1u) {
                    fn(0, count, 0u);
                    return;
                }

                // Ensure enough worker threads are available for the requested parallelism level.
                const uint32_t asyncWorkers = usedWorkers - 1u;
                EnsureWorkers(asyncWorkers);

                // Compute static partitioning of [0,count) into contiguous subranges for each worker.
                const size_t base = count / usedWorkers;
                const size_t rem = count % usedWorkers;

                // First `rem` workers get `base + 1` iterations, the rest get `base` iterations.
                std::vector<size_t> begins(usedWorkers, 0u);
                std::vector<size_t> ends(usedWorkers, 0u);
                size_t begin = 0u;
                for (uint32_t w = 0; w < usedWorkers; ++w) {
                    // Compute the span for this worker, accounting for any remainder.
                    const size_t span = base + (w < rem ? 1u : 0u);
                    begins[w] = begin;
                    ends[w] = begin + span;
                    begin = ends[w];
                }

                // Set up shared state for workers and notify them to start processing.
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_fn = &fn;
                    m_activeWorkers = asyncWorkers;
                    m_completedWorkers = 0u;

                    // Update partition info for each worker thread.
                    for (uint32_t idx = 1u; idx <= asyncWorkers; ++idx) {
                        m_begin[idx] = begins[idx];
                        m_end[idx] = ends[idx];
                    }
                    ++m_epoch; // Increment epoch to signal workers of new york.
                }

                // Notify worker threads to start processing their assigned partitions.
                m_startCv.notify_all();
                fn(begins[0], ends[0], 0u);

                // Wait for all worker threads to complete their tasks before returning.
                std::unique_lock<std::mutex> lock(m_mutex);
                m_doneCv.wait(lock, [this]() {
                    return m_completedWorkers >= m_activeWorkers;
                    });
            }

        private:
            // Ensure that the pool has at least `targetAsyncWorkers` worker threads available, creating them if necessary.
            void EnsureWorkers(uint32_t targetAsyncWorkers) {
                // Worker index 0 is reserved for the calling thread, so we need `targetAsyncWorkers` additional threads.
                while (static_cast<uint32_t>(m_workers.size()) < targetAsyncWorkers) {
                    // Worker indices start from 1 since 0 is the calling thread.
                    const uint32_t workerIndex = static_cast<uint32_t>(m_workers.size()) + 1u;
                    m_begin.push_back(0u);
                    m_end.push_back(0u);
                    m_workers.emplace_back([this, workerIndex]() {
                        WorkerLoop(workerIndex);
                        });
                }
            }

            // Main loop for worker threads, waiting for tasks and executing assigned partitions when signaled.
            void WorkerLoop(uint32_t workerIndex) {
                // Each worker thread waits for a signal to start processing, executes its assigned partition, and then signals completion.
                size_t localEpoch = 0u;

                // Worker loop runs indefinitely until the pool is destructed, at which point `m_stopping` will be set to true and workers will exit.
                for (;;) {
                    // Wait for a signal to start processing, checking for stop condition or epoch change.
                    const std::function<void(size_t, size_t, uint32_t)>* fn = nullptr;
                    size_t begin = 0u;
                    size_t end = 0u;
                    {
                        // Wait for the main thread to signal a new task or to stop the worker.
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_startCv.wait(lock, [this, localEpoch]() {
                            return m_stopping || m_epoch != localEpoch;
                            });

                        // If stopping, exit the loop and end the thread.
                        if (m_stopping) {
                            return;
                        }

                        // If the epoch has changed, this means a new task has been assigned. Update local epoch and retrieve the function and partition info for this worker.
                        localEpoch = m_epoch;
                        if (workerIndex > m_activeWorkers) {
                            continue;
                        }

                        fn = m_fn;
                        begin = m_begin[workerIndex];
                        end = m_end[workerIndex];
                    }

                    // Execute the assigned partition of the task using the provided function, passing the worker index for potential use in the task.
                    (*fn)(begin, end, workerIndex);

                    {
                        // After completing the task, update the shared state to indicate this worker has finished, and if all workers are done, notify the main thread.
                        std::lock_guard<std::mutex> lock(m_mutex);
                        ++m_completedWorkers;
                        if (m_completedWorkers >= m_activeWorkers) {
                            m_doneCv.notify_one();
                        }
                    }
                }
            }

            std::mutex m_mutex;
            std::condition_variable m_startCv;
            std::condition_variable m_doneCv;
            const std::function<void(size_t, size_t, uint32_t)>* m_fn = nullptr;
            std::vector<std::thread> m_workers;
            std::vector<size_t> m_begin{ 0u };
            std::vector<size_t> m_end{ 0u };
            uint32_t m_activeWorkers = 0u;
            uint32_t m_completedWorkers = 0u;
            size_t m_epoch = 0u;
            bool m_stopping = false;
        };

        // Singleton accessor for the static worker pool instance used by ParallelForStatic.
        inline StaticWorkerPool& GetStaticWorkerPool() {
            static StaticWorkerPool pool;
            return pool;
        }
    }

    /**
     * @brief Execute [0,count) with static contiguous partitions for deterministic task assignment.
     * @param count Total iteration count.
     * @param maxWorkers Upper bound for worker threads.
     * @param fn Callback invoked with (begin,end,workerIndex).
     *
     * Static partitioning avoids dynamic work-stealing nondeterminism in task ownership.
     */
    inline void ParallelForStatic(
        size_t count,
        uint32_t maxWorkers,
        const std::function<void(size_t, size_t, uint32_t)>& fn)
    {
        if (count == 0) {
            return;
        }

        const uint32_t hw = std::max<uint32_t>(1u, std::thread::hardware_concurrency());
        const uint32_t workers = std::max<uint32_t>(1u, std::min<uint32_t>(maxWorkers, hw));
        const uint32_t usedWorkers = static_cast<uint32_t>(std::min<size_t>(workers, count));
        Detail::GetStaticWorkerPool().Dispatch(count, usedWorkers, fn);
    }
}

#endif
