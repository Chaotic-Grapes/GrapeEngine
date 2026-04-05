    /* Start Header *****************************************************************/
    /*!
    \file     ParticleSystem.h
    \author   Choi Meng Yew (100%)
    \date     28th February 2026
    \brief
    ECS system header for GPU-accelerated particles.

    Architecture:
      - Simulation runs on 3 CUDA-only SoA buffers (d_posVel, d_lifeColor, d_sizeRot)
      - After simulation, Interleave() packs the SoA data into the mapped GL VBO
      - Double-buffered VBOs: CUDA writes buffer[writeIdx], GL renders buffer[readIdx]

    Optimizations:
      - Double-buffered VBOs (no GL/CUDA contention, eliminates unmap stalls)
      - Single shared CUDA stream
      - Single cudaGraphicsMapResources/UnmapResources call per frame
      - Deferred totalAlive readback
      - Fused update+markAlive kernel
      - Early-out for idle emitters

    Copyright (C) 2025 DigiPen Institute of Technology.
    Reproduction or disclosure of this file or its contents without the
    prior written consent of DigiPen Institute of Technology is prohibited.
    */
    /* End Header *******************************************************************/

    #pragma once

    #include "ecs/SystemManager.h"
    #include "graphics/ParticlePreset.hpp"
    #include "graphics/LightManager.hpp"
    #include <unordered_map>
    #include <vector>
    #include <cstdint>
    #include <glad/glad.h>

    #ifdef GRAPE_HAS_CUDA
    #include <cuda_runtime.h>
    struct cudaGraphicsResource;
    #endif

    class TileMap;

    namespace Graphics {
        class LightManager;
    }

    namespace ECS {

        struct EmitterGPUData {
            // --- OpenGL (double buffered) ---
            GLuint vao[2] = { 0, 0 };
            GLuint quadVBO = 0;
            GLuint instanceVBO[2] = { 0, 0 };

            uint8_t bufferIndex = 0;             // CUDA writes to this index
            uint8_t mappedWriteIdx = 0;          // which index was actually mapped

            // --- Simulation state ---
            int maxParticles = 0;
            int aliveCount = 0;
            float emitAccum = 0.0f;

            bool initialized = false;

    #ifdef GRAPE_HAS_CUDA
            cudaGraphicsResource* cudaVBO[2] = { nullptr, nullptr };
            float4* d_mappedVBO = nullptr;       // mapped write buffer ptr

            float4* d_posVel = nullptr;
            float4* d_lifeColor = nullptr;
            float4* d_sizeRot = nullptr;

            int* d_aliveFlags = nullptr;
            int* d_offsets = nullptr;
            int* d_totalAlive = nullptr;
            int* h_totalAlive = nullptr;

            float4* d_tempPV = nullptr;
            float4* d_tempLC = nullptr;
            float4* d_tempSR = nullptr;

            void* d_scanTemp = nullptr;
            size_t  scanTempBytes = 0;
    #endif
        };

        struct EmitterRenderData {
            GLuint vao          = 0;
            int    aliveCount   = 0;
            uint32_t textureId  = 0;
            float  particleSize = 8.0f;
            uint16_t layerId    = 0;
            int zOrder          = 0;
        };

        class ParticleSystem : public ISystem {
        public:
            /**
             * @brief Return system metadata for scheduler registration.
             * @return SystemMetadata describing component access and execution order.
             */
            SystemMetadata GetMetadata() const override;

            /**
             * @brief Initialize GPU resources for all existing emitter entities.
             * @param world ECS world containing entities with particle emitter components.
             */
            void OnCreate(World& world) override;

            /**
             * @brief Simulate and render all active particle emitters this frame.
             * @param world ECS world containing entities with particle emitter components.
             */
            void OnUpdate(World& world) override;

            /**
             * @brief Release all GPU resources owned by particle emitters.
             * @param world ECS world passed by the scheduler.
             */
            void OnDestroy(World& world) override;

            /**
             * @brief Render all emitters on the requested layer using current GPU buffers.
             * @param layerId Layer identifier to filter emitters for rendering.
             * @param shader Shader program used to render particle instances.
             * @param viewProj Combined view-projection matrix for the current camera.
             * @param lights Light manager providing lighting data for the particle shader.
             * @param world ECS world used to resolve emitter entity components.
             */
            void DrawEmittersByLayer(uint16_t layerId, Shader& shader,
                const glm::mat4& viewProj,
                Graphics::LightManager& lights, World& world);

            /**
             * @brief Render one specific emitter identified by its ECS entity index.
             * @param entityIndex ECS entity index of the emitter to render.
             * @param shader Shader program used to render particle instances.
             * @param world ECS world used to resolve the emitter entity components.
             */
            void DrawEmitterForEntity(uint32_t entityIndex, Shader& shader, World& world);

            /**
             * @brief Register one preset and return its index for emitter references.
             * @param preset Particle preset configuration to register.
             * @return Index of the newly registered preset.
             */
            uint32_t RegisterPreset(const ParticlePreset& preset) {
                m_presets.push_back(preset);
                return (uint32_t)(m_presets.size() - 1);
            }

            /**
             * @brief Retrieve a preset by index.
             * @param id Index of the preset to retrieve.
             * @return Reference to the ParticlePreset at that index.
             */
            const ParticlePreset& GetPreset(uint32_t id) const {
                return m_presets[id];
            }

            /**
             * @brief Return the number of registered particle presets.
             * @return Count of presets registered via RegisterPreset.
             */
            size_t GetPresetCount() const { return m_presets.size(); }

            /**
             * @brief Upload and refresh the shared tile collision grid used for particle collisions.
             * @param tileMap Tile map whose solid-cell data will be copied to the GPU.
             */
            void UpdateCollisionGrid(const TileMap& tileMap);

            /**
             * @brief Return the per-emitter render data map for read-only use by the renderer.
             * @return Reference to the map from entity index to EmitterRenderData.
             */
            const std::unordered_map<uint32_t, EmitterRenderData>& GetRenderData() const {
                return m_renderData;
            }

            /**
             * @brief Queue a burst spawn request for a specific emitter entity.
             * @param entityIndex ECS entity index of the emitter to burst.
             * @param count Number of particles to spawn in the burst.
             */
            void TriggerBurst(uint32_t entityIndex, int count);

        private:
            /**
             * @brief Allocate and initialize GPU simulation and render resources for one emitter.
             * @param entityIndex ECS entity index of the emitter to initialize.
             * @param maxParticles Maximum particle capacity to allocate.
             */
            void InitEmitter(uint32_t entityIndex, int maxParticles);

            /**
             * @brief Destroy GPU simulation and render resources for one emitter.
             * @param entityIndex ECS entity index of the emitter to destroy.
             */
            void DestroyEmitter(uint32_t entityIndex);

            /**
             * @brief Build quad geometry and VAO bindings for an emitter render slot.
             * @param gpu GPU state struct to populate with the new VAO and VBO.
             * @param slot Double-buffer slot index (0 or 1) to initialize.
             * @param maxParticles Number of particles the VAO must accommodate.
             */
            void CreateQuadVAO(EmitterGPUData& gpu, int slot, int maxParticles);

            std::vector<ParticlePreset>                         m_presets;
            std::unordered_map<uint32_t, EmitterGPUData>        m_emitters;
            std::unordered_map<uint32_t, EmitterRenderData>     m_renderData;
            std::unordered_map<uint32_t, int>                   m_pendingBursts;

    #ifdef GRAPE_HAS_CUDA
            cudaStream_t m_cudaStream = nullptr;
    #endif

            struct CollisionGrid {
                uint8_t* d_masks = nullptr;
                int32_t  width = 0;
                int32_t  height = 0;
                int32_t  originX = 0;
                int32_t  originY = 0;
                float    tileSize = 0.0f;
            } m_collisionGrid;
        };

    } // namespace ECS