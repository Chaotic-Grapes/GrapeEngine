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
            SystemMetadata GetMetadata() const override;

            void OnCreate(World& world) override;
            void OnUpdate(World& world) override;
            void OnDestroy(World& world) override;

            // Render all emitters on the requested layer using current GPU buffers.
            void DrawEmittersByLayer(uint16_t layerId, Shader& shader,
                const glm::mat4& viewProj,
                Graphics::LightManager& lights, World& world);

            void DrawEmitterForEntity(uint32_t entityIndex, Shader& shader, World& world);

            // Register one preset and return its index for emitter references.
            uint32_t RegisterPreset(const ParticlePreset& preset) {
                m_presets.push_back(preset);
                return (uint32_t)(m_presets.size() - 1);
            }

            // Retrieve a preset by index.
            const ParticlePreset& GetPreset(uint32_t id) const {
                return m_presets[id];
            }

            size_t GetPresetCount() const { return m_presets.size(); }

            // Upload/refresh shared tile collision grid used for particle collisions.
            void UpdateCollisionGrid(const TileMap& tileMap);

            const std::unordered_map<uint32_t, EmitterRenderData>& GetRenderData() const {
                return m_renderData;
            }

            // Queue a burst spawn request for a specific emitter entity.
            void TriggerBurst(uint32_t entityIndex, int count);

        private:
            // Allocate and initialize GPU simulation/render resources for one emitter.
            void InitEmitter(uint32_t entityIndex, int maxParticles);
            // Destroy GPU simulation/render resources for one emitter.
            void DestroyEmitter(uint32_t entityIndex);
            // Build quad geometry and VAO bindings for an emitter render slot.
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