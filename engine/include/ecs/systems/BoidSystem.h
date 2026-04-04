/* Start Header *****************************************************************/
/*!
\file   BoidSystem.h
\author Choi Meng Yew (100%)
\date   10th March 2026
\brief
Declares the BoidSystem ECS system responsible for managing and
simulating large-scale boid flocking using GPU acceleration.

BoidSystem integrates CUDA compute with OpenGL rendering through
CUDA-OpenGL interoperability, allowing boid simulation data to remain
entirely on the GPU. Each BoidFlock entity owns a dedicated GPU-backed
flock whose position and velocity data are updated every frame by
CUDA kernels implementing the classic boid steering rules:

    - Separation
    - Alignment
    - Cohesion

Optimizations:
    - Batched CudaGL Map/Unmap (2 sync points per frame, not 2*N)
    - Per-flock CUDA streams for GPU overlap
    - Stream sync before unmap for correctness

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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

        /**
         * @brief Initialize GPU buffers and resources for all existing flock entities.
         * @param world ECS world containing entities with BoidFlock components.
         */
        void OnCreate(World& world) override;

        /**
         * @brief Simulate boid steering and update GPU buffers for this frame.
         * @param world ECS world containing entities with BoidFlock components.
         */
        void OnUpdate(World& world) override;

        /**
         * @brief Release all GPU resources owned by flock entities.
         * @param world ECS world passed by the scheduler.
         */
        void OnDestroy(World& world) override;

        /**
         * @brief Return system metadata for scheduler registration.
         * @return SystemMetadata describing component access and execution order.
         */
        SystemMetadata GetMetadata() const override;

        /**
         * @brief Run in the Update group alongside game logic systems.
         * @return SystemGroup::Update.
         */
        SystemGroup GetSystemGroup() const override { return SystemGroup::Update; }

        /**
         * @brief Only run during play mode; boid simulation is inactive in edit mode.
         * @return SystemRunMode::PlayOnly.
         */
        SystemRunMode GetRunMode() const override { return SystemRunMode::PlayOnly; }

        /**
         * @brief Render all flock instances that belong to a specific layer.
         * @param layerId Layer identifier to filter flocks for rendering.
         * @param shader Shader program used to render the boid instances.
         * @param viewProj Combined view-projection matrix for the current camera.
         */
        void DrawFlocksByLayer(uint16_t layerId, Shader& shader,
            const glm::mat4& viewProj);

        /**
         * @brief Render one flock directly by owning entity index.
         * @param entityIndex ECS entity index of the flock owner.
         * @param shader Shader program used to render the boid instances.
         */
        void DrawFlockForEntity(uint32_t entityIndex, Shader& shader);

        // ----------------------------------------------------------------
        // Render data - read by RendererSystem each frame
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

            // From Material2D (optional - zero means no PBR)
            uint32_t normalTexId = 0;
            uint32_t mraTexId = 0;
            float    metallic = 0.0f;
            float    smoothness = 0.5f;
            float    aoStrength = 1.0f;
            float    normalStrength = 1.0f;
            uint32_t materialFlags = 0;
            bool     hasMaterial = false;
        };

        /**
         * @brief Return the per-flock render data map for read-only use by the renderer.
         * @return Reference to the map from entity index to FlockRenderData.
         */
        const std::unordered_map<uint32_t, FlockRenderData>& GetFlockRenderData() const {
            return m_renderData;
        }

        /**
         * @brief Upload and refresh the shared tile collision grid used by boid avoidance.
         * @param tileMap Tile map whose solid-cell data will be copied to the GPU.
         */
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

            // Mapped VBO pointers (valid between batch map/unmap phases)
            float4* d_mappedPrev = nullptr;
            float4* d_mappedCur = nullptr;

            uint32_t* d_cellIds = nullptr;  // cell id per boid
            uint32_t* d_boidIds = nullptr;  // boid indices sorted by cell
            uint32_t* d_cellStart = nullptr;  // start index per cell
            int       hashTableSize = 0;
#endif
            uint8_t  bufferIndex = 0;
            int      count = 0;
            bool     initialized = false;
            bool     needsInit = false;
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

        /**
         * @brief Allocate and initialize GPU buffers and state for one flock entity.
         * @param entityIndex ECS entity index of the flock owner.
         * @param count Number of boids to allocate GPU memory for.
         */
        void InitFlock(uint32_t entityIndex, int count);

        /**
         * @brief Destroy all GPU resources owned by one flock entity.
         * @param entityIndex ECS entity index of the flock owner.
         */
        void DestroyFlock(uint32_t entityIndex);

        /**
         * @brief Build quad geometry and VAO bindings for one render slot.
         * @param gpu GPU state struct to populate with the new VAO and VBO.
         * @param slot Double-buffer slot index (0 or 1) to initialize.
         */
        void CreateQuadVAO(FlockGPUData& gpu, int slot);
    };

} // namespace ECS