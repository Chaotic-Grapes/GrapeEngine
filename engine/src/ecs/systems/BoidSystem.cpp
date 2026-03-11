/* Start Header *****************************************************************/
/*!
\file   BoidSystem.cpp
\author Choi Meng Yew
\date   26th February 2026
\brief
GPU-accelerated boid flocking simulation using CUDA–OpenGL interop.
Each BoidFlock entity owns a separate GPU-backed flock. Simulation
is executed entirely on the GPU via CUDA kernels implementing
separation, alignment, and cohesion rules.

Per-frame workflow:
    - Map OpenGL instance VBO for CUDA access
    - Launch boid simulation kernel (reads previous state, writes current)
    - Synchronize and unmap VBO
    - RendererSystem draws instances using the updated buffer

The system maintains a double-buffered position/velocity layout
(previous + current) to ensure deterministic updates. Results live
directly in an OpenGL VBO, enabling zero CPU copies and fully
GPU-resident simulation-to-rendering flow.

ISSUES TO DO:
1) Remove the Device-to-Device cudaMemcpy
2) Use CUDA Streams

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/systems/BoidSystem.h"
#include "ecs/Components.h"
#include "services/TimeSystem.h"
#include "core/Logger.h"
#include "Core/World/TileMap.hpp"

#ifdef GRAPE_HAS_CUDA
#include "cuda/CudaBoids.cuh"
#include "cuda/CudaGLInterop.cuh"
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

    void BoidSystem::OnUpdate(World& world) {
        const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());

        // ----------------------------------------------------------
        // 1. Discover flocks: init new ones, detect removed ones
        // ----------------------------------------------------------
        // Track which flocks are still alive this frame
        std::unordered_map<uint32_t, bool> alive;
        for (auto& [id, _] : m_flocks) {
            alive[id] = false;
        }

        world.Each<Components::BoidFlock>(
            [&](Entity entity, Components::BoidFlock& flock) {
                const uint32_t id = entity.Index;
                alive[id] = true;

                auto it = m_flocks.find(id);
                if (it == m_flocks.end()) {
                    // New flock � allocate GPU resources
                    InitFlock(id, flock.count);
                    it = m_flocks.find(id);
                }

                // Handle count change (resize)
                if (it->second.count != flock.count && flock.count > 0) {
                    DestroyFlock(id);
                    InitFlock(id, flock.count);
                    it = m_flocks.find(id);
                }

                // Update render data each frame (texture/size may change in editor)
                // Read layer
                uint16_t layerId = 0;
                if (world.Has<Components::Layer>(entity))
                    layerId = world.Get<Components::Layer>(entity).Id;

                // Read ZIndex
                int zOrder = 0;
                if (world.Has<Components::ZIndex2D>(entity))
                    zOrder = world.Get<Components::ZIndex2D>(entity).ZOrder;

                // Read SpriteRenderer2D for texture + color + emissive
                FlockRenderData rd{};
                rd.vao = it->second.vao[1 - it->second.bufferIndex];
                rd.count = flock.count;
                rd.boidSize = flock.boidSize;
                rd.layerId = layerId;
                rd.zOrder = zOrder;

                if (world.Has<Components::SpriteRenderer2D>(entity)) {
                    const auto& sr = world.Get<Components::SpriteRenderer2D>(entity);
                    rd.textureId = sr.TextureId;
                    rd.emissiveTexId = sr.EmissiveTextureId;
                    rd.emissiveStrength = sr.EmissiveStrength;
                    rd.color = glm::vec4(sr.Color.R, sr.Color.G, sr.Color.B, sr.Color.A);
                }
                else {
                    rd.textureId = 0;
                    rd.emissiveTexId = 0;
                    rd.emissiveStrength = 0.0f;
                    rd.color = glm::vec4(1.0f);
                }

                // Read Material2D for PBR lighting (optional)
                if (world.Has<Components::Material2D>(entity)) {
                    const auto& mat = world.Get<Components::Material2D>(entity);
                    rd.hasMaterial = true;
                    rd.normalTexId = mat.NormalTextureId != 0 ? mat.NormalTextureId : 0;
                    rd.mraTexId = mat.MRA_TextureId;
                    rd.metallic = mat.Metallic;
                    rd.smoothness = mat.Smoothness;
                    rd.aoStrength = mat.AOStrength;
                    rd.normalStrength = mat.NormalStrength;
                    rd.materialFlags = mat.Flags;
                }

                m_renderData[id] = rd;

#ifdef GRAPE_HAS_CUDA
                // --------------------------------------------------
                // 2. Run CUDA simulation kernel
                // --------------------------------------------------
                FlockGPUData& gpu = it->second;
                if (!gpu.initialized) return;

                const uint8_t readIdx = gpu.bufferIndex;        // prev frame's output
                const uint8_t writeIdx = 1 - gpu.bufferIndex;    // this frame's target

                if (!gpu.cudaVBO[readIdx] || !gpu.cudaVBO[writeIdx]) return;

                const float4* d_prev = CudaGL::Map<float4>(gpu.cudaVBO[readIdx]);
                float4* d_cur = CudaGL::Map<float4>(gpu.cudaVBO[writeIdx]);

                if (!d_prev || !d_cur) {
                    if (d_prev) CudaGL::Unmap(gpu.cudaVBO[readIdx]);
                    if (d_cur)  CudaGL::Unmap(gpu.cudaVBO[writeIdx]);
                    return;
                }

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

                // Spatial hashing stuff
                params.cellSize = flock.visualRange;  // cell = visual range so 3x3 covers all neighbors
                params.hashTableSize = gpu.hashTableSize;
                params.d_cellIds = gpu.d_cellIds;
                params.d_boidIds = gpu.d_boidIds;
                params.d_cellStart = gpu.d_cellStart;

                CudaBoids::Launch(d_cur, d_prev, params, gpu.stream);

                CudaGL::Unmap(gpu.cudaVBO[readIdx]);
                CudaGL::Unmap(gpu.cudaVBO[writeIdx]);

                // Flip — no memcpy, just a bit toggle
                gpu.bufferIndex = writeIdx;
#endif
            });

        // ----------------------------------------------------------
        // 3. Clean up removed flocks
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
        // Pick a prime just larger than count for good hash distribution

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
        // Initialize boid positions into buffer 0
        float4* d_posVel = CudaGL::Map<float4>(gpu.cudaVBO[0]);
        if (d_posVel) {
            CudaBoids::InitRandom(d_posVel, count,
                -200.0f, -200.0f,
                200.0f, 200.0f,
                100.0f,
                entityIndex,
                m_collisionGrid.d_masks,
                m_collisionGrid.width,
                m_collisionGrid.height,
                m_collisionGrid.originX,
                m_collisionGrid.originY,
                m_collisionGrid.tileSize);
            CudaGL::Unmap(gpu.cudaVBO[0]);
        }

        // Copy buffer 0 -> buffer 1 so prev is also initialized
        // (only needed at init, not every frame)
        {
            float4* src = CudaGL::Map<float4>(gpu.cudaVBO[0]);
            float4* dst = CudaGL::Map<float4>(gpu.cudaVBO[1]);
            if (src && dst)
                cudaMemcpy(dst, src, sizeof(float4) * count, cudaMemcpyDeviceToDevice);
            CudaGL::Unmap(gpu.cudaVBO[0]);
            CudaGL::Unmap(gpu.cudaVBO[1]);
        }
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
        for (int i = 0; i < 2; ++i) {
            if (gpu.cudaVBO[i]) {
                CudaGL::UnregisterBuffer(gpu.cudaVBO[i]);
                gpu.cudaVBO[i] = nullptr;
            }
        }
        if (gpu.stream) {
            cudaStreamDestroy(gpu.stream);
            gpu.stream = nullptr;
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

        // Quad VBO is shared between both VAOs
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

        // Count non-zero masks to verify collision data exists
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

        // Reallocate if size changed
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

            // Albedo texture
            if (rd.textureId != 0) {
                shader.setUniform("uHasTexture", 1);
                shader.setUniform("uTexture", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, rd.textureId);
            }
            else {
                shader.setUniform("uHasTexture", 0);
            }

            // Emissive
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

            // Material2D — lighting
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