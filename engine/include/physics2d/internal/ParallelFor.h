/* Start Header *****************************************************************/
/*!
\file   ParallelFor.h
\author Dalton Koh Shi Hao (100%)
\par d.koh@digipen.edu
\brief Deterministic static-partition parallel-for helper for physics stages.
*/
/* End Header *******************************************************************/

#ifndef ENGINE_PHYSICS2D_INTERNAL_PARALLELFOR_H
#define ENGINE_PHYSICS2D_INTERNAL_PARALLELFOR_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace Engine::Physics2D::Internal {
    /**
     * @brief Execute [0,count) with static contiguous partitions for deterministic task assignment.
     * @param count Total iteration count.
     * @param maxWorkers Upper bound for worker threads.
     * @param fn Callback invoked with (begin,end,workerIndex).
     *
     * Static partitioning avoids dynamic work-stealing nondeterminism in task ownership.
     */
    inline void ParallelForStatic(
        size_t count,
        uint32_t maxWorkers,
        const std::function<void(size_t, size_t, uint32_t)>& fn)
    {
        if (count == 0) {
            return;
        }

        const uint32_t hw = std::max<uint32_t>(1u, std::thread::hardware_concurrency());
        const uint32_t workers = std::max<uint32_t>(1u, std::min<uint32_t>(maxWorkers, hw));
        const uint32_t usedWorkers = static_cast<uint32_t>(std::min<size_t>(workers, count));
        if (usedWorkers <= 1) {
            fn(0, count, 0);
            return;
        }

        std::vector<std::thread> threads;
        threads.reserve(usedWorkers - 1);

        const size_t base = count / usedWorkers;
        const size_t rem = count % usedWorkers;
        size_t begin = 0;
        for (uint32_t w = 0; w < usedWorkers; ++w) {
            const size_t span = base + (w < rem ? 1 : 0);
            const size_t end = begin + span;
            if (w == 0) {
                fn(begin, end, w);
            } else {
                threads.emplace_back([=]() {
                    fn(begin, end, w);
                    });
            }
            begin = end;
        }

        for (auto& t : threads) {
            t.join();
        }
    }
}

#endif
