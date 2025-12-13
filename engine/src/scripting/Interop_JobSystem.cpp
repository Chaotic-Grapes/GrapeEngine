/* Start Header *****************************************************************/
/*!
\file    Interop_JobSystem.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
Implementation of job system interop functions for C# scripting systems.

Exposes the C++ job system (scheduling, profiling, thread affinity) to C#
through P/Invoke declarations.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef BUILDING_INTEROP
#define BUILDING_INTEROP
#endif

#include "Export.h"
#include "ecs/jobs/JobManager.h"
#include "ecs/jobs/JobProfiler.h"
#include "ecs/jobs/ThreadAffinity.h"
#include <cstring>
#include <sstream>

namespace {
    // Helper to safely convert pointers
    inline ECS::Jobs::JobManager* ToJobManager(void* ptr) {
        return static_cast<ECS::Jobs::JobManager*>(ptr);
    }

    inline ECS::Jobs::JobHandle* ToJobHandle(void* ptr) {
        return static_cast<ECS::Jobs::JobHandle*>(ptr);
    }

    inline ECS::Jobs::JobProfiler* ToJobProfiler(void* ptr) {
        return reinterpret_cast<ECS::Jobs::JobProfiler*>(ptr);
    }

    // Helper to allocate and copy C++ string to C memory
    inline char* AllocateString(const std::string& str) {
        size_t len = str.length() + 1;
        char* result = new char[len];
        strcpy_s(result, len, str.c_str());

        return result;
    }
}

// ============================================================================
// Job Handle Operations
// ============================================================================

INTEROP_API bool JobInterop_HandleIsComplete(void* handlePtr) {
    if (!handlePtr) return true;
    
    auto* handle = ToJobHandle(handlePtr);
    return handle->IsComplete();
}

INTEROP_API void JobInterop_HandleComplete(void* handlePtr) {
    if (!handlePtr) return;
    
    auto* handle = ToJobHandle(handlePtr);
    handle->Complete();
}

INTEROP_API bool JobInterop_HandleTryComplete(void* handlePtr, int timeoutMs) {
    if (!handlePtr) return true;
    
    auto* handle = ToJobHandle(handlePtr);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    while (!handle->IsComplete()) {
        auto elapsed = std::chrono::high_resolution_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeoutMs) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

// ========================================================================
// Job Manager Operations
// ========================================================================

INTEROP_API void* JobInterop_ManagerSchedule(void* jobManagerPtr, void* jobPtr, void* dependsOnPtr, int priority) {
    if (!jobManagerPtr) return nullptr;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    
    // For C# jobs, we would need a wrapper to call C# code from C++
    // For now, return invalid handle as jobs need to be implemented
    return nullptr;
}

INTEROP_API void* JobInterop_ManagerScheduleParallel(void* jobManagerPtr, void* jobsPtr, uint32_t jobCount, int priority) {
    if (!jobManagerPtr) return nullptr;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    
    // Similar to ScheduleJob - would need C# job wrapper
    return nullptr;
}

INTEROP_API void JobInterop_ManagerCompleteAllJobs(void* jobManagerPtr) {
    if (!jobManagerPtr) return;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    jobManager->CompleteAllJobs();
}

INTEROP_API bool JobInterop_ManagerIsRunning(void* jobManagerPtr) {
    if (!jobManagerPtr) return false;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    return jobManager->IsRunning();
}

INTEROP_API uint32_t JobInterop_ManagerGetNumWorkerThreads(void* jobManagerPtr) {
    if (!jobManagerPtr) return 0;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    return jobManager->GetNumWorkerThreads();
}

INTEROP_API size_t JobInterop_ManagerGetPendingJobCount(void* jobManagerPtr) {
    if (!jobManagerPtr) return 0;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    return jobManager->GetPendingJobCount();
}

INTEROP_API bool JobInterop_ManagerIsProfilingEnabled(void* jobManagerPtr) {
    if (!jobManagerPtr) return false;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    return jobManager->IsProfilingEnabled();
}

INTEROP_API void JobInterop_ManagerSetProfilingEnabled(void* jobManagerPtr, bool enabled) {
    if (!jobManagerPtr) return;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    jobManager->SetProfilingEnabled(enabled);
}

INTEROP_API void JobInterop_ManagerResetProfiler(void* jobManagerPtr) {
    if (!jobManagerPtr) return;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    jobManager->GetProfiler().Reset();
}

INTEROP_API char* JobInterop_ManagerGetProfilingReport(void* jobManagerPtr) {
    if (!jobManagerPtr) return nullptr;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    auto report = jobManager->GetProfilingReport();
    return AllocateString(report);
}

// ========================================================================
// Job Profiler Operations
// ========================================================================

INTEROP_API bool JobInterop_ProfilerIsEnabled(void* jobManagerPtr) {
    if (!jobManagerPtr) return false;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    return jobManager->GetProfiler().IsEnabled();
}

// Helper structure for interop
struct InteropJobTypeStats {
    const char* JobNamePtr;
    uint64_t ExecutionCount;
    int64_t TotalExecutionTimeMicros;
    int64_t MinExecutionTimeMicros;
    int64_t MaxExecutionTimeMicros;
    int64_t AvgExecutionTimeMicros;
    uint64_t StolenCount;
    uint64_t EntitiesProcessedTotal;
};

INTEROP_API InteropJobTypeStats JobInterop_ProfilerGetStats(void* jobManagerPtr, const char* jobName) {
    InteropJobTypeStats result{};
    if (!jobManagerPtr || !jobName) return result;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    auto stats = jobManager->GetProfiler().GetStats(jobName);
    
    if (!stats.JobName.empty()) {
        result.JobNamePtr = AllocateString(stats.JobName);
        result.ExecutionCount = stats.ExecutionCount;
        result.TotalExecutionTimeMicros = stats.TotalExecutionTime.count();
        result.MinExecutionTimeMicros = stats.MinExecutionTime.count();
        result.MaxExecutionTimeMicros = stats.MaxExecutionTime.count();
        result.AvgExecutionTimeMicros = stats.AvgExecutionTime.count();
        result.StolenCount = stats.StolenCount;
        result.EntitiesProcessedTotal = stats.EntitiesProcessedTotal;
    }
    
    return result;
}

INTEROP_API InteropJobTypeStats* JobInterop_ProfilerGetAllStats(void* jobManagerPtr, uint32_t* outCount) {
    if (!jobManagerPtr || !outCount) return nullptr;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    auto allStats = jobManager->GetProfiler().GetAllStats();
    
    *outCount = static_cast<uint32_t>(allStats.size());
    if (allStats.empty()) return nullptr;
    
    auto* result = new InteropJobTypeStats[allStats.size()];
    for (size_t i = 0; i < allStats.size(); ++i) {
        const auto& stats = allStats[i];
        result[i].JobNamePtr = AllocateString(stats.JobName);
        result[i].ExecutionCount = stats.ExecutionCount;
        result[i].TotalExecutionTimeMicros = stats.TotalExecutionTime.count();
        result[i].MinExecutionTimeMicros = stats.MinExecutionTime.count();
        result[i].MaxExecutionTimeMicros = stats.MaxExecutionTime.count();
        result[i].AvgExecutionTimeMicros = stats.AvgExecutionTime.count();
        result[i].StolenCount = stats.StolenCount;
        result[i].EntitiesProcessedTotal = stats.EntitiesProcessedTotal;
    }
    
    return result;
}

struct InteropJobMetrics {
    const char* JobNamePtr;
    int64_t ExecutionTimeMicros;
    int64_t WaitTimeMicros;
    uint32_t ExecutingThreadId;
    uint64_t EntitiesProcessed;
    bool WasStolen;
};

INTEROP_API InteropJobMetrics* JobInterop_ProfilerGetAllMetrics(void* jobManagerPtr, uint32_t* outCount) {
    if (!jobManagerPtr || !outCount) return nullptr;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    auto allMetrics = jobManager->GetProfiler().GetAllMetrics();
    
    *outCount = static_cast<uint32_t>(allMetrics.size());
    if (allMetrics.empty()) return nullptr;
    
    auto* result = new InteropJobMetrics[allMetrics.size()];
    for (size_t i = 0; i < allMetrics.size(); ++i) {
        const auto& metrics = allMetrics[i];
        result[i].JobNamePtr = AllocateString(metrics.JobName);
        result[i].ExecutionTimeMicros = metrics.ExecutionTime.count();
        result[i].WaitTimeMicros = metrics.WaitTime.count();
        result[i].ExecutingThreadId = metrics.ExecutingThreadId;
        result[i].EntitiesProcessed = metrics.EntitiesProcessed;
        result[i].WasStolen = metrics.WasStolen;
    }
    
    return result;
}

INTEROP_API void JobInterop_ProfilerReset(void* jobManagerPtr) {
    if (!jobManagerPtr) return;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    jobManager->GetProfiler().Reset();
}

INTEROP_API void JobInterop_ProfilerMarkFrameStart(void* jobManagerPtr) {
    if (!jobManagerPtr) return;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    jobManager->GetProfiler().MarkFrameStart();
}

INTEROP_API void JobInterop_ProfilerMarkFrameEnd(void* jobManagerPtr) {
    if (!jobManagerPtr) return;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    jobManager->GetProfiler().MarkFrameEnd();
}

INTEROP_API char* JobInterop_ProfilerGenerateReport(void* jobManagerPtr) {
    if (!jobManagerPtr) return nullptr;
    
    auto* jobManager = ToJobManager(jobManagerPtr);
    auto report = jobManager->GetProfiler().GenerateReport();
    return AllocateString(report);
}

// ========================================================================
// Memory Management
// ========================================================================

INTEROP_API void JobInterop_FreeString(char* ptr) {
    if (ptr) {
        delete[] ptr;
    }
}

INTEROP_API void JobInterop_FreeJobStatsArray(InteropJobTypeStats* ptr, uint32_t count) {
    if (!ptr) return;
    
    for (uint32_t i = 0; i < count; ++i) {
        if (ptr[i].JobNamePtr) {
            delete[] ptr[i].JobNamePtr;
        }
    }
    delete[] ptr;
}

INTEROP_API void JobInterop_FreeJobMetricsArray(InteropJobMetrics* ptr, uint32_t count) {
    if (!ptr) return;
    
    for (uint32_t i = 0; i < count; ++i) {
        if (ptr[i].JobNamePtr) {
            delete[] ptr[i].JobNamePtr;
        }
    }
    delete[] ptr;
}

// ========================================================================
// Managed Job Scheduling
// ========================================================================

// Wrapper job that executes a C# managed job
class ManagedJobWrapper : public ECS::Jobs::IJob {
public:
    ManagedJobWrapper(intptr_t managedHandle, const std::string& jobName)
        : m_managedHandle(managedHandle), m_jobName(jobName) {}

    void Execute() override {
        // Call the managed job's Execute method through the marshalling callback
        // Note: The actual implementation requires .NET interop callback setup
        // For now, this is a placeholder that will be completed when the
        // proper callback mechanism is established
        
        // The managed code provides a static callback function that will
        // look up the GCHandle and invoke the C# IJob.Execute() method
        if (s_executeCallback) {
            s_executeCallback(m_managedHandle);
        }
    }

    const std::string& GetName() const override {
        return m_jobName;
    }

    static void SetExecuteCallback(void (*callback)(intptr_t)) {
        s_executeCallback = callback;
    }

private:
    intptr_t m_managedHandle;
    std::string m_jobName;
    static inline void (*s_executeCallback)(intptr_t) = nullptr;
};

INTEROP_API intptr_t JobInterop_ScheduleManagedJob(
    void* jobManagerPtr,
    intptr_t managedJobHandle,
    char* jobNamePtr,
    void* dependsOnPtr,
    int32_t priority)
{
    if (!jobManagerPtr || !jobNamePtr) {
        return 0;
    }

    auto* jobManager = ToJobManager(jobManagerPtr);
    std::string jobName(jobNamePtr);

    try {
        auto wrapper = std::make_unique<ManagedJobWrapper>(managedJobHandle, jobName);
        
        // Create a handle for the result
        ECS::Jobs::JobHandle resultHandle;
        if (dependsOnPtr) {
            auto* depHandle = static_cast<ECS::Jobs::JobHandle*>(dependsOnPtr);
            // Schedule with dependency
            // Note: Actual implementation depends on JobManager::Schedule signature
            resultHandle = jobManager->Schedule(std::move(wrapper), *depHandle);
        }
        else {
            // Schedule without dependency
            resultHandle = jobManager->Schedule(std::move(wrapper));
        }

        // Return handle as intptr_t (will be wrapped back in C# JobHandle)
        return reinterpret_cast<intptr_t>(new ECS::Jobs::JobHandle(resultHandle));
    }
    catch (...) {
        return 0;
    }
}

INTEROP_API intptr_t JobInterop_ScheduleManagedJobBatch(
    void* jobManagerPtr,
    intptr_t* managedJobHandles,
    char** jobNames,
    uint32_t jobCount,
    int32_t priority)
{
    if (!jobManagerPtr || !managedJobHandles || !jobNames || jobCount == 0) {
        return 0;
    }

    auto* jobManager = ToJobManager(jobManagerPtr);

    try {
        std::vector<std::unique_ptr<ECS::Jobs::IJob>> jobs;
        jobs.reserve(jobCount);

        for (uint32_t i = 0; i < jobCount; ++i) {
            if (jobNames[i]) {
                auto wrapper = std::make_unique<ManagedJobWrapper>(
                    managedJobHandles[i],
                    std::string(jobNames[i])
                );
                jobs.push_back(std::move(wrapper));
            }
        }

        if (jobs.empty()) {
            return 0;
        }

        // Schedule batch - implementation depends on JobManager parallel scheduling
        // This is a placeholder that needs to be adapted to the actual API
        ECS::Jobs::JobHandle resultHandle;
        
        // For now, schedule sequentially (not optimal but safe)
        // A better implementation would use jobManager->ScheduleParallel()
        for (auto& job : jobs) {
            resultHandle = jobManager->Schedule(std::move(job));
        }

        return reinterpret_cast<intptr_t>(new ECS::Jobs::JobHandle(resultHandle));
    }
    catch (...) {
        return 0;
    }
}

// ========================================================================
// Thread Affinity
// ========================================================================

INTEROP_API bool ThreadAffinityInterop_IsSupported() {
    return ECS::Jobs::ThreadAffinity::IsSupported();
}

INTEROP_API uint32_t ThreadAffinityInterop_GetNumCores() {
    return ECS::Jobs::ThreadAffinity::GetNumCores();
}

INTEROP_API bool ThreadAffinityInterop_SetCurrentThreadAffinity(uint32_t coreIndex) {
    return ECS::Jobs::ThreadAffinity::SetCurrentThreadAffinity(coreIndex);
}
