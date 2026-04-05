/* Start Header *****************************************************************/
/*!
\file   CudaBoids.cuh
\author Choi Meng Yew (100%)
\date   3rd March 2026
\brief
Declares the CUDA interface used for GPU-accelerated boid simulation
within GrapeEngine.

This module defines the parameter structure and host-side launch
functions required to execute the boid simulation kernels on the GPU.
The simulation updates boid position and velocity data stored in
OpenGL vertex buffers via CUDA-OpenGL interoperability.

Each CUDA thread simulates a single boid using classic flocking
behaviors:

    - Separation
    - Alignment
    - Cohesion

Additional behaviors supported by the simulation include:

    - Spatial hashing for efficient neighbor lookup
    - Tile-based collision avoidance
    - World boundary wrapping
    - Randomized initialization of flock states

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

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

    /**
     * @brief Launch the boid steering kernel for one simulation step.
     *        After the kernel, the caller swaps bufferIndex; no copy is needed.
     *        Both buffers are GL VBOs accessed via CUDA-OpenGL interop.
     * @param d_posVel Device buffer to write updated boid positions and velocities.
     * @param d_posVelPrev Device buffer containing boid positions and velocities from the previous frame.
     * @param params Boid simulation parameters for this frame.
     * @param stream CUDA stream to use for asynchronous execution.
     */
    void Launch(float4* d_posVel,
        const float4* d_posVelPrev,
        const BoidParams& params,
        cudaStream_t stream = 0);

    /**
     * @brief Initialize boid positions and velocities randomly within world bounds.
     * @param d_posVel Device buffer to write the initial position (xy) and velocity (zw) for each boid.
     * @param count Number of boids to initialize.
     * @param minX Minimum x bound for initial position scatter.
     * @param minY Minimum y bound for initial position scatter.
     * @param maxX Maximum x bound for initial position scatter.
     * @param maxY Maximum y bound for initial position scatter.
     * @param maxSpeed Maximum initial speed magnitude.
     * @param entitySeed Per-entity seed value for reproducible initialization.
     * @param collisionMasks Device pointer to tile collision mask data (optional).
     * @param collisionWidth Width of the collision grid in tiles.
     * @param collisionHeight Height of the collision grid in tiles.
     * @param collisionOriginX X origin of the collision grid in world units.
     * @param collisionOriginY Y origin of the collision grid in world units.
     * @param tileSize Size of each tile in world units.
     */
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
