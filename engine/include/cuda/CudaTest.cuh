/* Start Header *****************************************************************/
/*!
\file   CudaTest.cuh
\author Choi Meng Yew (100%)
\date   13th March 2026
\brief
CUDA diagnostic interface for verifying that CUDA is correctly
configured and operational within GrapeEngine.

This header exposes a lightweight runtime test that launches a small
CUDA kernel and validates device memory allocation, kernel execution,
and device-to-host data transfer.

The test is intended for engine developers and users to confirm that
the CUDA runtime and GPU driver are functioning properly before
enabling CUDA-accelerated engine systems such as GPU boids or
particle simulation.

Responsibilities:
    - Declare the CUDA runtime verification function
    - Provide a simple entry point for CUDA self-testing
    - Allow engine systems to check CUDA availability safely

Used by:
    - Engine startup validation
    - CUDA feature detection
    - Debugging CUDA installation issues

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#pragma once

#ifdef GRAPE_HAS_CUDA
// Returns true if CUDA is working
bool CudaTestRun();
#endif