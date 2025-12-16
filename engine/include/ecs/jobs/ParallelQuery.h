/* Start Header *****************************************************************/
/*!
\file    ParallelQuery.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of ParallelQuery - a template for safely
iterating over entities and chunks in parallel with component access validation.

Provides type-safe query construction with automatic chunk filtering and
entity iteration, enabling safe parallel job scheduling.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef PARALLEL_QUERY_H
#define PARALLEL_QUERY_H

#include <vector>
#include <memory>
#include <functional>
#include <typeinfo>
#include "ecs/Signature.h"
#include "ecs/Chunk.h"
#include "ecs/jobs/JobHandle.h"
#include "ecs/jobs/IJob.h"
#include "ecs/ComponentAccessAttribute.h"

namespace ECS {

    // Forward declarations
    class World;
    class Archetype;

    namespace Jobs {

        /**
         * @brief Type-safe parallel query for iterating over entities and chunks.
         * 
         * ParallelQuery allows safe iteration over entities matching a specific
         * component signature. It provides both entity-level and chunk-level
         * iteration interfaces suitable for job-based parallelization.
         * 
         * Example usage:
         * @code
         * // Create a query for entities with Transform and Velocity
         * auto query = world.CreateParallelQuery<Transform, Velocity>();
         * 
         * // Get matching chunks
         * auto chunks = query.GetChunks();
         * for (auto* chunk : chunks) {
         *     uint32_t count = chunk->Count();
         *     auto* transforms = chunk->GetComponentArray<Transform>(0);
         *     auto* velocities = chunk->GetComponentArray<Velocity>(1);
         *     // Process chunk data
         * }
         * 
         * // Or use with jobs
         * struct MyJob : public IJobChunk {
         *     void Execute(Chunk& chunk, uint32_t count) override {
         *         // Job code
         *     }
         * };
         * @endcode
         * 
         * @tparam Components Component types to query for
         */
        template<typename... Components>
        class ParallelQuery {
        public:
            /**
             * @brief Constructor.
             * @param world The world to query
             */
            ParallelQuery(class World* world)
                : m_world(world), m_signature(_buildSignature()) {
            }

            /**
             * @brief Get all chunks matching this query's component signature.
             * 
             * Returns a list of chunks containing entities with all the
             * components specified in the query template parameters.
             * 
             * @return Vector of pointers to matching chunks
             */
            std::vector<Chunk*> GetChunks() const;

            /**
             * @brief Get all entities matching this query's component signature.
             * 
             * @return Vector of entities with matching components
             */
            std::vector<Entity> GetEntities() const;

            /**
             * @brief Iterate over matching chunks with a function.
             * 
             * Calls the provided function for each matching chunk.
             * This is a blocking operation.
             * 
             * @tparam Func Callable type accepting (Chunk&, uint32_t count)
             * @param func Function to apply to each chunk
             */
            template<typename Func>
            void ForEachChunk(Func&& func) const {
                auto chunks = GetChunks();
                for (auto* chunk : chunks) {
                    if (chunk) {
                        func(*chunk, chunk->Count());
                    }
                }
            }

            /**
             * @brief Iterate over matching entities with a function.
             * 
             * Calls the provided function for each matching entity.
             * This is a blocking operation.
             * 
             * @tparam Func Callable type accepting (Entity)
             * @param func Function to apply to each entity
             */
            template<typename Func>
            void ForEachEntity(Func&& func) const {
                auto entities = GetEntities();
                for (const auto& entity : entities) {
                    func(entity);
                }
            }

            /**
             * @brief Get the component signature for this query.
             * @return Reference to the signature
             */
            const Signature& GetSignature() const { return m_signature; }

            /**
             * @brief Get the world this query operates on.
             * @return Pointer to the world
             */
            World* GetWorld() const { return m_world; }

            /**
             * @brief Get component access patterns for this query.
             * 
             * By default, all components are read-only. Override in derived
             * classes or use SetComponentAccess to declare write access.
             * 
             * @return Vector of component access declarations
             */
            const std::vector<ComponentAccess>& GetComponentAccesses() const {
                return m_accesses;
            }

            /**
             * @brief Declare that a component is written to in jobs using this query.
             * 
             * Used by job scheduling to detect conflicts with other jobs.
             * 
             * @tparam Component The component type being written
             */
            template<typename Component>
            void SetComponentAsWritable() {
                ComponentTypeId id = TypeIdOf<Component>();
                for (auto& access : m_accesses) {
                    if (access.ComponentId == id) {
                        access.Mode = ComponentAccessMode::Write;
                        return;
                    }
                }
                // Not found, add it
                m_accesses.emplace_back(
                    ComponentAccess(id, ComponentAccessMode::Write)
                );
            }

        private:
            /**
             * @brief Build signature from component template parameters.
             * @return Signature matching all component types
             */
            static Signature _buildSignature();

            World* m_world = nullptr;
            Signature m_signature;
            std::vector<ComponentAccess> m_accesses;
        };

        // Template specialization for empty query (all entities)
        template<>
        class ParallelQuery<> {
        public:
            ParallelQuery(class World* world) : m_world(world) { }

            std::vector<Chunk*> GetChunks() const;
            std::vector<Entity> GetEntities() const;

            template<typename Func>
            void ForEachChunk(Func&& func) const {
                auto chunks = GetChunks();
                for (auto* chunk : chunks) {
                    if (chunk) {
                        func(*chunk, chunk->Count());
                    }
                }
            }

            template<typename Func>
            void ForEachEntity(Func&& func) const {
                auto entities = GetEntities();
                for (const auto& entity : entities) {
                    func(entity);
                }
            }

            World* GetWorld() const { return m_world; }

        private:
            World* m_world = nullptr;
        };

    }  // namespace Jobs
}  // namespace ECS

#endif
