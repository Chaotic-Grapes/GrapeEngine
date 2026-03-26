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

    // Update alive particles and write alive flags (fused markAlive).
    // d_aliveFlags is written here so Compact can skip its own mark pass.
    void Update(float4* posVel, float4* lifeColor, float4* sizeRot,
        int* d_aliveFlags,
        const ParticleParams& params, cudaStream_t stream = 0);

    int  Emit(float4* posVel, float4* lifeColor, float4* sizeRot,
        const ParticleParams& params, cudaStream_t stream = 0);

    // Compact expects d_aliveFlags to already be populated (by Update).
    // h_totalAlive is pinned host memory for deferred async readback.
    int  Compact(float4* posVel, float4* lifeColor, float4* sizeRot,
        int aliveCount, int maxParticles,
        int* d_aliveFlags, int* d_offsets, int* d_totalAlive,
        int* h_totalAlive,
        float4* d_tempPV, float4* d_tempLC, float4* d_tempSR,
        void* d_scanTemp, size_t scanTempBytes,
        cudaStream_t stream = 0);

    void Interleave(float4* dst, const float4* posVel,
        const float4* lifeColor, const float4* sizeRot,
        int count, cudaStream_t stream = 0);

    size_t GetScanTempBytes(int count, int* d_in, int* d_out);

} // namespace CudaParticles

#endif // GRAPE_HAS_CUDA
