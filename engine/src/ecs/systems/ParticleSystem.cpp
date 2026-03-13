/* Start Header *****************************************************************/
/*!
\file     ParticleSystem.cpp
\author   Choi Meng Yew (100%)
\date     28th February 2026
\brief
GPU-accelerated particle system using CUDA-OpenGL interop.
Each ParticleEmitter entity gets a separate GPU buffer. CUDA kernels
handle emission, physics, and compaction. Mirrors BoidSystem architecture.

Architecture:
  - 3 CUDA-only SoA buffers: d_posVel, d_lifeColor, d_sizeRot
  - Simulation (Update, Compact, Emit) runs entirely on those buffers
  - After simulation, Interleave() packs SoA into the mapped GL VBO
  - Unmap releases the VBO back to OpenGL for instanced rendering

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/ParticleSystem.h"
#include "ecs/Components.h"
#include "graphics/shader.hpp"
#include "services/TimeSystem.h"
#include "core/Logger.h"
#include "Core/World/TileMap.hpp"

#ifdef GRAPE_HAS_CUDA
#include "cuda/CudaParticles.cuh"
#include "cuda/CudaGLInterop.cuh"
#endif

namespace ECS {

    // ================================================================
    // Metadata
    // ================================================================
    SystemMetadata ParticleSystem::GetMetadata() const {
        ComponentAccessBuilder builder("ParticleSystem");
        builder.SetExecutionOrder(51);
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    // ================================================================
    // Lifecycle
    // ================================================================
    void ParticleSystem::OnCreate(World& /*world*/) {
        LOG_INFO("[ParticleSystem] OnCreate");

        // Register built-in presets
        RegisterPreset(ParticlePreset::Bubbles());    // 0
        RegisterPreset(ParticlePreset::Geyser());     // 1
        RegisterPreset(ParticlePreset::Smoke());       // 2
        RegisterPreset(ParticlePreset::Explosion());   // 3
        RegisterPreset(ParticlePreset::Sediment());    // 4
    }

    void ParticleSystem::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        const float totalTime = static_cast<float>(TimeSystem::Instance().GetTotalTime());
        const unsigned int frameCount = static_cast<unsigned int>(TimeSystem::Instance().GetFrameCount());

        // Track which emitters are still alive
        std::unordered_map<uint32_t, bool> alive;
        for (auto& [id, _] : m_emitters) {
            alive[id] = false;
        }

        world.Each<Components::ParticleEmitter>(
            [&](Entity entity, Components::ParticleEmitter& emitter) {
                if (!emitter.active) return;

                const uint32_t id = entity.Index;
                alive[id] = true;

                auto it = m_emitters.find(id);
                if (it == m_emitters.end()) {
                    InitEmitter(id, emitter.maxParticles);
                    it = m_emitters.find(id);
                }

                // Handle capacity change
                if (it->second.maxParticles != emitter.maxParticles && emitter.maxParticles > 0) {
                    DestroyEmitter(id);
                    InitEmitter(id, emitter.maxParticles);
                    it = m_emitters.find(id);
                }

                EmitterGPUData& gpu = it->second;

#ifdef GRAPE_HAS_CUDA
                if (!gpu.initialized || !gpu.cudaVBO) return;

                // Component is the source of truth — no preset lookup needed
                ParticleParams params{};

                // Emitter position from Transform
                params.emitterX = 0.0f;
                params.emitterY = 0.0f;
                if (world.Has<Components::LocalTransform>(entity)) {
                    auto& transform = world.Get<Components::LocalTransform>(entity);
                    params.emitterX = transform.Position.X;
                    params.emitterY = transform.Position.Y;
                }

                // All simulation data read directly from component
                params.emissionAngle = emitter.emissionAngle;
                params.emissionSpread = emitter.emissionSpread;
                params.emissionRadius = emitter.emissionRadius;
                params.shape = static_cast<EmissionShape>(emitter.emissionShape);
                params.lifetimeMin = emitter.lifetimeMin;
                params.lifetimeMax = emitter.lifetimeMax;
                params.speedMin = emitter.speedMin;
                params.speedMax = emitter.speedMax;
                params.gravityX = emitter.gravityX;
                params.gravityY = emitter.gravityY;
                params.drag = emitter.drag;
                params.turbulence = emitter.turbulence;
                params.wobbleFrequency = emitter.wobbleFrequency;
                params.wobbleAmplitude = emitter.wobbleAmplitude;
                params.sizeStart = emitter.sizeStart;
                params.sizeEnd = emitter.sizeEnd;
                params.rotationSpeedMin = emitter.rotationSpeedMin;
                params.rotationSpeedMax = emitter.rotationSpeedMax;
                params.colorStartR = emitter.colorStartR;
                params.colorStartG = emitter.colorStartG;
                params.colorStartB = emitter.colorStartB;
                params.colorStartA = emitter.colorStartA;
                params.colorEndR = emitter.colorEndR;
                params.colorEndG = emitter.colorEndG;
                params.colorEndB = emitter.colorEndB;
                params.colorEndA = emitter.colorEndA;
                params.dieOnCollision = emitter.dieOnCollision;
                params.bounciness = emitter.bounciness;
                params.killOutOfBounds = emitter.killOutOfBounds;
                params.maxParticles = emitter.maxParticles;

                // From system state
                params.dt = dt;
                params.totalTime = totalTime;
                params.aliveCount = gpu.aliveCount;
                params.frameCount = frameCount;

                // Collision grid
                params.collisionMasks = m_collisionGrid.d_masks;
                params.collisionWidth = m_collisionGrid.width;
                params.collisionHeight = m_collisionGrid.height;
                params.collisionOriginX = m_collisionGrid.originX;
                params.collisionOriginY = m_collisionGrid.originY;
                params.tileSize = m_collisionGrid.tileSize;

                // --- Compute emission count from rate ---
                int emitCount = 0;
                if (emitter.emissionRate > 0.0f) {
                    gpu.emitAccum += emitter.emissionRate * dt;
                    emitCount = (int)gpu.emitAccum;
                    gpu.emitAccum -= (float)emitCount;
                }
                params.emitCount = emitCount;

                // --- Handle bursts ---
                params.burstCount = 0;
                auto burstIt = m_pendingBursts.find(id);
                if (burstIt != m_pendingBursts.end()) {
                    params.burstCount = burstIt->second;
                    m_pendingBursts.erase(burstIt);
                }
                if (emitter.burstCount > 0) {
                    params.burstCount += emitter.burstCount;
                    emitter.burstCount = 0; // consume
                }

                // ============================================
                // Simulation on CUDA-only work buffers
                // ============================================

                // Step 1: Update alive particles
                CudaParticles::Update(gpu.d_posVel, gpu.d_lifeColor, gpu.d_sizeRot, params);

                // Step 2: Compact (remove dead)
                gpu.aliveCount = CudaParticles::Compact(
                    gpu.d_posVel,
                    gpu.d_lifeColor,
                    gpu.d_sizeRot,
                    gpu.aliveCount,
                    emitter.maxParticles,
                    gpu.d_aliveFlags,
                    gpu.d_offsets,
                    gpu.d_totalAlive,
                    gpu.d_tempPV,
                    gpu.d_tempLC,
                    gpu.d_tempSR,
                    gpu.d_scanTemp,
                    gpu.scanTempBytes);

                // Step 3: Emit new particles
                params.aliveCount = gpu.aliveCount;
                gpu.aliveCount = CudaParticles::Emit(gpu.d_posVel, gpu.d_lifeColor, gpu.d_sizeRot, params);

                // ============================================
                // Interleave SoA into mapped GL VBO
                // ============================================
                if (gpu.aliveCount > 0) {
                    float4* d_vbo = CudaGL::Map<float4>(gpu.cudaVBO);
                    if (d_vbo) {
                        CudaParticles::Interleave(d_vbo,
                            gpu.d_posVel, gpu.d_lifeColor, gpu.d_sizeRot,
                            gpu.aliveCount);
                        CudaGL::Unmap(gpu.cudaVBO);
                    }
                }

                uint16_t layerId = 0;
                if (world.Has<Components::Layer>(entity))
                    layerId = world.Get<Components::Layer>(entity).Id;

                // Update render data
                m_renderData[id] = EmitterRenderData{
                    gpu.vao,
                    gpu.aliveCount,
                    emitter.textureId,
                    emitter.particleSize,
                    layerId
                };
#endif
            });

        // Clean up removed emitters
        for (auto& [id, isAlive] : alive) {
            if (!isAlive) {
                DestroyEmitter(id);
                m_renderData.erase(id);
            }
        }
    }

    void ParticleSystem::OnDestroy(World&) {
        LOG_INFO("[ParticleSystem] OnDestroy - freeing all GPU resources");

        for (auto& [id, gpu] : m_emitters) {
#ifdef GRAPE_HAS_CUDA
            if (gpu.cudaVBO)     CudaGL::UnregisterBuffer(gpu.cudaVBO);

            if (gpu.d_posVel)    cudaFree(gpu.d_posVel);
            if (gpu.d_lifeColor) cudaFree(gpu.d_lifeColor);
            if (gpu.d_sizeRot)   cudaFree(gpu.d_sizeRot);

            if (gpu.d_aliveFlags) cudaFree(gpu.d_aliveFlags);
            if (gpu.d_offsets)    cudaFree(gpu.d_offsets);
            if (gpu.d_totalAlive) cudaFree(gpu.d_totalAlive);

            if (gpu.d_tempPV) cudaFree(gpu.d_tempPV);
            if (gpu.d_tempLC) cudaFree(gpu.d_tempLC);
            if (gpu.d_tempSR) cudaFree(gpu.d_tempSR);

            if (gpu.d_scanTemp) cudaFree(gpu.d_scanTemp);
#endif
            if (gpu.vao)         glDeleteVertexArrays(1, &gpu.vao);
            if (gpu.quadVBO)     glDeleteBuffers(1, &gpu.quadVBO);
            if (gpu.instanceVBO) glDeleteBuffers(1, &gpu.instanceVBO);
        }

#ifdef GRAPE_HAS_CUDA
        if (m_collisionGrid.d_masks) {
            cudaFree(m_collisionGrid.d_masks);
            m_collisionGrid.d_masks = nullptr;
        }
#endif

        m_emitters.clear();
        m_renderData.clear();
    }

    // ================================================================
    // Burst API
    // ================================================================
    void ParticleSystem::TriggerBurst(uint32_t entityIndex, int count) {
        m_pendingBursts[entityIndex] += count;
    }

    // ================================================================
    // GPU Resource Management
    // ================================================================
    void ParticleSystem::InitEmitter(uint32_t entityIndex, int maxParticles) {
        if (maxParticles <= 0) return;

        EmitterGPUData gpu{};
        gpu.maxParticles = maxParticles;
        gpu.aliveCount = 0;
        gpu.emitAccum = 0.0f;

        // Instance VBO: interleaved output for rendering
        // 3 float4s per particle (posVel + lifeColor + sizeRot) = 48 bytes each
        glGenBuffers(1, &gpu.instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
            sizeof(float) * 4 * 3 * maxParticles,
            nullptr,
            GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

#ifdef GRAPE_HAS_CUDA
        // Register VBO with CUDA for interleave output
        gpu.cudaVBO = CudaGL::RegisterBuffer(gpu.instanceVBO);

        // Allocate CUDA-only work buffers for simulation
        cudaMalloc(&gpu.d_posVel, sizeof(float4) * maxParticles);
        cudaMalloc(&gpu.d_lifeColor, sizeof(float4) * maxParticles);
        cudaMalloc(&gpu.d_sizeRot, sizeof(float4) * maxParticles);

        // --- Persistent compaction scratch ---

        cudaMalloc(&gpu.d_aliveFlags, sizeof(int) * maxParticles);
        cudaMalloc(&gpu.d_offsets, sizeof(int) * maxParticles);
        cudaMalloc(&gpu.d_totalAlive, sizeof(int));

        cudaMalloc(&gpu.d_tempPV, sizeof(float4) * maxParticles);
        cudaMalloc(&gpu.d_tempLC, sizeof(float4) * maxParticles);
        cudaMalloc(&gpu.d_tempSR, sizeof(float4) * maxParticles);

        gpu.scanTempBytes = CudaParticles::GetScanTempBytes(
            maxParticles,
            gpu.d_aliveFlags,
            gpu.d_offsets);

        cudaMalloc(&gpu.d_scanTemp, gpu.scanTempBytes);

        cudaMemset(gpu.d_posVel, 0, sizeof(float4) * maxParticles);
        cudaMemset(gpu.d_lifeColor, 0, sizeof(float4) * maxParticles);
        cudaMemset(gpu.d_sizeRot, 0, sizeof(float4) * maxParticles);
#endif

        CreateQuadVAO(gpu, maxParticles);

        gpu.initialized = true;
        m_emitters[entityIndex] = gpu;

        LOG_INFO("[ParticleSystem] Initialized emitter for entity "
            << entityIndex << " with capacity " << maxParticles);
    }

    void ParticleSystem::DestroyEmitter(uint32_t entityIndex) {
        auto it = m_emitters.find(entityIndex);
        if (it == m_emitters.end()) return;

        EmitterGPUData& gpu = it->second;

#ifdef GRAPE_HAS_CUDA
        if (gpu.cudaVBO) { CudaGL::UnregisterBuffer(gpu.cudaVBO); gpu.cudaVBO = nullptr; }

        if (gpu.d_posVel) { cudaFree(gpu.d_posVel);    gpu.d_posVel = nullptr; }
        if (gpu.d_lifeColor) { cudaFree(gpu.d_lifeColor); gpu.d_lifeColor = nullptr; }
        if (gpu.d_sizeRot) { cudaFree(gpu.d_sizeRot);   gpu.d_sizeRot = nullptr; }

        if (gpu.d_aliveFlags) { cudaFree(gpu.d_aliveFlags); gpu.d_aliveFlags = nullptr; }
        if (gpu.d_offsets) { cudaFree(gpu.d_offsets);    gpu.d_offsets = nullptr; }
        if (gpu.d_totalAlive) { cudaFree(gpu.d_totalAlive); gpu.d_totalAlive = nullptr; }

        if (gpu.d_tempPV) { cudaFree(gpu.d_tempPV); gpu.d_tempPV = nullptr; }
        if (gpu.d_tempLC) { cudaFree(gpu.d_tempLC); gpu.d_tempLC = nullptr; }
        if (gpu.d_tempSR) { cudaFree(gpu.d_tempSR); gpu.d_tempSR = nullptr; }

        if (gpu.d_scanTemp) { cudaFree(gpu.d_scanTemp); gpu.d_scanTemp = nullptr; }
#endif
        if (gpu.vao) { glDeleteVertexArrays(1, &gpu.vao); gpu.vao = 0; }
        if (gpu.quadVBO) { glDeleteBuffers(1, &gpu.quadVBO); gpu.quadVBO = 0; }
        if (gpu.instanceVBO) { glDeleteBuffers(1, &gpu.instanceVBO); gpu.instanceVBO = 0; }

        m_emitters.erase(it);
        LOG_INFO("[ParticleSystem] Destroyed emitter for entity " << entityIndex);
    }

    void ParticleSystem::CreateQuadVAO(EmitterGPUData& gpu, int /*maxParticles*/) {
        float quadVertices[] = {
            // pos.x, pos.y, uv.x, uv.y
            -0.5f, -0.5f,  0.0f, 0.0f,
             0.5f, -0.5f,  1.0f, 0.0f,
             0.5f,  0.5f,  1.0f, 1.0f,
            -0.5f, -0.5f,  0.0f, 0.0f,
             0.5f,  0.5f,  1.0f, 1.0f,
            -0.5f,  0.5f,  0.0f, 1.0f,
        };

        glGenVertexArrays(1, &gpu.vao);
        glBindVertexArray(gpu.vao);

        // Quad VBO (per-vertex data)
        glGenBuffers(1, &gpu.quadVBO);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        // layout(location = 0) in vec2 aPos
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        // layout(location = 1) in vec2 aTexCoord
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        // Instance VBO: interleaved, stride = 3 * float4 = 48 bytes
        glBindBuffer(GL_ARRAY_BUFFER, gpu.instanceVBO);
        GLsizei stride = 3 * 4 * sizeof(float); // 48 bytes

        // layout(location = 2) in vec4 aPosVel — offset 0
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribDivisor(2, 1);

        // layout(location = 3) in vec4 aLifeColor — offset 16
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glVertexAttribDivisor(3, 1);

        // layout(location = 4) in vec4 aSizeRot — offset 32
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
        glVertexAttribDivisor(4, 1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void ParticleSystem::UpdateCollisionGrid(const TileMap& tileMap) {
#ifdef GRAPE_HAS_CUDA
        const auto& masks = tileMap.GetCollisionMasks();
        if (masks.empty()) return;

        const size_t byteCount = masks.size();

        if (!m_collisionGrid.d_masks ||
            (size_t)(m_collisionGrid.width * m_collisionGrid.height) != masks.size())
        {
            if (m_collisionGrid.d_masks) cudaFree(m_collisionGrid.d_masks);
            cudaMalloc(&m_collisionGrid.d_masks, byteCount);
        }

        cudaMemcpy(m_collisionGrid.d_masks, masks.data(), byteCount, cudaMemcpyHostToDevice);

        m_collisionGrid.width = (int32_t)tileMap.CollisionWidth();
        m_collisionGrid.height = (int32_t)tileMap.CollisionHeight();
        m_collisionGrid.originX = tileMap.OriginX();
        m_collisionGrid.originY = tileMap.OriginY();
        m_collisionGrid.tileSize = tileMap.TileSize();
#endif
    }

    void ParticleSystem::DrawEmittersByLayer(uint16_t layerId, Shader& shader,
        const glm::mat4& viewProj,
        Graphics::LightManager& lights, World& world) {
        (void)viewProj;  // set by caller before this call
        (void)lights;    // bound by caller before this call

        for (const auto& [entityId, emitter] : m_renderData) {
            if (emitter.layerId != layerId) continue;
            if (emitter.aliveCount <= 0 || emitter.vao == 0) continue;

            shader.setUniform("uParticleSize", emitter.particleSize);

            // Material2D gate — same opt-in as sprites
            ECS::Entity e{ entityId };
            bool hasMaterial = world.Has<Components::Material2D>(e);

            if (hasMaterial) {
                const auto& mat = world.Get<Components::Material2D>(e);
                shader.setUniform("uLightingEnabled", 1);
                shader.setUniform("uMaterialFlags", static_cast<int>(mat.Flags));
                shader.setUniform("uMetallic", mat.Metallic);
                shader.setUniform("uSmoothness", mat.Smoothness);
                shader.setUniform("uAOStrength", mat.AOStrength);
                shader.setUniform("uNormalStrength", mat.NormalStrength);

                if (mat.NormalTextureId != 0) {
                    shader.setUniform("uHasNormalMap", 1);
                    shader.setUniform("uNormalMap", 1);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, mat.NormalTextureId);
                }
                else {
                    shader.setUniform("uHasNormalMap", 0);
                }

                if (mat.MRA_TextureId != 0) {
                    shader.setUniform("uHasMRAMap", 1);
                    shader.setUniform("uMRAMap", 2);
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, mat.MRA_TextureId);
                }
                else {
                    shader.setUniform("uHasMRAMap", 0);
                }
            }
            else {
                shader.setUniform("uLightingEnabled", 0);
                shader.setUniform("uMaterialFlags", 0);
                shader.setUniform("uHasNormalMap", 0);
                shader.setUniform("uHasMRAMap", 0);
            }

            if (emitter.textureId != 0) {
                shader.setUniform("uHasTexture", 1);
                shader.setUniform("uTexture", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, emitter.textureId);
            }
            else {
                shader.setUniform("uHasTexture", 0);
            }

            glBindVertexArray(emitter.vao);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, emitter.aliveCount);
            glBindVertexArray(0);
        }
    }
} // namespace ECS