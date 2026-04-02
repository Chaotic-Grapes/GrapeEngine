/* Start Header *****************************************************************/
/*!
\file   BoidSystem.cpp
\author Choi Meng Yew (100%)
\date   26th February 2026
\brief
GPU-accelerated boid flocking simulation using CUDA-OpenGL interop.
Each BoidFlock entity owns a separate GPU-backed flock. Simulation
is executed entirely on the GPU via CUDA kernels implementing
separation, alignment, and cohesion rules.

Per-frame workflow (optimized):
    Phase 1: Bulk-map ALL flock VBOs in ONE cudaGraphicsMapResources call
    Phase 2: Launch all CUDA kernels on per-flock streams
    Phase 3: Sync streams, bulk-unmap ALL VBOs in ONE call

Optimizations:
    - Single cudaGraphicsMapResources/UnmapResources call per frame
      (not one per VBO). This is the key perf win for many flocks.
    - Per-flock CUDA streams for GPU overlap
    - cudaStreamSynchronize before unmap for correctness

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/BoidSystem.h"
#include "ecs/Components.h"
#include "services/TimeSystem.h"
#include "core/Logger.h"
#include "scene/LayerManager.h"
#include "Core/World/TileMap.hpp"

#ifdef GRAPE_HAS_CUDA
#include "cuda/CudaBoids.cuh"
#include "cuda/CudaGLInterop.cuh"
#include <vector>
#endif

namespace ECS {

    // ================================================================
    // Metadata
    // ================================================================
    SystemMetadata BoidSystem::GetMetadata() const {
        ComponentAccessBuilder builder("BoidSystem");
        builder.SetExecutionOrder(50);       // run before RendererSystem
        builder.SetGroup(SystemGroup::Update);
        builder.SetRunMode(SystemRunMode::PlayOnly);
        return builder.Build();
    }

    // ================================================================
    // Lifecycle
    // ================================================================
    void BoidSystem::OnCreate(World& /*world*/) {
        LOG_INFO("[BoidSystem] OnCreate");
    }

    /**
     * @brief Simulate boid flocks and refresh render payloads.
     * @param world ECS world containing boid flock components.
     * @return void
     * @note Traverses entities by layer first when LayerManager exists.
     * @note Flocks without a Layer component are processed in a fallback pass.
     * @complexity O(sum(layer entities) + unlayered flocks + GPU simulation work)
     */
    void BoidSystem::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
        auto* layerManager = world.GetLayerManager();

        // ----------------------------------------------------------
        // Track which flocks are still alive this frame
        // ----------------------------------------------------------
        std::unordered_map<uint32_t, bool> alive;
        for (auto& [id, _] : m_flocks) {
            alive[id] = false;
        }

        // ==============================================================
        // Phase 1: Bulk-map ALL flock VBOs in a SINGLE driver call
        // ==============================================================
#ifdef GRAPE_HAS_CUDA
        // Collect all CUDA graphics resources to map
        std::vector<cudaGraphicsResource*> resourcesToMap;
        for (auto& [id, gpu] : m_flocks) {
            gpu.d_mappedPrev = nullptr;
            gpu.d_mappedCur = nullptr;
            if (gpu.initialized && gpu.cudaVBO[0] && gpu.cudaVBO[1]) {
                resourcesToMap.push_back(gpu.cudaVBO[gpu.bufferIndex]);       // read (prev)
                resourcesToMap.push_back(gpu.cudaVBO[1 - gpu.bufferIndex]);   // write (cur)
            }
        }

        // ONE map call for all resources
        if (!resourcesToMap.empty()) {
            cudaGraphicsMapResources((int)resourcesToMap.size(), resourcesToMap.data(), 0);
        }

        // Get device pointers (cheap, no sync)
        for (auto& [id, gpu] : m_flocks) {
            if (gpu.initialized && gpu.cudaVBO[0] && gpu.cudaVBO[1]) {
                size_t size = 0;
                cudaGraphicsResourceGetMappedPointer(
                    (void**)&gpu.d_mappedPrev, &size, gpu.cudaVBO[gpu.bufferIndex]);
                cudaGraphicsResourceGetMappedPointer(
                    (void**)&gpu.d_mappedCur, &size, gpu.cudaVBO[1 - gpu.bufferIndex]);
            }
        }
