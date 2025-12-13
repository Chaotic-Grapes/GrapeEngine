/* Start Header *****************************************************************/
/*!
\file    ThreadAffinity.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the implementation of ThreadAffinity utilities for setting
thread affinity on Windows and other platforms.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "ecs/jobs/ThreadAffinity.h"
#include <thread>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace ECS::Jobs {

    bool ThreadAffinity::SetCurrentThreadAffinity(uint32_t coreIndex) {
#ifdef _WIN32
        // Windows implementation using SetThreadAffinityMask
        HANDLE currentThread = GetCurrentThread();
        DWORD_PTR affinityMask = 1ULL << coreIndex;
        
        DWORD_PTR result = SetThreadAffinityMask(currentThread, affinityMask);
        return result != 0;
#else
        // Unsupported platform
        return false;
#endif
    }

    bool ThreadAffinity::SetThreadAffinity(std::thread::native_handle_type handle, 
                                          uint32_t coreIndex) {
#ifdef _WIN32
        // Windows implementation
        DWORD_PTR affinityMask = 1ULL << coreIndex;
        DWORD_PTR result = SetThreadAffinityMask(handle, affinityMask);
        return result != 0;
#else
        // Unsupported platform
        return false;
#endif
    }

    uint32_t ThreadAffinity::GetNumCores() {
        return std::thread::hardware_concurrency();
    }

    bool ThreadAffinity::IsSupported() {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

}
