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

    // Spatial hashing
    float    cellSize;       // should equal visualRange
    int      hashTableSize;  // prime number, e.g. 100003 for 100K boids

    uint32_t* d_cellIds;    // [count] cell id per boid
    uint32_t* d_boidIds;    // [count] boid indices sorted by cell  
    uint32_t* d_cellStart;  // [hashTableSize] where each cell starts in sorted array
};

namespace CudaBoids {

    // After the kernel, the caller swaps bufferIndex (no copy needed).
    // Both buffers are GL VBOs accessed via CUDA interop.
    void Launch(float4* d_posVel,
        const float4* d_posVelPrev,
        const BoidParams& params,
        cudaStream_t stream = 0);

    // Initialize boid positions/velocities randomly within bounds.
    void InitRandom(float4* d_posVel,
        int count,
        float minX, float minY,
        float maxX, float maxY,
        float maxSpeed,
        uint32_t entitySeed = 0,
        const uint8_t* collisionMasks = nullptr,
        int collisionWidth = 0,
        int collisionHeight = 0,
        int collisionOriginX = 0,
        int collisionOriginY = 0,
        float tileSize = 0.0f);

} // namespace CudaBoids

#endif // GRAPE_HAS_CUDA