#endif

        // ==============================================================
        // Phase 2: Simulate all flocks (per-flock streams, no sync)
        // ==============================================================
        auto processFlock = [&](Entity entity, Components::BoidFlock& flock, const uint16_t resolvedLayerId) {
                const uint32_t id = entity.Index;
                alive[id] = true;

                auto it = m_flocks.find(id);
                if (it == m_flocks.end()) {
                    // New flock - allocate GPU resources
                    InitFlock(id, flock.count);
                    it = m_flocks.find(id);

#ifdef GRAPE_HAS_CUDA
                    // Late-map this newly created flock (individual map, unavoidable)
                    FlockGPUData& newGpu = it->second;
                    if (newGpu.initialized && newGpu.cudaVBO[0] && newGpu.cudaVBO[1]) {
                        cudaGraphicsResource* lateRes[2] = {
                            newGpu.cudaVBO[newGpu.bufferIndex],
                            newGpu.cudaVBO[1 - newGpu.bufferIndex]
                        };
                        cudaGraphicsMapResources(2, lateRes, 0);
                        size_t size = 0;
                        cudaGraphicsResourceGetMappedPointer(
                            (void**)&newGpu.d_mappedPrev, &size, lateRes[0]);
                        cudaGraphicsResourceGetMappedPointer(
                            (void**)&newGpu.d_mappedCur, &size, lateRes[1]);

                        if (newGpu.needsInit && newGpu.d_mappedCur && newGpu.d_mappedPrev) {
                            CudaBoids::InitRandom(newGpu.d_mappedCur, flock.count,
                                -200.0f, -200.0f, 200.0f, 200.0f, 100.0f, id,
                                m_collisionGrid.d_masks, m_collisionGrid.width,
                                m_collisionGrid.height, m_collisionGrid.originX,
                                m_collisionGrid.originY, m_collisionGrid.tileSize);
                            cudaMemcpy(newGpu.d_mappedPrev, newGpu.d_mappedCur,
                                sizeof(float4) * flock.count, cudaMemcpyDeviceToDevice);
                            newGpu.needsInit = false;
                        }
                    }
#endif
                }

                // Handle count change (resize)
                if (it->second.count != flock.count && flock.count > 0) {
#ifdef GRAPE_HAS_CUDA
                    // Unmap before destroy
                    FlockGPUData& oldGpu = it->second;
                    if (oldGpu.d_mappedPrev || oldGpu.d_mappedCur) {
                        cudaGraphicsResource* unmapRes[2] = {
                            oldGpu.cudaVBO[0], oldGpu.cudaVBO[1]
                        };
                        cudaGraphicsUnmapResources(2, unmapRes, 0);
                        oldGpu.d_mappedPrev = nullptr;
                        oldGpu.d_mappedCur = nullptr;
                    }
#endif
                    DestroyFlock(id);
                    InitFlock(id, flock.count);
                    it = m_flocks.find(id);

#ifdef GRAPE_HAS_CUDA
                    FlockGPUData& newGpu = it->second;
                    if (newGpu.initialized && newGpu.cudaVBO[0] && newGpu.cudaVBO[1]) {
                        cudaGraphicsResource* lateRes[2] = {
                            newGpu.cudaVBO[newGpu.bufferIndex],
                            newGpu.cudaVBO[1 - newGpu.bufferIndex]
                        };
                        cudaGraphicsMapResources(2, lateRes, 0);
                        size_t size = 0;
                        cudaGraphicsResourceGetMappedPointer(
                            (void**)&newGpu.d_mappedPrev, &size, lateRes[0]);
                        cudaGraphicsResourceGetMappedPointer(
                            (void**)&newGpu.d_mappedCur, &size, lateRes[1]);

                        if (newGpu.needsInit && newGpu.d_mappedCur && newGpu.d_mappedPrev) {
                            CudaBoids::InitRandom(newGpu.d_mappedCur, flock.count,
                                -200.0f, -200.0f, 200.0f, 200.0f, 100.0f, id,
                                m_collisionGrid.d_masks, m_collisionGrid.width,
                                m_collisionGrid.height, m_collisionGrid.originX,
                                m_collisionGrid.originY, m_collisionGrid.tileSize);
                            cudaMemcpy(newGpu.d_mappedPrev, newGpu.d_mappedCur,
                                sizeof(float4) * flock.count, cudaMemcpyDeviceToDevice);
                            newGpu.needsInit = false;
                        }
                    }
#endif
                }

                // Update render data each frame (texture/size may change in editor)
                int zOrder = 0;
                if (const auto* z = world.TryGet<Components::ZIndex2D>(entity)) {
                    zOrder = z->ZOrder;
                }

                FlockRenderData rd{};
                rd.vao = it->second.vao[1 - it->second.bufferIndex];
                rd.count = flock.count;
                rd.boidSize = flock.boidSize;
                rd.layerId = resolvedLayerId;
                rd.zOrder = zOrder;

                if (const auto* sr = world.TryGet<Components::SpriteRenderer2D>(entity)) {
                    rd.textureId = sr->TextureId;
                    rd.emissiveTexId = sr->EmissiveTextureId;
                    rd.emissiveStrength = sr->EmissiveStrength;
                    rd.color = glm::vec4(sr->Color.R, sr->Color.G, sr->Color.B, sr->Color.A);
                }
                else {
                    rd.textureId = 0;
                    rd.emissiveTexId = 0;
                    rd.emissiveStrength = 0.0f;
                    rd.color = glm::vec4(1.0f);
                }

                if (const auto* mat = world.TryGet<Components::Material2D>(entity)) {
                    rd.hasMaterial = true;
                    rd.normalTexId = mat->NormalTextureId != 0 ? mat->NormalTextureId : 0;
                    rd.mraTexId = mat->MRA_TextureId;
                    rd.metallic = mat->Metallic;
                    rd.smoothness = mat->Smoothness;
                    rd.aoStrength = mat->AOStrength;
                    rd.normalStrength = mat->NormalStrength;
                    rd.materialFlags = mat->Flags;
                }

                m_renderData[id] = rd;

#ifdef GRAPE_HAS_CUDA
                // --------------------------------------------------
                // Run CUDA simulation kernel on per-flock stream
                // --------------------------------------------------
                FlockGPUData& gpu = it->second;
                if (!gpu.initialized) return;
                if (!gpu.d_mappedPrev || !gpu.d_mappedCur) return;

                // Build kernel params from component
                BoidParams params{};
                params.separationWeight = flock.separationWeight;
                params.alignmentWeight = flock.alignmentWeight;
                params.cohesionWeight = flock.cohesionWeight;
                params.visualRange = flock.visualRange;
                params.maxSpeed = flock.maxSpeed;
                params.maxForce = flock.maxForce;
                params.dt = dt;
                params.count = flock.count;
                params.collisionMasks = m_collisionGrid.d_masks;
                params.collisionWidth = m_collisionGrid.width;
                params.collisionHeight = m_collisionGrid.height;
                params.collisionOriginX = m_collisionGrid.originX;
                params.collisionOriginY = m_collisionGrid.originY;
                params.tileSize = m_collisionGrid.tileSize;
                params.collisionAvoidWeight = flock.collisionAvoidWeight;
                params.collisionAvoidRadius = flock.collisionAvoidRadius;
                params.frameCount = (unsigned int)TimeSystem::Instance().GetFrameCount();
                params.boundsMinX = -500.0f;
                params.boundsMinY = -500.0f;
                params.boundsMaxX = 500.0f;
                params.boundsMaxY = 500.0f;

                // Spatial hashing
                params.cellSize = flock.visualRange;
                params.hashTableSize = gpu.hashTableSize;
                params.d_cellIds = gpu.d_cellIds;
                params.d_boidIds = gpu.d_boidIds;
                params.d_cellStart = gpu.d_cellStart;

                CudaBoids::Launch(gpu.d_mappedCur, gpu.d_mappedPrev, params, gpu.stream);

                // Flip buffer index (no memcpy, just a bit toggle)
                gpu.bufferIndex = 1 - gpu.bufferIndex;
#endif
            };

        if (layerManager) {
            for (const auto layerId : layerManager->DrawOrder()) {
                if (!layerManager->IsUpdateEnabled(layerId)) {
                    continue;
                }
                for (const Entity entity : layerManager->EntitiesIn(layerId)) {
                    auto* flock = world.TryGet<Components::BoidFlock>(entity);
                    if (!flock) {
                        continue;
                    }
                    processFlock(entity, *flock, layerId);
                }
            }

            // Keep unlayered flocks in parity with previous behavior.
            world.Each<Components::BoidFlock>([&](Entity entity, Components::BoidFlock& flock) {
                if (world.TryGet<Components::Layer>(entity)) {
                    return;
                }
                processFlock(entity, flock, 0);
            });
        } else {
            world.Each<Components::BoidFlock>([&](Entity entity, Components::BoidFlock& flock) {
                const auto* layer = world.TryGet<Components::Layer>(entity);
                const uint16_t layerId = layer ? layer->Id : 0;
                processFlock(entity, flock, layerId);
            });
        }

        // ==============================================================
        // Phase 3: Sync all streams, then bulk-unmap in ONE call
        // ==============================================================
