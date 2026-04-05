/* Start Header *****************************************************************/
/*!
\file     CudaParticles.cuh
\author   Choi Meng Yew (100%)
\date     28th February 2026
\brief
CUDA particle system kernel declarations. Updated with per-emitter stream
support, fused update+markAlive, and deferred totalAlive readback.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#ifdef GRAPE_HAS_CUDA

#include <cuda_runtime.h>
#include <cstdint>

// Emission shape enum
enum class EmissionShape : int {
    Point = 0,
    Circle = 1,
    Line = 2
};

// All parameters needed by CUDA kernels (passed by value)
struct ParticleParams {
    float emitterX = 0.0f;
    float emitterY = 0.0f;

    float emissionAngle = 0.0f;
    float emissionSpread = 0.0f;
    float emissionRadius = 0.0f;
    EmissionShape shape = EmissionShape::Point;

    float lifetimeMin = 1.0f;
    float lifetimeMax = 2.0f;
    float speedMin = 0.5f;
    float speedMax = 1.5f;

    float gravityX = 0.0f;
    float gravityY = 0.0f;
    float drag = 0.0f;
    float turbulence = 0.0f;
    float wobbleFrequency = 0.0f;
    float wobbleAmplitude = 0.0f;

    float sizeStart = 1.0f;
    float sizeEnd = 0.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;

    float colorStartR = 1.0f, colorStartG = 1.0f, colorStartB = 1.0f, colorStartA = 1.0f;
    float colorEndR = 1.0f, colorEndG = 1.0f, colorEndB = 1.0f, colorEndA = 0.0f;

    bool dieOnCollision = false;
    float bounciness = 0.0f;
    bool killOutOfBounds = false;

    float boundsMinX = -1000.0f, boundsMaxX = 1000.0f;
    float boundsMinY = -1000.0f, boundsMaxY = 1000.0f;

    int maxParticles = 0;
    int emitCount = 0;
    int burstCount = 0;
    int aliveCount = 0;

    float dt = 0.0f;
    float totalTime = 0.0f;
    unsigned int frameCount = 0;

    // Collision grid (device pointers)
    uint8_t* collisionMasks = nullptr;
    int32_t collisionWidth = 0;
    int32_t collisionHeight = 0;
    int32_t collisionOriginX = 0;
    int32_t collisionOriginY = 0;
    float tileSize = 0.0f;
};

namespace CudaParticles {

    /**
     * @brief Advance alive particles one simulation step and write per-particle alive flags.
     *        d_aliveFlags is populated here so Compact can skip its own mark pass.
     * @param posVel Device array of particle position (xy) and velocity (zw).
     * @param lifeColor Device array of particle lifetime and color data.
     * @param sizeRot Device array of particle size and rotation data.
     * @param d_aliveFlags Device array receiving 1 for each alive particle, 0 for dead.
     * @param params Particle simulation parameters for this frame.
     * @param stream CUDA stream to use for asynchronous execution.
     */
    void Update(float4* posVel, float4* lifeColor, float4* sizeRot,
        int* d_aliveFlags,
        const ParticleParams& params, cudaStream_t stream = 0);

    /**
     * @brief Emit new particles into available dead slots.
     * @param posVel Device array of particle position (xy) and velocity (zw).
     * @param lifeColor Device array of particle lifetime and color data.
     * @param sizeRot Device array of particle size and rotation data.
     * @param params Particle emission parameters including emitter position and count.
     * @param stream CUDA stream to use for asynchronous execution.
     * @return Number of particles successfully emitted.
     */
    int  Emit(float4* posVel, float4* lifeColor, float4* sizeRot,
        const ParticleParams& params, cudaStream_t stream = 0);

    /**
     * @brief Compact alive particles to a contiguous range using pre-populated alive flags.
     *        Expects d_aliveFlags to already be written by Update.
     *        h_totalAlive is pinned host memory for deferred async readback.
     * @param posVel Device array of particle position and velocity data.
     * @param lifeColor Device array of particle lifetime and color data.
     * @param sizeRot Device array of particle size and rotation data.
     * @param aliveCount Current alive particle count before compaction.
     * @param maxParticles Maximum particle capacity of the buffers.
     * @param d_aliveFlags Device array of alive flags populated by Update.
     * @param d_offsets Device scratch array for prefix-sum offsets.
     * @param d_totalAlive Device scalar receiving the post-compaction alive count.
     * @param h_totalAlive Pinned host scalar for deferred asynchronous readback.
     * @param d_tempPV Temporary device buffer for position/velocity during compaction.
     * @param d_tempLC Temporary device buffer for life/color during compaction.
     * @param d_tempSR Temporary device buffer for size/rotation during compaction.
     * @param d_scanTemp Temporary device buffer for the prefix-sum scan.
     * @param scanTempBytes Size in bytes of d_scanTemp (from GetScanTempBytes).
     * @param stream CUDA stream to use for asynchronous execution.
     * @return Alive particle count after compaction.
     */
    int  Compact(float4* posVel, float4* lifeColor, float4* sizeRot,
        int aliveCount, int maxParticles,
        int* d_aliveFlags, int* d_offsets, int* d_totalAlive,
        int* h_totalAlive,
        float4* d_tempPV, float4* d_tempLC, float4* d_tempSR,
        void* d_scanTemp, size_t scanTempBytes,
        cudaStream_t stream = 0);

    /**
     * @brief Interleave SoA particle buffers into the mapped OpenGL VBO for rendering.
     * @param dst Destination device pointer mapped from the GL VBO.
     * @param posVel Source device array of particle position and velocity data.
     * @param lifeColor Source device array of particle lifetime and color data.
     * @param sizeRot Source device array of particle size and rotation data.
     * @param count Number of alive particles to interleave.
     * @param stream CUDA stream to use for asynchronous execution.
     */
    void Interleave(float4* dst, const float4* posVel,
        const float4* lifeColor, const float4* sizeRot,
        int count, cudaStream_t stream = 0);

    /**
     * @brief Query the temporary device memory required for the prefix-sum scan.
     * @param count Number of elements to scan.
     * @param d_in Device input array (used to determine algorithm requirements).
     * @param d_out Device output array (used to determine algorithm requirements).
     * @return Required scratch buffer size in bytes.
     */
    size_t GetScanTempBytes(int count, int* d_in, int* d_out);

} // namespace CudaParticles

#endif // GRAPE_HAS_CUDA
