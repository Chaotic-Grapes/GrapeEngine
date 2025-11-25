/* Start Header *****************************************************************/
/*!
\file   PerformancePanel.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th November 2025

\brief
Implements the in-editor performance monitoring panel used by the Level Editor.

Displays realtime performance metrics while the editor is in Play mode:
- FPS and frame time (from the engine Profiler)
- System-wide CPU and memory usage (percentage)
- A compact listing of profiler scopes with avg/last/max timings

Monitoring is paused when the editor is not in Play state to avoid
polling system counters while the game is not running.
*/
/* End Header *******************************************************************/

#include "PerformancePanel.h"
#include "core/Profiler.h"
#include "core/Logger.h"
#include <imgui.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void PerformancePanel::Initialize(ImFont* mainFont, ImFont* boldFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_initialized = true;

#ifdef _WIN32
    // Initialize previous times
    FILETIME idle, kernel, user;

    // Get initial system times
    if (GetSystemTimes(&idle, &kernel, &user)) {
        ULARGE_INTEGER ulIdle, ulKernel, ulUser;
        ulIdle.LowPart = idle.dwLowDateTime; ulIdle.HighPart = idle.dwHighDateTime;
        ulKernel.LowPart = kernel.dwLowDateTime; ulKernel.HighPart = kernel.dwHighDateTime;
        ulUser.LowPart = user.dwLowDateTime; ulUser.HighPart = user.dwHighDateTime;
        m_prevIdle = ulIdle.QuadPart;
        m_prevKernel = ulKernel.QuadPart;
        m_prevUser = ulUser.QuadPart;
    }
#endif
}

void PerformancePanel::Shutdown() {
    m_initialized = false;
}

#ifdef _WIN32
static unsigned long long FileTimeToULL(const FILETIME &ft) {
    ULARGE_INTEGER ul;
    ul.LowPart = ft.dwLowDateTime;
    ul.HighPart = ft.dwHighDateTime;
    return ul.QuadPart;
}

static float GetSystemMemoryUsagePercent() {
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        DWORDLONG used = mem.ullTotalPhys - mem.ullAvailPhys;
        double pct = (double)used / (double)mem.ullTotalPhys * 100.0;
        return static_cast<float>(pct);
    }
    return 0.0f;
}

static float GetCpuUsagePercent(unsigned long long &prevIdle, unsigned long long &prevKernel, unsigned long long &prevUser) {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) return 0.0f;

    unsigned long long id = FileTimeToULL(idleTime);
    unsigned long long kr = FileTimeToULL(kernelTime);
    unsigned long long ur = FileTimeToULL(userTime);

    unsigned long long sys = (kr + ur) - (prevKernel + prevUser);
    unsigned long long idle = id - prevIdle;

    float cpuPercent = 0.0f;
    if (sys > 0) cpuPercent = 100.0f * (1.0f - (double)idle / (double)sys);

    prevIdle = id;
    prevKernel = kr;
    prevUser = ur;

    if (cpuPercent < 0.0f) cpuPercent = 0.0f;
    if (cpuPercent > 100.0f) cpuPercent = 100.0f;

    return cpuPercent;
}
#endif

void PerformancePanel::Render(bool isPlaying) {
    if (!m_initialized) return;

    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Performance");

    if (!isPlaying) {
        ImGui::TextColored(ImVec4(1,1,0,1), "Monitoring paused — enter Play (Run) to collect data");
        ImGui::End();
        ImGui::PopFont();
        return;
    }

    // FPS from Profiler
    float fps = 0.0f;
    float frameMs = 0.0f;
    // Use Profiler static accessor if available
    try {
        fps = Profiler::GetFPS();
        frameMs = Profiler::GetFrameTimeMs();
    }
    catch (...) {
        // Fallback: show zeros
        fps = 0.0f;
        frameMs = 0.0f;
    }

    ImGui::Text("FPS: %.1f", fps);
    ImGui::SameLine(150);
    ImGui::Text("Frame: %.2f ms", frameMs);

#ifdef _WIN32
    float cpu = GetCpuUsagePercent(m_prevIdle, m_prevKernel, m_prevUser);
    float mem = GetSystemMemoryUsagePercent();

    ImGui::Text("CPU Usage: %.1f %%", cpu);
    ImGui::SameLine(150);
    ImGui::Text("Memory Usage: %.1f %%", mem);
#else
    ImGui::Text("CPU Usage: N/A on this platform");
    ImGui::Text("Memory Usage: N/A on this platform");
#endif

    ImGui::Separator();
    ImGui::TextWrapped("Profiler Scopes:");

    // List top-level scope info if available
    const auto &scopes = Profiler::GetAllScopeData();
    for (const auto &kv : scopes) {
        const std::string &name = kv.first;
        const auto &data = kv.second;
        ImGui::Text("%s: avg=%.2f ms, last=%.2f ms, max=%.2f ms", name.c_str(), data.AverageTimeMs, data.LastTimeMs, data.MaxTimeMs);
    }

    ImGui::End();
    ImGui::PopFont();
}
