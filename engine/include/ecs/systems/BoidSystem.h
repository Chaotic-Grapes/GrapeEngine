#pragma once

#include "ecs/ISystem.h"
#include "ecs/ComponentAccessAttribute.h"
#include "graphics/shader.hpp"
#include "Export.h"

#include <glad/glad.h>
#include <unordered_map>
#include <cstdint>

#ifdef GRAPE_HAS_CUDA
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#endif

// Forward declare so we don't need to include TileMap.hpp in the header
class TileMap;

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

        void DrawFlocksByLayer(uint16_t layerId, Shader& shader,
            const glm::mat4& viewProj);

        void DrawFlockForEntity(uint32_t entityIndex, Shader& shader);

        // ----------------------------------------------------------------
        // Render data — read by RendererSystem each frame
        // ----------------------------------------------------------------
        struct FlockRenderData {
            GLuint   vao = 0;
            int      count = 0;
            float    boidSize = 1.0f;
            uint16_t layerId = 0;
            int      zOrder = 0;

            // From SpriteRenderer2D
            uint32_t textureId = 0;
            uint32_t emissiveTexId = 0;
            float    emissiveStrength = 0.0f;
            glm::vec4 color = glm::vec4(1.0f);

            // From Material2D (optional — zero means no PBR)
            uint32_t normalTexId = 0;
            uint32_t mraTexId = 0;
            float    metallic = 0.0f;
            float    smoothness = 0.5f;
            float    aoStrength = 1.0f;
            float    normalStrength = 1.0f;
            uint32_t materialFlags = 0;
            bool     hasMaterial = false;
        };

        const std::unordered_map<uint32_t, FlockRenderData>& GetFlockRenderData() const {
            return m_renderData;
        }

        // Collision grid management
        void UpdateCollisionGrid(const TileMap& tileMap);

    private:
        // Per-flock GPU state
        struct FlockGPUData {
            GLuint   instanceVBO[2] = { 0, 0 };
            GLuint   quadVBO = 0;
            GLuint   vao[2] = { 0, 0 };
#ifdef GRAPE_HAS_CUDA
            cudaGraphicsResource_t cudaVBO[2] = { nullptr, nullptr };
            cudaStream_t           stream = nullptr;
            uint32_t* d_cellIds = nullptr;  // cell id per boid
            uint32_t* d_boidIds = nullptr;  // boid indices sorted by cell
            uint32_t* d_cellStart = nullptr;  // start index per cell
            int       hashTableSize = 0;
#endif
            uint8_t  bufferIndex = 0;
            int      count = 0;
            bool     initialized = false;
        };

        // Shared collision grid uploaded once per tilemap change
        struct CollisionGridGPU {
            uint8_t* d_masks = nullptr;  // device pointer, owned by BoidSystem
            int32_t  width = 0;
            int32_t  height = 0;
            int32_t  originX = 0;
            int32_t  originY = 0;
            float    tileSize = 0.0f;
        };

        std::unordered_map<uint32_t, FlockGPUData>   m_flocks;
        std::unordered_map<uint32_t, FlockRenderData> m_renderData;
        CollisionGridGPU                              m_collisionGrid;

        void InitFlock(uint32_t entityIndex, int count);
        void DestroyFlock(uint32_t entityIndex);
        void CreateQuadVAO(FlockGPUData& gpu, int slot);
    };

} // namespace ECS