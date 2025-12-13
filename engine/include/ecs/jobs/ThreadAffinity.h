/* Start Header *****************************************************************/
/*!
\file    ThreadAffinity.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains utilities for setting thread affinity to improve cache
locality and reduce context switching overhead on multi-core systems.

Thread affinity binds a thread to specific CPU cores, improving cache
efficiency and reducing data migration between cores.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef THREAD_AFFINITY_H
#define THREAD_AFFINITY_H

#include <thread>
#include <cstdint>

namespace ECS::Jobs {

    /**
     * @brief Utilities for managing thread affinity.
     * 
     * Thread affinity binds a thread to specific CPU cores, which can improve
     * performance by:
     * - Keeping thread data in the core's cache
     * - Reducing context switching overhead
     * - Improving NUMA locality
     * 
     * Implementation is platform-specific (Windows, Linux, macOS).
     */
    class ThreadAffinity {
    public:
        /**
         * @brief Set the affinity of the current thread to a specific core.
         * 
         * Binds the current thread to execute only on the specified core.
         * 
         * @param coreIndex The core to bind to (0-based)
         * @return true if successful, false if not supported or invalid core
         */
        static bool SetCurrentThreadAffinity(uint32_t coreIndex);

        /**
         * @brief Set the affinity of a given thread to a specific core.
         * 
         * @param thread The thread to bind
         * @param coreIndex The core to bind to (0-based)
         * @return true if successful, false if not supported or invalid core
         */
        static bool SetThreadAffinity(std::thread::native_handle_type handle, 
                                     uint32_t coreIndex);

        /**
         * @brief Get the number of available CPU cores.
         * 
         * @return Number of logical cores available
         */
        static uint32_t GetNumCores();

        /**
         * @brief Check if thread affinity is supported on this platform.
         * 
         * @return true if SetThreadAffinity calls will work
         */
        static bool IsSupported();
    };

}

#endif
