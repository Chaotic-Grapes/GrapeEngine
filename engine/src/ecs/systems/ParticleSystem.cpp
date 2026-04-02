/* Start Header *****************************************************************/
/*!
\file     ParticleSystem.cpp
\author   Choi Meng Yew (100%)
\date     28th February 2026
\brief
GPU-accelerated particle system using CUDA-OpenGL interop.

Architecture:
  - Double-buffered VBOs per emitter: CUDA writes to buffer[writeIdx],
    GL renders from buffer[readIdx]. No contention, no stalls.
  - Only the write buffer is mapped for CUDA; the read buffer stays
    available for GL draw calls from the previous frame.

Optimizations:
  - Double-buffered VBOs (eliminates unmap stalls from GL pipeline)
  - Single shared CUDA stream for all emitters
  - Single cudaGraphicsMapResources/UnmapResources call per frame
  - Deferred totalAlive readback
  - Fused update+markAlive kernel
  - Early-out for idle emitters

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
#include "scene/LayerManager.h"
#include "Core/World/TileMap.hpp"

#ifdef GRAPE_HAS_CUDA
#include "cuda/CudaParticles.cuh"
#include "cuda/CudaGLInterop.cuh"
#include <vector>
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

#ifdef GRAPE_HAS_CUDA
        cudaStreamCreate(&m_cudaStream);
#endif

        RegisterPreset(ParticlePreset::Bubbles());
        RegisterPreset(ParticlePreset::Geyser());
        RegisterPreset(ParticlePreset::Smoke());
        RegisterPreset(ParticlePreset::Explosion());
        RegisterPreset(ParticlePreset::Sediment());
    }

    /**
     * @brief Simulate and prepare particle emitters for rendering.
     * @param world ECS world containing particle emitter components.
     * @return void
     * @note Uses layer-first traversal when LayerManager is available.
     * @note Entities without a Layer component are processed through a fallback pass.
     * @complexity O(sum(layer entities) + unlayered emitters + GPU simulation work)
     */
    void ParticleSystem::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        const float totalTime = static_cast<float>(TimeSystem::Instance().GetTotalTime());
        const unsigned int frameCount = static_cast<unsigned int>(TimeSystem::Instance().GetFrameCount());
        auto* layerManager = world.GetLayerManager();

        std::unordered_map<uint32_t, bool> alive;
        for (auto& [id, _] : m_emitters) {
            alive[id] = false;
        }

        // ==============================================================
        // Phase 1: Bulk-map only the WRITE buffers
        // ==============================================================
#ifdef GRAPE_HAS_CUDA
        std::vector<cudaGraphicsResource*> resourcesToMap;
        for (auto& [id, gpu] : m_emitters) {
            gpu.d_mappedVBO = nullptr;
            gpu.mappedWriteIdx = gpu.bufferIndex;
            if (gpu.initialized) {
                uint8_t writeIdx = gpu.bufferIndex;
                if (gpu.cudaVBO[writeIdx]) {
                    resourcesToMap.push_back(gpu.cudaVBO[writeIdx]);
                }
            }
        }

        if (!resourcesToMap.empty()) {
            cudaGraphicsMapResources((int)resourcesToMap.size(), resourcesToMap.data(), 0);
        }

        for (auto& [id, gpu] : m_emitters) {
            if (gpu.initialized) {
                uint8_t writeIdx = gpu.bufferIndex;
                if (gpu.cudaVBO[writeIdx]) {
                    size_t size = 0;
                    cudaGraphicsResourceGetMappedPointer(
                        (void**)&gpu.d_mappedVBO, &size, gpu.cudaVBO[writeIdx]);
                }
            }
        }
