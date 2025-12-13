/* Start Header *****************************************************************/
/*!
\file    IJob.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of base job interfaces for the job system.

Defines the core job types:
- IJob: Generic job with no parallel safety
- IJobEntity: Parallel job processing individual entities
- IJobChunk: Parallel job processing chunks of entities

Each job type provides different levels of abstraction and parallelization support.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef IJOB_H
#define IJOB_H

#include <string>
#include <vector>
#include <cstdint>
#include <typeinfo>
#include "ecs/Entity.h"

namespace ECS {
    // Forward declaration
    class Chunk;

    namespace Jobs {

        /**
         * @brief Metadata for component access patterns.
         * 
         * Used to declare which components a job reads or writes,
         * enabling the dependency system to detect conflicts and
         * prevent race conditions.
         */
        enum class ComponentAccessType {
            Read,       // Read-only access to component
            Write,      // Exclusive write access to component
            ReadWrite   // Read and write access (most restrictive)
        };

        /**
         * @brief Declares access to a single component.
         */
        struct ComponentAccess {
            const std::type_info* ComponentType = nullptr;  // Component type (from typeid)
            ComponentAccessType AccessType = ComponentAccessType::Read;

            ComponentAccess() = default;
            ComponentAccess(const std::type_info& type, ComponentAccessType access)
                : ComponentType(&type), AccessType(access) {
            }
        };

        /**
         * @brief Base interface for all jobs.
         * 
         * Generic job type for non-parallel work or custom job logic.
         * Jobs inheriting from IJob run sequentially and cannot be parallelized
         * by the job system without additional synchronization.
         * 
         * Example:
         * @code
         * struct SetupJob : public IJob {
         *     void Execute() override {
         *         // Setup code here
         *     }
         *     
         *     const std::string& GetName() const override {
         *         static const std::string name = "SetupJob";
         *         return name;
         *     }
         * };
         * @endcode
         */
        class IJob {
        public:
            virtual ~IJob() = default;

            /**
             * @brief Execute the job.
             * 
             * Override this method to implement job logic.
             * For sequential jobs, this is called once on the job thread.
             */
            virtual void Execute() = 0;

            /**
             * @brief Get the name of this job for debugging and profiling.
             * @return Reference to job name string
             */
            virtual const std::string& GetName() const = 0;

            /**
             * @brief Get component access patterns for dependency resolution.
             * 
             * Override to declare which components this job reads/writes.
             * Default implementation returns empty (no dependencies).
             * 
             * @return Vector of component accesses
             */
            virtual const std::vector<ComponentAccess>& GetComponentAccesses() const {
                static const std::vector<ComponentAccess> empty;
                return empty;
            }
        };

        /**
         * @brief Base interface for entity-parallel jobs.
         * 
         * IJobEntity allows the job system to schedule job execution across
         * multiple entities in parallel. The Execute() method is called once
         * per entity by different worker threads.
         * 
         * The job system handles safe parallel iteration and component
         * access validation. Multiple IJobEntity instances can be scheduled
         * with the same archetype pattern.
         * 
         * Example:
         * @code
         * struct UpdatePositionJob : public IJobEntity {
         *     float DeltaTime;
         *     
         *     void Execute(Entity e) override {
         *         // Process entity e
         *         // Can access components via the world if needed
         *     }
         *     
         *     const std::string& GetName() const override {
         *         static const std::string name = "UpdatePosition";
         *         return name;
         *     }
         * };
         * @endcode
         */
        class IJobEntity : public IJob {
        public:
            ~IJobEntity() override = default;

            /**
             * @brief Execute job for a single entity.
             * 
             * Called by the job system for each entity matching this job's query.
             * Can be called from multiple threads in parallel for different entities.
             * 
             * @param entity The entity to process
             */
            virtual void Execute(Entity entity) { (void)entity; }

            /**
             * @brief Execute - not used for entity jobs.
             * 
             * IJobEntity overrides this with entity-specific execution.
             */
            void Execute() final override { } // This is overridden by entity-specific jobs
        };

        /**
         * @brief Base interface for chunk-parallel jobs.
         * 
         * IJobChunk provides lower-level access to chunk data for jobs that
         * need to iterate over entities within a chunk. This allows for more
         * efficient processing when you need to access multiple components
         * for the same entities.
         * 
         * The job system invokes Execute() once per chunk, and the job
         * iterates through the entities within that chunk.
         * 
         * Example:
         * @code
         * struct CalculateAIJob : public IJobChunk {
         *     void Execute(Chunk& chunk, uint32_t count) override {
         *         // Chunk contains count entities
         *         // Iterate through entities in the chunk
         *     }
         *     
         *     const std::string& GetName() const override {
         *         static const std::string name = "CalculateAI";
         *         return name;
         *     }
         * };
         * @endcode
         */
        class IJobChunk : public IJob {
        public:
            ~IJobChunk() override = default;

            /**
             * @brief Execute job for a chunk of entities.
             * 
             * Called by the job system for each chunk matching this job's query.
             * Can be called from multiple threads in parallel for different chunks.
             * 
             * @param chunk The chunk containing entities to process
             * @param count The number of valid entities in the chunk
             */
            virtual void Execute(Chunk& chunk, uint32_t count) { (void)chunk; (void)count; }

            /**
             * @brief Execute - not used for chunk jobs.
             * 
             * IJobChunk overrides this with chunk-specific execution.
             */
            void Execute() final override { } // This is overridden by chunk-specific jobs
        };

        /**
         * @brief Base interface for parallel-for jobs.
         * 
         * IJobParallelFor is used for generic parallel work over an index range,
         * not tied to specific entities or chunks. Useful for tasks like
         * batch processing, data transforms, or physics calculations.
         * 
         * Example:
         * @code
         * struct ParallelComputeJob : public IJobParallelFor {
         *     float* InputData;
         *     float* OutputData;
         *     
         *     void Execute(uint32_t index) override {
         *         OutputData[index] = ComputeValue(InputData[index]);
         *     }
         *     
         *     const std::string& GetName() const override {
         *         static const std::string name = "ParallelCompute";
         *         return name;
         *     }
         * };
         * @endcode
         */
        class IJobParallelFor : public IJob {
        public:
            ~IJobParallelFor() override = default;

            /**
             * @brief Execute job for a single index in the range.
             * 
             * Called by the job system for each index from 0 to Count-1.
             * Can be called from multiple threads in parallel for different indices.
             * 
             * @param index The index to process (0 <= index < Count)
             */
            virtual void Execute(uint32_t index) { (void)index; }

            /**
             * @brief Execute - not used for parallel-for jobs.
             */
            void Execute() final override { } // This is overridden by parallel-for jobs

            /**
             * @brief Get the total number of iterations for this job.
             * @return Number of indices to process (0 to Count-1)
             */
            virtual uint32_t GetCount() const { return 0; }
        };

    }
}

#endif
