/* Start Header *****************************************************************/
/*!
\file     ParticleSystem.h
\author   Choi Meng Yew
\date     28th February 2026
\brief
ECS system header for GPU-accelerated particles. Mirrors BoidSystem's
CUDA-GL interop pattern. Includes a preset registry so emitter components
stay lightweight.

Architecture:
  - Simulation runs on 3 CUDA-only SoA buffers (d_posVel, d_lifeColor, d_sizeRot)
  - After simulation, Interleave() packs the SoA data into the mapped GL VBO
  - The VBO is interleaved: [posVel0][lifeColor0][sizeRot0][posVel1]...
  - The VAO reads the interleaved data via instanced attributes

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#include "ecs/SystemManager.h"
#include "graphics/ParticlePreset.hpp"
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <glad/glad.h>

#ifdef GRAPE_HAS_CUDA
#include <cuda_runtime.h>
struct cudaGraphicsResource;
#endif

class TileMap;

namespace ECS {

    // GPU data per emitter entity
    struct EmitterGPUData {
        // --- OpenGL (render only) ---
        GLuint vao = 0;
        GLuint quadVBO = 0;
        GLuint instanceVBO = 0;        // interleaved output for rendering

        // --- Simulation state ---
        int maxParticles = 0;
        int aliveCount = 0;
        float emitAccum = 0.0f;

        bool initialized = false;

#ifdef GRAPE_HAS_CUDA
        cudaGraphicsResource* cudaVBO = nullptr;  // CUDA mapping of instanceVBO

        // CUDA-only work buffers (simulation runs here)
        float4* d_posVel = nullptr;
        float4* d_lifeColor = nullptr;
        float4* d_sizeRot = nullptr;

        // ------------------------------------------------------------
        // Persistent scratch (used by Compact every frame; no cudaMalloc)
        // ------------------------------------------------------------
        int* d_aliveFlags = nullptr;   // 0/1 flags (size = maxParticles)
        int* d_offsets = nullptr;   // exclusive scan output (size = maxParticles)
        int* d_totalAlive = nullptr;   // single int on device

        float4* d_tempPV = nullptr;       // compacted temp storage (size = maxParticles)
        float4* d_tempLC = nullptr;
        float4* d_tempSR = nullptr;

        // CUB scan temp storage
        void* d_scanTemp = nullptr;
        size_t  scanTempBytes = 0;
#endif
    };

    // Render data passed to RendererSystem
    struct EmitterRenderData {
        GLuint vao = 0;
        int    aliveCount = 0;
        uint32_t textureId = 0;
        float  particleSize = 8.0f;
    };

    class ParticleSystem : public ISystem {
    public:
        SystemMetadata GetMetadata() const override;

        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;

        // --- Preset registry ---
        uint32_t RegisterPreset(const ParticlePreset& preset) {
            m_presets.push_back(preset);
            return (uint32_t)(m_presets.size() - 1);
        }

        const ParticlePreset& GetPreset(uint32_t id) const {
            return m_presets[id];
        }

        size_t GetPresetCount() const { return m_presets.size(); }

        // --- Collision grid ---
        void UpdateCollisionGrid(const TileMap& tileMap);

        // --- Renderer access ---
        const std::unordered_map<uint32_t, EmitterRenderData>& GetRenderData() const {
            return m_renderData;
        }

        // --- Burst API ---
        void TriggerBurst(uint32_t entityIndex, int count);

    private:
        void InitEmitter(uint32_t entityIndex, int maxParticles);
        void DestroyEmitter(uint32_t entityIndex);
        void CreateQuadVAO(EmitterGPUData& gpu, int maxParticles);

        std::vector<ParticlePreset>                         m_presets;
        std::unordered_map<uint32_t, EmitterGPUData>        m_emitters;
        std::unordered_map<uint32_t, EmitterRenderData>     m_renderData;
        std::unordered_map<uint32_t, int>                   m_pendingBursts;

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