#pragma once

#ifdef GRAPE_HAS_CUDA

#include <cuda_runtime.h>

// Parameters passed to the boid kernel each frame
struct BoidParams {
    float separationWeight;
    float alignmentWeight;
    float cohesionWeight;
    float visualRange;
    float maxSpeed;
    float maxForce;
    float dt;
    int count;

    // World bounds (boids wrap around these)
    float boundsMinX;
    float boundsMinY;
    float boundsMaxX;
    float boundsMaxY;

    // Tile avoidance params
    const uint8_t* collisionMasks;
    int32_t collisionWidth;
    int32_t collisionHeight;
    int32_t collisionOriginX;
    int32_t collisionOriginY;
    float   tileSize;
    float   collisionAvoidWeight;
    float   collisionAvoidRadius;
    unsigned int frameCount;  // set from TimeSystem::Instance().GetFrameCount()
};

namespace CudaBoids {

    // Launch the boids simulation kernel.
    //
    // d_posVel:     device pointer to float4 array (mapped from GL VBO)
    //               xy = position, zw = velocity
    // d_posVelPrev: device pointer to float4 array (previous frame, CUDA-only)
    //               The kernel reads from prev and writes to posVel.
    // params:       simulation parameters for this frame
    //
    // After the kernel, the caller should swap the buffers or copy posVel -> posVelPrev.
    void Launch(float4* d_posVel,
                const float4* d_posVelPrev,
                const BoidParams& params);

    // Initialize boid positions/velocities randomly within bounds.
    void InitRandom(float4* d_posVel,
                    int count,
                    float minX, float minY,
                    float maxX, float maxY,
                    float maxSpeed);

} // namespace CudaBoids

#endif // GRAPE_HAS_CUDA