#endif

        // ==============================================================
        // Phase 2: Simulate all emitters on shared stream
        // ==============================================================
        auto processEmitter = [&](Entity entity, Components::ParticleEmitter& emitter, const uint16_t resolvedLayerId) {
                if (!emitter.active) {
                    return;
                }

                const uint32_t id = entity.Index;
                alive[id] = true;

                // Read zOrder once, in scope for both render data assignments
                int zOrder = 0;
                if (const auto* z = world.TryGet<Components::ZIndex2D>(entity)) {
                    zOrder = z->ZOrder;
                }

                auto it = m_emitters.find(id);
                if (it == m_emitters.end()) {
                    InitEmitter(id, emitter.maxParticles);
                    it = m_emitters.find(id);

#ifdef GRAPE_HAS_CUDA
                    EmitterGPUData& newGpu = it->second;
                    if (newGpu.initialized) {
                        uint8_t writeIdx = newGpu.bufferIndex;
                        newGpu.mappedWriteIdx = writeIdx;
                        if (newGpu.cudaVBO[writeIdx]) {
                            cudaGraphicsResource* res = newGpu.cudaVBO[writeIdx];
                            cudaGraphicsMapResources(1, &res, 0);
                            size_t size = 0;
                            cudaGraphicsResourceGetMappedPointer(
                                (void**)&newGpu.d_mappedVBO, &size, newGpu.cudaVBO[writeIdx]);
                        }
                    }
#endif
                }

                // Handle capacity change
                if (it->second.maxParticles != emitter.maxParticles && emitter.maxParticles > 0) {
#ifdef GRAPE_HAS_CUDA
                    EmitterGPUData& oldGpu = it->second;
                    if (oldGpu.d_mappedVBO) {
                        uint8_t writeIdx = oldGpu.bufferIndex;
                        if (oldGpu.cudaVBO[writeIdx]) {
                            cudaGraphicsResource* res = oldGpu.cudaVBO[writeIdx];
                            cudaGraphicsUnmapResources(1, &res, 0);
                        }
                        oldGpu.d_mappedVBO = nullptr;
                    }
#endif
                    DestroyEmitter(id);
                    InitEmitter(id, emitter.maxParticles);
                    it = m_emitters.find(id);

#ifdef GRAPE_HAS_CUDA
                    EmitterGPUData& newGpu = it->second;
                    if (newGpu.initialized) {
                        uint8_t writeIdx = newGpu.bufferIndex;
                        newGpu.mappedWriteIdx = writeIdx;
                        if (newGpu.cudaVBO[writeIdx]) {
                            cudaGraphicsResource* res = newGpu.cudaVBO[writeIdx];
                            cudaGraphicsMapResources(1, &res, 0);
                            size_t size = 0;
                            cudaGraphicsResourceGetMappedPointer(
                                (void**)&newGpu.d_mappedVBO, &size, newGpu.cudaVBO[writeIdx]);
                        }
                    }
#endif
                }

                EmitterGPUData& gpu = it->second;

#ifdef GRAPE_HAS_CUDA
                if (!gpu.initialized) return;

                int emitCount = 0;
                if (emitter.emissionRate > 0.0f) {
                    gpu.emitAccum += emitter.emissionRate * dt;
                    emitCount = (int)gpu.emitAccum;
                    gpu.emitAccum -= (float)emitCount;
                }

                int burstCount = 0;
                auto burstIt = m_pendingBursts.find(id);
                if (burstIt != m_pendingBursts.end()) {
                    burstCount = burstIt->second;
                    m_pendingBursts.erase(burstIt);
                }
                if (emitter.burstCount > 0) {
                    burstCount += emitter.burstCount;
                    emitter.burstCount = 0;
                }

                // Early-out: nothing to do
                if (gpu.aliveCount == 0 && emitCount == 0 && burstCount == 0) {
                    uint8_t readIdx = 1 - gpu.bufferIndex;
                    m_renderData[id] = EmitterRenderData{
                        gpu.vao[readIdx], 0, emitter.textureId, emitter.particleSize, resolvedLayerId, zOrder
                    };
                    return;
                }

                ParticleParams params{};

                params.emitterX = 0.0f;
                params.emitterY = 0.0f;
                if (world.Has<Components::LocalTransform>(entity)) {
                    auto& transform = world.Get<Components::LocalTransform>(entity);
                    params.emitterX = transform.Position.X;
                    params.emitterY = transform.Position.Y;
                }

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

                params.dt = dt;
                params.totalTime = totalTime;
                params.aliveCount = gpu.aliveCount;
                params.frameCount = frameCount;
                params.emitCount = emitCount;
                params.burstCount = burstCount;

                params.collisionMasks = m_collisionGrid.d_masks;
                params.collisionWidth = m_collisionGrid.width;
                params.collisionHeight = m_collisionGrid.height;
                params.collisionOriginX = m_collisionGrid.originX;
                params.collisionOriginY = m_collisionGrid.originY;
                params.tileSize = m_collisionGrid.tileSize;

                // ============================================
                // Simulation on shared stream
                // ============================================

                CudaParticles::Update(gpu.d_posVel, gpu.d_lifeColor, gpu.d_sizeRot,
                    gpu.d_aliveFlags, params, m_cudaStream);

                gpu.aliveCount = CudaParticles::Compact(
                    gpu.d_posVel, gpu.d_lifeColor, gpu.d_sizeRot,
                    gpu.aliveCount, emitter.maxParticles,
                    gpu.d_aliveFlags, gpu.d_offsets, gpu.d_totalAlive,
                    gpu.h_totalAlive,
                    gpu.d_tempPV, gpu.d_tempLC, gpu.d_tempSR,
                    gpu.d_scanTemp, gpu.scanTempBytes,
                    m_cudaStream);

                params.aliveCount = gpu.aliveCount;
                gpu.aliveCount = CudaParticles::Emit(gpu.d_posVel, gpu.d_lifeColor, gpu.d_sizeRot,
                    params, m_cudaStream);

                // Interleave into the WRITE buffer
                if (gpu.aliveCount > 0 && gpu.d_mappedVBO) {
                    CudaParticles::Interleave(gpu.d_mappedVBO,
                        gpu.d_posVel, gpu.d_lifeColor, gpu.d_sizeRot,
                        gpu.aliveCount, m_cudaStream);
                }

                // Render data points to the buffer we just wrote
                m_renderData[id] = EmitterRenderData{
                    gpu.vao[gpu.bufferIndex],
                    gpu.aliveCount,
                    emitter.textureId,
                    emitter.particleSize,
                    resolvedLayerId,
                    zOrder
                };

                // Flip for next frame
                gpu.bufferIndex = 1 - gpu.bufferIndex;
#endif
            };

        if (layerManager) {
            for (const auto layerId : layerManager->DrawOrder()) {
                if (!layerManager->IsUpdateEnabled(layerId)) {
                    continue;
                }
                for (const Entity entity : layerManager->EntitiesIn(layerId)) {
                    auto* emitter = world.TryGet<Components::ParticleEmitter>(entity);
                    if (!emitter) {
                        continue;
                    }
                    processEmitter(entity, *emitter, layerId);
                }
            }

            // Keep unlayered emitters in parity with previous behavior.
            world.Each<Components::ParticleEmitter>([&](Entity entity, Components::ParticleEmitter& emitter) {
                if (world.TryGet<Components::Layer>(entity)) {
                    return;
                }
                processEmitter(entity, emitter, 0);
            });
        } else {
            world.Each<Components::ParticleEmitter>([&](Entity entity, Components::ParticleEmitter& emitter) {
                const auto* layer = world.TryGet<Components::Layer>(entity);
                const uint16_t layerId = layer ? layer->Id : 0;
                processEmitter(entity, emitter, layerId);
            });
        }

        // ==============================================================
        // Phase 3: ONE sync, then bulk-unmap write buffers
        // ==============================================================
