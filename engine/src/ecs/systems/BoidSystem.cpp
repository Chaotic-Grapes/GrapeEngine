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
#include "services/TimeSystem.h"
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
                m_renderData[id] = FlockRenderData{
                    it->second.vao,
                    flock.count,
                    flock.textureId,
                    flock.boidSize
                };

#ifdef GRAPE_HAS_CUDA
                // --------------------------------------------------
                // 2. Run CUDA simulation kernel
                // --------------------------------------------------
                FlockGPUData& gpu = it->second;
                if (!gpu.initialized || !gpu.cudaVBO) return;

                // Map the GL VBO for CUDA write access
                float4* d_posVel = CudaGL::Map<float4>(gpu.cudaVBO);
                if (!d_posVel) return;

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

                // TileMap collision avoidance
                params.collisionMasks = m_collisionGrid.d_masks;
                params.collisionWidth = m_collisionGrid.width;
                params.collisionHeight = m_collisionGrid.height;
                params.collisionOriginX = m_collisionGrid.originX;
                params.collisionOriginY = m_collisionGrid.originY;
                params.tileSize = m_collisionGrid.tileSize;
                params.collisionAvoidWeight = flock.collisionAvoidWeight;
                params.collisionAvoidRadius = flock.collisionAvoidRadius;
                params.frameCount = (unsigned int)TimeSystem::Instance().GetFrameCount();

                // World bounds — use a generous default for now
                // TODO: make configurable per flock or derive from camera
                params.boundsMinX = -500.0f;
                params.boundsMinY = -500.0f;
                params.boundsMaxX = 500.0f;
                params.boundsMaxY = 500.0f;

                LOG_INFO("[BoidSystem] BoidParams collision:"
                    << " masks=" << (params.collisionMasks ? "valid" : "NULL")
                    << " w=" << params.collisionWidth
                    << " h=" << params.collisionHeight
                    << " originX=" << params.collisionOriginX
                    << " originY=" << params.collisionOriginY
                    << " tileSize=" << params.tileSize
                    << " avoidWeight=" << params.collisionAvoidWeight);

                // Launch: reads from prev, writes to current
                CudaBoids::Launch(d_posVel, gpu.d_prevPosVel, params);

                // Copy current -> prev for next frame
                cudaMemcpy(gpu.d_prevPosVel, d_posVel,
                    sizeof(float4) * flock.count,
                    cudaMemcpyDeviceToDevice);

                // Unmap so OpenGL can use the VBO for rendering
                CudaGL::Unmap(gpu.cudaVBO);
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
            if (gpu.cudaVBO)
                CudaGL::UnregisterBuffer(gpu.cudaVBO);

            if (gpu.d_prevPosVel)
                cudaFree(gpu.d_prevPosVel);
#endif

            if (gpu.vao) glDeleteVertexArrays(1, &gpu.vao);
            if (gpu.quadVBO) glDeleteBuffers(1, &gpu.quadVBO);
            if (gpu.instanceVBO) glDeleteBuffers(1, &gpu.instanceVBO);
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

        // Create the OpenGL instance VBO (float4 per boid: pos.xy, vel.xy)
        glGenBuffers(1, &gpu.instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, gpu.instanceVBO);
        glBufferData(GL_ARRAY_BUFFER,
            sizeof(float) * 4 * count,
            nullptr,
            GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

#ifdef GRAPE_HAS_CUDA
        // Register VBO with CUDA
        gpu.cudaVBO = CudaGL::RegisterBuffer(gpu.instanceVBO);

        // Allocate CUDA-only "previous frame" buffer
        cudaMalloc(&gpu.d_prevPosVel, sizeof(float4) * count);

        // Initialize boid positions randomly
        float4* d_posVel = CudaGL::Map<float4>(gpu.cudaVBO);
        if (d_posVel) {
            CudaBoids::InitRandom(d_posVel, count,
                -200.0f, -200.0f,
                200.0f, 200.0f,
                100.0f,
                entityIndex);
            // Copy initial state to prev buffer
            cudaMemcpy(gpu.d_prevPosVel, d_posVel,
                sizeof(float4) * count,
                cudaMemcpyDeviceToDevice);
            CudaGL::Unmap(gpu.cudaVBO);
        }
#endif

        // Create the VAO that binds the unit quad + instance data
        CreateQuadVAO(gpu, count);

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
        if (gpu.cudaVBO) {
            CudaGL::UnregisterBuffer(gpu.cudaVBO);
            gpu.cudaVBO = nullptr;
        }
        if (gpu.d_prevPosVel) {
            cudaFree(gpu.d_prevPosVel);
            gpu.d_prevPosVel = nullptr;
        }
#endif

        if (gpu.vao) { glDeleteVertexArrays(1, &gpu.vao);   gpu.vao = 0; }
        if (gpu.quadVBO) { glDeleteBuffers(1, &gpu.quadVBO);    gpu.quadVBO = 0; }
        if (gpu.instanceVBO) { glDeleteBuffers(1, &gpu.instanceVBO); gpu.instanceVBO = 0; }

        m_flocks.erase(it);

        LOG_INFO("[BoidSystem] Destroyed flock for entity " << entityIndex);
    }

    void BoidSystem::CreateQuadVAO(FlockGPUData& gpu, int /*boidCount*/) {
        // Unit quad: 2 triangles, each vertex has pos(2) + uv(2)
        // Centered at origin, size 1x1
        float quadVertices[] = {
            // pos.x  pos.y  uv.x  uv.y
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

        // Instance VBO (per-instance data: float4 = pos.xy, vel.xy)
        glBindBuffer(GL_ARRAY_BUFFER, gpu.instanceVBO);

        // layout(location = 2) in vec4 aInstancePosVel
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glVertexAttribDivisor(2, 1);  // advance once per instance

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
} // namespace ECS