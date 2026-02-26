#pragma once

#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"
#include "Export.h"

#include <glad/glad.h>
#include <unordered_map>
#include <cstdint>

#ifdef GRAPE_HAS_CUDA
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#endif

namespace ECS {

    class GRAPEENGINE_API BoidSystem : public ISystem {
    public:
        BoidSystem() = default;
        ~BoidSystem() override = default;

        void OnCreate(World& world) override;
        void OnUpdate(World& world) override;
        void OnDestroy(World& world) override;

        SystemMetadata GetMetadata() const override;
        SystemGroup GetSystemGroup() const override { return SystemGroup::Update; }
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

        // ----------------------------------------------------------------
        // Render data — read by RendererSystem each frame
        // ----------------------------------------------------------------
        struct FlockRenderData {
            GLuint   vao = 0;      // VAO with quad mesh + instanced attributes
            int      count = 0;      // number of active boids
            uint32_t textureId = 0;      // sprite texture for this flock
            float    boidSize = 0.3f;   // world-space size per boid
        };

        const std::unordered_map<uint32_t, FlockRenderData>& GetFlockRenderData() const {
            return m_renderData;
        }

    private:
        // Per-flock GPU state
        struct FlockGPUData {
            GLuint   instanceVBO = 0;     // GL VBO: float4 per boid (pos.xy, vel.xy)
            GLuint   quadVBO = 0;     // unit quad vertices
            GLuint   vao = 0;     // VAO binding quad + instance data
#ifdef GRAPE_HAS_CUDA
            cudaGraphicsResource_t cudaVBO = nullptr;  // CUDA handle for instanceVBO
            float4* d_prevPosVel = nullptr;  // previous frame (CUDA-only buffer)
#endif
            int      count = 0;
            bool     initialized = false;
        };

        std::unordered_map<uint32_t, FlockGPUData>   m_flocks;
        std::unordered_map<uint32_t, FlockRenderData> m_renderData;

        void InitFlock(uint32_t entityIndex, int count);
        void DestroyFlock(uint32_t entityIndex);
        void CreateQuadVAO(FlockGPUData& gpu, int boidCount);
    };

} // namespace ECS