#ifdef GRAPE_HAS_CUDA
        cudaStreamSynchronize(m_cudaStream);

        std::vector<cudaGraphicsResource*> resourcesToUnmap;
        for (auto& [id, gpu] : m_emitters) {
            if (gpu.d_mappedVBO) {
                uint8_t mappedIdx = gpu.mappedWriteIdx;
                if (gpu.cudaVBO[mappedIdx]) {
                    resourcesToUnmap.push_back(gpu.cudaVBO[mappedIdx]);
                }
                gpu.d_mappedVBO = nullptr;
            }
        }

        if (!resourcesToUnmap.empty()) {
            cudaGraphicsUnmapResources((int)resourcesToUnmap.size(), resourcesToUnmap.data(), 0);
        }
#endif

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
            for (int i = 0; i < 2; ++i)
                if (gpu.cudaVBO[i]) CudaGL::UnregisterBuffer(gpu.cudaVBO[i]);

            if (gpu.d_posVel)    cudaFree(gpu.d_posVel);
            if (gpu.d_lifeColor) cudaFree(gpu.d_lifeColor);
            if (gpu.d_sizeRot)   cudaFree(gpu.d_sizeRot);

            if (gpu.d_aliveFlags) cudaFree(gpu.d_aliveFlags);
            if (gpu.d_offsets)    cudaFree(gpu.d_offsets);
            if (gpu.d_totalAlive) cudaFree(gpu.d_totalAlive);
            if (gpu.h_totalAlive) cudaFreeHost(gpu.h_totalAlive);

            if (gpu.d_tempPV) cudaFree(gpu.d_tempPV);
            if (gpu.d_tempLC) cudaFree(gpu.d_tempLC);
            if (gpu.d_tempSR) cudaFree(gpu.d_tempSR);

            if (gpu.d_scanTemp) cudaFree(gpu.d_scanTemp);
