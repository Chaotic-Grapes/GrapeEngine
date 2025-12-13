/* Start Header *****************************************************************/
/*!
\file    WorkStealingQueue.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the implementation of WorkStealingQueue - work-stealing queue
for efficient job distribution and load balancing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/jobs/WorkStealingQueue.h"
#include "ecs/jobs/JobQueue.h"
#include <random>
#include <algorithm>

namespace ECS::Jobs {

    WorkStealingQueue::WorkStealingQueue(uint32_t numThreads)
        : m_numThreads(numThreads) {
        
        // Create a per-thread queue for each worker thread
        for (uint32_t i = 0; i < numThreads; ++i) {
            m_queues.push_back(std::make_unique<PerThreadQueue>());
        }
    }

    WorkStealingQueue::~WorkStealingQueue() {
        Shutdown();
    }

    void WorkStealingQueue::Push(uint32_t threadIndex, WorkItem item) {
        if (m_shutdown || threadIndex >= m_numThreads) {
            return;
        }

        // Push to the front of the target thread's queue (LIFO order)
        auto& queue = m_queues[threadIndex];
        {
            std::unique_lock<std::mutex> lock(queue->Mutex);
            queue->Queue.push_front(std::move(item));
        }
    }

    bool WorkStealingQueue::Pop(uint32_t threadIndex, WorkItem& outItem) {
        if (m_shutdown || threadIndex >= m_numThreads) {
            return false;
        }

        auto& queue = m_queues[threadIndex];
        {
            std::unique_lock<std::mutex> lock(queue->Mutex);
            
            if (queue->Queue.empty()) {
                return false;
            }

            // Pop from front (LIFO - most recently pushed item, cache locality)
            outItem = std::move(queue->Queue.front());
            queue->Queue.pop_front();
            return true;
        }
    }

    bool WorkStealingQueue::Steal(uint32_t stealingThreadIndex, WorkItem& outItem) {
        if (m_shutdown || stealingThreadIndex >= m_numThreads) {
            return false;
        }

        // Try to steal from a random other thread
        // Attempt up to numThreads iterations to find work
        for (uint32_t attempt = 0; attempt < m_numThreads; ++attempt) {
            uint32_t victimIndex = _selectRandomThread(stealingThreadIndex);
            
            if (_stealFromThread(victimIndex, outItem)) {
                return true;
            }
        }

        return false;
    }

    bool WorkStealingQueue::TryGetWork(uint32_t threadIndex, WorkItem& outItem) {
        // First try to pop from own queue
        if (Pop(threadIndex, outItem)) {
            return true;
        }

        // If that fails, try to steal from others
        return Steal(threadIndex, outItem);
    }

    size_t WorkStealingQueue::GetPendingWorkCount() const {
        size_t total = 0;

        for (const auto& queue : m_queues) {
            {
                std::unique_lock<std::mutex> lock(queue->Mutex);
                total += queue->Queue.size();
            }
        }

        return total;
    }

    bool WorkStealingQueue::IsEmpty() const {
        for (const auto& queue : m_queues) {
            {
                std::unique_lock<std::mutex> lock(queue->Mutex);
                if (!queue->Queue.empty()) {
                    return false;
                }
            }
        }
        return true;
    }

    void WorkStealingQueue::Shutdown() {
        m_shutdown = true;
        
        // Clear all queues
        for (auto& queue : m_queues) {
            {
                std::unique_lock<std::mutex> lock(queue->Mutex);
                queue->Queue.clear();
            }
        }
    }

    uint32_t WorkStealingQueue::_selectRandomThread(uint32_t excludeIndex) const {
        if (m_numThreads <= 1) {
            return 0;
        }

        // Thread-safe random number generation
        std::unique_lock<std::mutex> lock(m_randomMutex);
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        std::uniform_int_distribution<> dis(0, m_numThreads - 2);
        uint32_t index = dis(gen);
        
        // Skip the exclude index
        if (index >= excludeIndex) {
            index++;
        }
        
        return index;
    }

    bool WorkStealingQueue::_stealFromThread(uint32_t victimIndex, WorkItem& outItem) {
        if (victimIndex >= m_numThreads) {
            return false;
        }

        auto& queue = m_queues[victimIndex];
        {
            std::unique_lock<std::mutex> lock(queue->Mutex);
            
            if (queue->Queue.empty()) {
                return false;
            }

            // Steal from back (FIFO - oldest item, fairness)
            // This ensures items don't starvate and maintains fairness
            outItem = std::move(queue->Queue.back());
            queue->Queue.pop_back();
            
            // Mark that work was stolen
            outItem.Priority++;  // Could use a better flag, but priority works
            
            return true;
        }
    }

}
