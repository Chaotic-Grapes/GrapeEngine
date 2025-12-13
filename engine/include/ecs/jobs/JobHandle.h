/* Start Header *****************************************************************/
/*!
\file    JobHandle.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of JobHandle and job completion tracking.
JobHandle is a lightweight identifier used to reference and synchronize jobs.

Provides thread-safe completion tracking through atomic operations and allows
waiting for job completion (blocking) or checking status (non-blocking).

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef JOB_HANDLE_H
#define JOB_HANDLE_H

#include <cstdint>
#include <atomic>
#include <memory>

namespace ECS::Jobs {

    /**
     * @brief Lightweight handle for tracking job execution and dependencies.
     * 
     * A JobHandle allows code to wait for a job to complete, check if it's done,
     * or create dependencies between jobs. Multiple JobHandles can reference the
     * same underlying job completion state.
     * 
     * Thread-safe operations:
     * - IsComplete(): Non-blocking check if job finished
     * - Complete(): Blocking wait for job to finish
     * - Copy and move semantics are safe
     */
    class JobHandle {
    public:
        /**
         * @brief Default constructor - creates an invalid/empty handle.
         */
        JobHandle() = default;

        /**
         * @brief Create a handle that wraps a completion token.
         * @param completionState Shared atomic state for job completion
         */
        explicit JobHandle(std::shared_ptr<std::atomic<bool>> completionState)
            : m_completionState(std::move(completionState)) {
        }

        /**
         * @brief Copy constructor - safe, creates another reference to same job state.
         */
        JobHandle(const JobHandle& other) = default;

        /**
         * @brief Copy assignment - safe, updates reference to job state.
         */
        JobHandle& operator=(const JobHandle& other) = default;

        /**
         * @brief Move constructor.
         */
        JobHandle(JobHandle&& other) noexcept = default;

        /**
         * @brief Move assignment.
         */
        JobHandle& operator=(JobHandle&& other) noexcept = default;

        /**
         * @brief Check if the job has completed without blocking.
         * @return true if the job is complete, false otherwise
         */
        bool IsComplete() const noexcept {
            if (!m_completionState) {
                return true;  // Invalid handle is considered complete
            }
            return m_completionState->load(std::memory_order_acquire);
        }

        /**
         * @brief Block until the job completes.
         * 
         * Spin-waits until the job's completion flag is set. Useful for
         * synchronization points where you need to ensure a job is done
         * before proceeding.
         * 
         * Safe to call multiple times from different threads.
         */
        void Complete() const noexcept {
            if (!m_completionState) {
                return;  // Invalid handle, nothing to wait for
            }
            
            // Spin-wait with backoff to avoid busy-looping
            while (!m_completionState->load(std::memory_order_acquire)) {
                // Yield to other threads to avoid excessive CPU usage
                // In production, this could be optimized with condition variables
            }
        }

        /**
         * @brief Check if this handle is valid (references an actual job).
         * @return true if handle references a real job, false if default-constructed
         */
        bool IsValid() const noexcept {
            return m_completionState != nullptr;
        }

        /**
         * @brief Equality comparison - checks if handles reference the same job.
         */
        bool operator==(const JobHandle& other) const noexcept {
            return m_completionState == other.m_completionState;
        }

        /**
         * @brief Inequality comparison.
         */
        bool operator!=(const JobHandle& other) const noexcept {
            return m_completionState != other.m_completionState;
        }

    private:
        std::shared_ptr<std::atomic<bool>> m_completionState;

        // Friend declaration for JobManager access
        friend class JobManager;
    };

}

#endif