#endif
            for (int i = 0; i < 2; ++i) {
                if (gpu.vao[i])         glDeleteVertexArrays(1, &gpu.vao[i]);
                if (gpu.instanceVBO[i]) glDeleteBuffers(1, &gpu.instanceVBO[i]);
            }
            if (gpu.quadVBO) glDeleteBuffers(1, &gpu.quadVBO);
        }

#ifdef GRAPE_HAS_CUDA
        if (m_cudaStream) {
            cudaStreamDestroy(m_cudaStream);
            m_cudaStream = nullptr;
        }

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
        gpu.bufferIndex = 0;

        for (int i = 0; i < 2; ++i) {
            glGenBuffers(1, &gpu.instanceVBO[i]);
            glBindBuffer(GL_ARRAY_BUFFER, gpu.instanceVBO[i]);
            glBufferData(GL_ARRAY_BUFFER,
                sizeof(float) * 4 * 3 * maxParticles,
                nullptr,
                GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

#ifdef GRAPE_HAS_CUDA
            gpu.cudaVBO[i] = CudaGL::RegisterBuffer(gpu.instanceVBO[i]);
#endif
        }

#ifdef GRAPE_HAS_CUDA
        cudaMalloc(&gpu.d_posVel, sizeof(float4) * maxParticles);
        cudaMalloc(&gpu.d_lifeColor, sizeof(float4) * maxParticles);
        cudaMalloc(&gpu.d_sizeRot, sizeof(float4) * maxParticles);

        cudaMalloc(&gpu.d_aliveFlags, sizeof(int) * maxParticles);
        cudaMalloc(&gpu.d_offsets, sizeof(int) * maxParticles);
        cudaMalloc(&gpu.d_totalAlive, sizeof(int));
        cudaHostAlloc(&gpu.h_totalAlive, sizeof(int), cudaHostAllocDefault);
        *gpu.h_totalAlive = 0;

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

        CreateQuadVAO(gpu, 0, maxParticles);
        CreateQuadVAO(gpu, 1, maxParticles);

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
        for (int i = 0; i < 2; ++i) {
            if (gpu.cudaVBO[i]) { CudaGL::UnregisterBuffer(gpu.cudaVBO[i]); gpu.cudaVBO[i] = nullptr; }
        }

        if (gpu.d_posVel) { cudaFree(gpu.d_posVel);    gpu.d_posVel = nullptr; }
        if (gpu.d_lifeColor) { cudaFree(gpu.d_lifeColor); gpu.d_lifeColor = nullptr; }
        if (gpu.d_sizeRot) { cudaFree(gpu.d_sizeRot);   gpu.d_sizeRot = nullptr; }

        if (gpu.d_aliveFlags) { cudaFree(gpu.d_aliveFlags); gpu.d_aliveFlags = nullptr; }
        if (gpu.d_offsets) { cudaFree(gpu.d_offsets);    gpu.d_offsets = nullptr; }
        if (gpu.d_totalAlive) { cudaFree(gpu.d_totalAlive); gpu.d_totalAlive = nullptr; }
        if (gpu.h_totalAlive) { cudaFreeHost(gpu.h_totalAlive); gpu.h_totalAlive = nullptr; }

        if (gpu.d_tempPV) { cudaFree(gpu.d_tempPV); gpu.d_tempPV = nullptr; }
        if (gpu.d_tempLC) { cudaFree(gpu.d_tempLC); gpu.d_tempLC = nullptr; }
        if (gpu.d_tempSR) { cudaFree(gpu.d_tempSR); gpu.d_tempSR = nullptr; }

        if (gpu.d_scanTemp) { cudaFree(gpu.d_scanTemp); gpu.d_scanTemp = nullptr; }
#endif
        for (int i = 0; i < 2; ++i) {
            if (gpu.vao[i]) { glDeleteVertexArrays(1, &gpu.vao[i]); gpu.vao[i] = 0; }
            if (gpu.instanceVBO[i]) { glDeleteBuffers(1, &gpu.instanceVBO[i]); gpu.instanceVBO[i] = 0; }
        }
        if (gpu.quadVBO) { glDeleteBuffers(1, &gpu.quadVBO); gpu.quadVBO = 0; }

        m_emitters.erase(it);
        LOG_INFO("[ParticleSystem] Destroyed emitter for entity " << entityIndex);
    }

    void ParticleSystem::CreateQuadVAO(EmitterGPUData& gpu, int slot, int /*maxParticles*/) {
        float quadVertices[] = {
            -0.5f, -0.5f,  0.0f, 0.0f,
             0.5f, -0.5f,  1.0f, 0.0f,
             0.5f,  0.5f,  1.0f, 1.0f,
            -0.5f, -0.5f,  0.0f, 0.0f,
             0.5f,  0.5f,  1.0f, 1.0f,
            -0.5f,  0.5f,  0.0f, 1.0f,
        };

        glGenVertexArrays(1, &gpu.vao[slot]);
        glBindVertexArray(gpu.vao[slot]);

        if (slot == 0) {
            glGenBuffers(1, &gpu.quadVBO);
            glBindBuffer(GL_ARRAY_BUFFER, gpu.quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        }
        else {
            glBindBuffer(GL_ARRAY_BUFFER, gpu.quadVBO);
        }

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindBuffer(GL_ARRAY_BUFFER, gpu.instanceVBO[slot]);
        GLsizei stride = 3 * 4 * sizeof(float);

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribDivisor(2, 1);

        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glVertexAttribDivisor(3, 1);

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
        (void)viewProj;
        (void)lights;

        for (const auto& [entityId, emitter] : m_renderData) {
            if (emitter.layerId != layerId) continue;
            if (emitter.aliveCount <= 0 || emitter.vao == 0) continue;

            shader.setUniform("uParticleSize", emitter.particleSize);

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

    void ParticleSystem::DrawEmitterForEntity(uint32_t entityIndex, Shader& shader, World& world) {
        auto it = m_renderData.find(entityIndex);
        if (it == m_renderData.end()) return;
        const EmitterRenderData& emitter = it->second;
        if (emitter.aliveCount <= 0 || emitter.vao == 0) return;

        shader.setUniform("uParticleSize", emitter.particleSize);

        ECS::Entity e{ entityIndex };
        const bool hasMaterial = world.Has<Components::Material2D>(e);

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
} // namespace ECS