#ifdef GRAPE_HAS_CUDA
        // Sync all streams first (ensure kernels are done writing)
        for (auto& [id, gpu] : m_flocks) {
            if (gpu.stream && (gpu.d_mappedPrev || gpu.d_mappedCur)) {
                cudaStreamSynchronize(gpu.stream);
            }
        }

        // Rebuild the resource list for unmap (same resources we mapped)
        // Note: late-mapped and resize-remapped flocks were added individually,
        // so we just collect everything that's currently mapped.
        std::vector<cudaGraphicsResource*> resourcesToUnmap;
        for (auto& [id, gpu] : m_flocks) {
            if (gpu.d_mappedPrev || gpu.d_mappedCur) {
                if (gpu.cudaVBO[0]) resourcesToUnmap.push_back(gpu.cudaVBO[0]);
                if (gpu.cudaVBO[1]) resourcesToUnmap.push_back(gpu.cudaVBO[1]);
                gpu.d_mappedPrev = nullptr;
                gpu.d_mappedCur = nullptr;
            }
        }

        // ONE unmap call for all resources
        if (!resourcesToUnmap.empty()) {
            cudaGraphicsUnmapResources((int)resourcesToUnmap.size(), resourcesToUnmap.data(), 0);
        }
#endif

        // ----------------------------------------------------------
        // Clean up removed flocks
        // ----------------------------------------------------------
        for (auto& [id, isAlive] : alive) {
            if (!isAlive) {
                DestroyFlock(id);
                m_renderData.erase(id);
            }
        }
    }

    void BoidSystem::OnDestroy(World&)
    {
        LOG_INFO("[BoidSystem] OnDestroy - freeing all GPU resources");

        for (auto& [id, gpu] : m_flocks)
        {
#ifdef GRAPE_HAS_CUDA
            for (int i = 0; i < 2; ++i)
                if (gpu.cudaVBO[i]) CudaGL::UnregisterBuffer(gpu.cudaVBO[i]);
            if (gpu.stream)      cudaStreamDestroy(gpu.stream);

            if (gpu.d_cellIds)   cudaFree(gpu.d_cellIds);
            if (gpu.d_boidIds)   cudaFree(gpu.d_boidIds);
            if (gpu.d_cellStart) cudaFree(gpu.d_cellStart);
#endif
            for (int i = 0; i < 2; ++i) {
                if (gpu.vao[i])         glDeleteVertexArrays(1, &gpu.vao[i]);
                if (gpu.instanceVBO[i]) glDeleteBuffers(1, &gpu.instanceVBO[i]);
            }
            if (gpu.quadVBO) glDeleteBuffers(1, &gpu.quadVBO);
        }

#ifdef GRAPE_HAS_CUDA
        if (m_collisionGrid.d_masks) {
            cudaFree(m_collisionGrid.d_masks);
            m_collisionGrid.d_masks = nullptr;
        }
#endif

        m_flocks.clear();
        m_renderData.clear();
    }

    // ================================================================
    // GPU Resource Management
    // ================================================================

    void BoidSystem::InitFlock(uint32_t entityIndex, int count) {
        if (count <= 0) return;

        FlockGPUData gpu{};
        gpu.count = count;
        gpu.bufferIndex = 0;

        for (int i = 0; i < 2; ++i) {
            glGenBuffers(1, &gpu.instanceVBO[i]);
            glBindBuffer(GL_ARRAY_BUFFER, gpu.instanceVBO[i]);
            glBufferData(GL_ARRAY_BUFFER,
                sizeof(float4) * count,
                nullptr,
                GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

#ifdef GRAPE_HAS_CUDA
            gpu.cudaVBO[i] = CudaGL::RegisterBuffer(gpu.instanceVBO[i]);
#endif
        }

#ifdef GRAPE_HAS_CUDA
        cudaStreamCreate(&gpu.stream);

        gpu.hashTableSize = (count <= 1000) ? 2003 :
            (count <= 10000) ? 20011 :
            (count <= 50000) ? 50021 :
            (count <= 100000) ? 100003 :
            (count <= 200000) ? 200003 :
            (count <= 500000) ? 500009 :
            (count <= 1000000) ? 1000003 :
            2000003;

        cudaMalloc(&gpu.d_cellIds, sizeof(uint32_t) * count);
        cudaMalloc(&gpu.d_boidIds, sizeof(uint32_t) * count);
        cudaMalloc(&gpu.d_cellStart, sizeof(uint32_t) * gpu.hashTableSize);
#endif

#ifdef GRAPE_HAS_CUDA
        gpu.needsInit = true;
        cudaGetLastError();   // clear any latent error
#endif

        // Create one VAO per buffer
        CreateQuadVAO(gpu, 0);
        CreateQuadVAO(gpu, 1);

        gpu.initialized = true;
        m_flocks[entityIndex] = gpu;

        LOG_INFO("[BoidSystem] Initialized flock for entity "
            << entityIndex << " with " << count << " boids");
    }

    void BoidSystem::DestroyFlock(uint32_t entityIndex) {
        auto it = m_flocks.find(entityIndex);
        if (it == m_flocks.end()) return;

        FlockGPUData& gpu = it->second;

#ifdef GRAPE_HAS_CUDA
        if (gpu.stream) {
            cudaStreamSynchronize(gpu.stream);
            cudaStreamDestroy(gpu.stream);
            gpu.stream = nullptr;
        }
        for (int i = 0; i < 2; ++i) {
            if (gpu.cudaVBO[i]) {
                CudaGL::UnregisterBuffer(gpu.cudaVBO[i]);
                gpu.cudaVBO[i] = nullptr;
            }
        }

        if (gpu.d_cellIds) { cudaFree(gpu.d_cellIds);   gpu.d_cellIds = nullptr; }
        if (gpu.d_boidIds) { cudaFree(gpu.d_boidIds);   gpu.d_boidIds = nullptr; }
        if (gpu.d_cellStart) { cudaFree(gpu.d_cellStart); gpu.d_cellStart = nullptr; }
#endif

        for (int i = 0; i < 2; ++i) {
            if (gpu.vao[i]) { glDeleteVertexArrays(1, &gpu.vao[i]);    gpu.vao[i] = 0; }
            if (gpu.instanceVBO[i]) { glDeleteBuffers(1, &gpu.instanceVBO[i]); gpu.instanceVBO[i] = 0; }
        }
        if (gpu.quadVBO) { glDeleteBuffers(1, &gpu.quadVBO); gpu.quadVBO = 0; }

        m_flocks.erase(it);

        LOG_INFO("[BoidSystem] Destroyed flock for entity " << entityIndex);
    }

    void BoidSystem::CreateQuadVAO(FlockGPUData& gpu, int slot) {
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
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glVertexAttribDivisor(2, 1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void BoidSystem::UpdateCollisionGrid(const TileMap& tileMap)
    {
#ifdef GRAPE_HAS_CUDA
        const auto& masks = tileMap.GetCollisionMasks();

        int nonZeroCount = 0;
        for (auto m : masks) if (m != 0) nonZeroCount++;

        LOG_INFO("[BoidSystem] UpdateCollisionGrid:"
            << " masks=" << masks.size()
            << " nonZero=" << nonZeroCount
            << " width=" << tileMap.CollisionWidth()
            << " height=" << tileMap.CollisionHeight()
            << " originX=" << tileMap.OriginX()
            << " originY=" << tileMap.OriginY()
            << " tileSize=" << tileMap.TileSize());

        if (masks.empty()) return;

        const size_t byteCount = masks.size();
        const uint32_t newWidth = tileMap.CollisionWidth();

        if (!m_collisionGrid.d_masks || m_collisionGrid.width != (int32_t)newWidth) {
            cudaFree(m_collisionGrid.d_masks);
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

    void BoidSystem::DrawFlocksByLayer(uint16_t layerId, Shader& shader, const glm::mat4& viewProj) {
        (void)viewProj;

        for (const auto& [id, rd] : m_renderData) {
            if (rd.layerId != layerId) continue;
            if (rd.count <= 0 || rd.vao == 0) continue;

            shader.setUniform("uBoidSize", rd.boidSize);
            shader.setUniform("uColor", rd.color);

            if (rd.textureId != 0) {
                shader.setUniform("uHasTexture", 1);
                shader.setUniform("uTexture", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, rd.textureId);
            }
            else {
                shader.setUniform("uHasTexture", 0);
            }

            shader.setUniform("uEmissiveStrength", rd.emissiveStrength);
            if (rd.emissiveTexId != 0) {
                shader.setUniform("uHasEmissive", 1);
                shader.setUniform("uEmissiveTex", 4);
                glActiveTexture(GL_TEXTURE4);
                glBindTexture(GL_TEXTURE_2D, rd.emissiveTexId);
            }
            else {
                shader.setUniform("uHasEmissive", 0);
            }

            shader.setUniform("uLightingEnabled", rd.hasMaterial ? 1 : 0);
            if (rd.hasMaterial) {
                shader.setUniform("uMetallic", rd.metallic);
                shader.setUniform("uSmoothness", rd.smoothness);
                shader.setUniform("uAOStrength", rd.aoStrength);
                shader.setUniform("uNormalStrength", rd.normalStrength);
                shader.setUniform("uMaterialFlags", (int)rd.materialFlags);

                if (rd.normalTexId != 0) {
                    shader.setUniform("uHasNormalMap", 1);
                    shader.setUniform("uNormalMap", 1);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, rd.normalTexId);
                }
                else {
                    shader.setUniform("uHasNormalMap", 0);
                }

                if (rd.mraTexId != 0) {
                    shader.setUniform("uHasMRAMap", 1);
                    shader.setUniform("uMRAMap", 2);
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, rd.mraTexId);
                }
                else {
                    shader.setUniform("uHasMRAMap", 0);
                }
            }

            glBindVertexArray(rd.vao);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, rd.count);
            glBindVertexArray(0);
        }
    }

    void BoidSystem::DrawFlockForEntity(uint32_t entityIndex, Shader& shader) {
        auto it = m_renderData.find(entityIndex);
        if (it == m_renderData.end()) return;
        const FlockRenderData& rd = it->second;
        if (rd.count <= 0 || rd.vao == 0) return;

        shader.setUniform("uBoidSize", rd.boidSize);
        shader.setUniform("uColor", rd.color);

        if (rd.textureId != 0) {
            shader.setUniform("uHasTexture", 1);
            shader.setUniform("uTexture", 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, rd.textureId);
        }
        else {
            shader.setUniform("uHasTexture", 0);
        }

        shader.setUniform("uEmissiveStrength", rd.emissiveStrength);
        if (rd.emissiveTexId != 0) {
            shader.setUniform("uHasEmissive", 1);
            shader.setUniform("uEmissiveTex", 4);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, rd.emissiveTexId);
        }
        else {
            shader.setUniform("uHasEmissive", 0);
        }

        shader.setUniform("uLightingEnabled", rd.hasMaterial ? 1 : 0);
        if (rd.hasMaterial) {
            shader.setUniform("uMetallic", rd.metallic);
            shader.setUniform("uSmoothness", rd.smoothness);
            shader.setUniform("uAOStrength", rd.aoStrength);
            shader.setUniform("uNormalStrength", rd.normalStrength);
            shader.setUniform("uMaterialFlags", (int)rd.materialFlags);
            if (rd.normalTexId != 0) {
                shader.setUniform("uHasNormalMap", 1);
                shader.setUniform("uNormalMap", 1);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, rd.normalTexId);
            }
            else {
                shader.setUniform("uHasNormalMap", 0);
            }
            if (rd.mraTexId != 0) {
                shader.setUniform("uHasMRAMap", 1);
                shader.setUniform("uMRAMap", 2);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, rd.mraTexId);
            }
            else {
                shader.setUniform("uHasMRAMap", 0);
            }
        }

        glBindVertexArray(rd.vao);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, rd.count);
        glBindVertexArray(0);
    }
} // namespace ECS