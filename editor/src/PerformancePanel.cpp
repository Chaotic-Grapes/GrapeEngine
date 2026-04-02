/* Start Header *****************************************************************/
/*!
\file   PerformancePanel.cpp
\author Muhammad Nur Fadzly Bin Zulkifli (90%)
        Foo Rui Qin (10%)
\par    muhammadnurfadzly.b@digipen.edu
        ruiqin.foo@digipen.edu
\date   12th March 2026

\brief
Implements the in-editor performance monitoring panel used by the Level Editor.

Displays realtime performance metrics while the editor is in Play mode:
- FPS and frame time (from the engine Profiler)
- System usage breakdown per registered profiling scope

Monitoring is paused when the editor is not in Play state to avoid
polling system counters while the game is not running.
*/
/* End Header *******************************************************************/

#include "PerformancePanel.h"
#include "EditorIcons.h"
#include "EditorStyle.h"
#include "services/TimeSystem.h"
#include "services/EditorService.h"
#include "ecs/SystemManager.h"
#include "services/MemoryManager.h"
#include "ecs/World.h"
#include "core/Application.h"
#include "platform/IPlatformContext.h"
#include "platform/IWindow.h"
#include "core/Logger.h"
#include <imgui.h>
#include <algorithm>
#include <deque>

// Initializes panel font handles and marks panel state ready for rendering
void PerformancePanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    // Cache font handles once so all sub-sections render with consistent typography
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;

    // Mark panel ready so Render can start collecting and drawing metrics
    m_initialized = true;
}

// Resets runtime flags so panel stops collecting and rendering cached performance data
void PerformancePanel::Shutdown() {
    // Prevent further rendering and data refresh calls until reinitialized
    m_initialized = false;

    // Drop collected-data marker so next startup begins from clean state
    m_hasCollectedData = false;
}

// Clears per-scene collected state when world content changes
void PerformancePanel::ResetForNewScene() {
    // Keep caches but force state to be treated as not yet refreshed for new scene context
    m_hasCollectedData = false;
}

// Stores system manager pointer used for scripted and enabled system status lookups
void PerformancePanel::SetSystemManager(ECS::SystemManager* systemManager) {
    // Store non-owning pointer managed by editor lifecycle
    m_systemManager = systemManager;
}

// Stores world pointer used to compute entity, component, and chunk-related metrics
void PerformancePanel::SetWorld(ECS::World* world) {
    // Store non-owning pointer managed by scene or editor service
    m_world = world;
}

// Renders the full performance panel and updates cached metrics each frame
void PerformancePanel::Render(bool /*isPlaying*/) {
    if (!m_initialized) return;

    if (!ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }


    // Update cached data each frame (continuous monitoring)
    _updateCachedData();

    // Render header with FPS and frame time
    _renderHeader();

    ImGui::Separator();

    // Render overview statistics
    _renderOverviewStats();

    ImGui::Separator();

    // Render system usage table
    _renderSystemsTable();

    // Render live memory pool stats (proves MM is active with game objects)
    _renderMemoryStats();

    ImGui::Separator();

    // Render memory benchmark
    _renderMemoryBenchmark();
    ImGui::End();
}

// -------------------------------------------------------------------------
// Private Rendering Methods
// -------------------------------------------------------------------------

// Draws the panel title row
void PerformancePanel::_renderHeader() {
    // Use bold face to visually anchor the panel title
    ImGui::PushFont(m_boldFont);
    ImGui::Text("Performance");
    ImGui::PopFont();
}

// Draws compact overview metrics including frame timings and world counts
void PerformancePanel::_renderOverviewStats() {
    // Section title in bold to separate overview from detailed tables
    ImGui::PushFont(m_boldFont);
    ImGui::Text("Overview");
    ImGui::PopFont();

    // Display overview stats in columns
    ImGui::Columns(8, "OverviewColumns", false);

    ImGui::Text("FPS:");
    ImGui::NextColumn();
    ImGui::Text("%.1f", m_cachedFps);
    ImGui::NextColumn();

    ImGui::Text("Frame:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_cachedFrameMs);
    ImGui::NextColumn();

    ImGui::Text("Min:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_cachedMinFrameMs);
    ImGui::NextColumn();

    ImGui::Text("Max:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_cachedMaxFrameMs);
    ImGui::NextColumn();

    ImGui::Separator();

    ImGui::Text("Entities:");
    ImGui::NextColumn();
    ImGui::Text("%u", m_cachedEntityCount);
    ImGui::NextColumn();

    ImGui::Text("Components:");
    ImGui::NextColumn();
    ImGui::Text("%u", m_cachedComponentCount);
    ImGui::NextColumn();

    // Pull optional GPU timings from editor service and window VSync status.
    float sceneGpuMs = 0.0f;
    float imguiGpuMs = 0.0f;
    bool hasGpuTiming = false;
    if (auto* editorService = Services::EditorService::Get()) {
        hasGpuTiming = editorService->HasGpuTiming();
        sceneGpuMs = editorService->GetSceneRenderGpuMs();
        imguiGpuMs = editorService->GetImGuiRenderGpuMs();
    }

    Platform::IWindow* mainWindow = nullptr;
    if (Engine::CORE) {
        if (auto* platformContext = Engine::CORE->GetPlatformContext()) {
            mainWindow = platformContext->GetMainWindow();
        }
    }

    ImGui::Text("Scene GPU:");
    ImGui::NextColumn();
    if (hasGpuTiming) ImGui::Text("%.2f ms", sceneGpuMs);
    else ImGui::TextDisabled("N/A");
    ImGui::NextColumn();

    ImGui::Text("ImGui GPU:");
    ImGui::NextColumn();
    if (hasGpuTiming) ImGui::Text("%.2f ms", imguiGpuMs);
    else ImGui::TextDisabled("N/A");
    ImGui::NextColumn();

    ImGui::Text("VSync:");
    ImGui::NextColumn();
    if (mainWindow) {
        bool vsyncEnabled = mainWindow->IsVSync();
        if (ImGui::Checkbox("##PerfVSync", &vsyncEnabled)) {
            mainWindow->SetVSync(vsyncEnabled);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(vsyncEnabled ? "On" : "Off");
    }
    else {
        ImGui::TextDisabled("N/A");
    }
    ImGui::NextColumn();

    ImGui::Text("Scopes Last:");
    ImGui::NextColumn();
    ImGui::Text("%.2f ms", m_cachedTotalTime);
    ImGui::NextColumn();

    ImGui::Columns(1);
}

/**
 * @brief Draw ECS-system and engine/frame scope timing tables separately.
 * @return void
 * @note Percentages are normalized against the current frame baseline so
 *       system and engine rows can be compared directly.
 */
void PerformancePanel::_renderSystemsTable() {
    ImGui::PushFont(m_boldFont);
    ImGui::Text("Profiling");
    ImGui::PopFont();

    // Use frame time as the baseline and compare against same-frame scope values.
    // This avoids mixing moving averages with last-frame totals, which can create
    // synthetic spikes in unattributed time during transient hitches.
    const double frameTimeMs = static_cast<double>(m_cachedFrameMs);
    const double totalScopeLastMs = std::max(0.0, m_cachedTotalTime);
    const double totalTime = std::max(frameTimeMs, totalScopeLastMs);

    // Sum last-sample values so unattributed time stays frame-consistent.
    double sumScopeLastMs = 0.0;
    double sumSystemLastMs = 0.0;
    double sumEngineLastMs = 0.0;
    for (const auto& kv : m_cachedScopes) {
        const double lastMs = static_cast<double>(kv.second.LastTimeMs);
        sumScopeLastMs += lastMs;

        if (kv.second.isKnownSystem) {
            sumSystemLastMs += lastMs;
        }
        else {
            sumEngineLastMs += lastMs;
        }
    }

    double unaccountedMs = totalTime - sumScopeLastMs;
    if (unaccountedMs < 0.0)
        unaccountedMs = 0.0; // Avoid negative due to rounding

    ImGui::Text("Systems");
    if (ImGui::BeginTable("SystemUsageTable", 4,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableHeadersRow();

        // Render only known ECS systems.
        for (const auto& kv : m_cachedScopes) {
            const std::string& name = kv.first;
            const auto& data = kv.second;

            if (!data.isKnownSystem) {
                continue;
            }

            // Skip disabled systems to reduce noise in active-frame diagnosis.
            if (!data.isEnabled) {
                continue;
            }

            // Calculate usage percentage and clamp to [0, 100]
            float usagePercent = 0.0f;
            if (totalTime > 0.001) {
                double percent = (static_cast<double>(data.LastTimeMs) / totalTime) * 100.0;
                if (percent < 0.0) percent = 0.0;
                if (percent > 100.0) percent = 100.0;
                usagePercent = static_cast<float>(percent);
            }

            // Determine bar color based on usage percentage
            ImVec4 barColor;
            if (usagePercent >= 80.0f) {
                barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f); // Red
            }
            else if (usagePercent >= 60.0f) {
                barColor = ImVec4(0.9f, 0.6f, 0.2f, 1.0f); // Orange
            }
            else if (usagePercent >= 40.0f) {
                barColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f); // Yellow
            }
            else {
                barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // Green
            }

            ImGui::TableNextRow();

            // Column 0: System name with source and status indicators.
            ImGui::TableSetColumnIndex(0);

            // Get system info from cached data
            bool isScripted = data.isScripted;
            bool isEnabled = data.isEnabled;
            // Display system source indicator.
            if (isScripted) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // Green for C#
                ImGui::Text("[C#]");
                ImGui::PopStyleColor();
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.6f, 0.8f, 1.0f)); // Blue for Native
                ImGui::Text("[Native]");
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();

            if (isEnabled) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.2f, 1.0f)); // Green for Enabled
                ImGui::Text("[Enabled]");
                ImGui::PopStyleColor();
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Gray for Disabled
                ImGui::Text("[Disabled]");
                ImGui::PopStyleColor();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", name.c_str());

            // Column 1: Usage % with color bar
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(usagePercent / 100.0f, ImVec2(-1.0f, 0.0f), "");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("%.1f%%", usagePercent);

            // Column 2: Last-sample time
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", data.LastTimeMs);

            // Column 3: Max time
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", data.MaxTimeMs);
        }

        ImGui::EndTable();
    }

    ImGui::Text("Engine / Frame");
    if (ImGui::BeginTable("EngineUsageTable", 4,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableHeadersRow();

        for (const auto& kv : m_cachedScopes) {
            const std::string& name = kv.first;
            const auto& data = kv.second;

            if (data.isKnownSystem) {
                continue;
            }

            float usagePercent = 0.0f;
            if (totalTime > 0.001) {
                double percent = (static_cast<double>(data.LastTimeMs) / totalTime) * 100.0;
                if (percent < 0.0) percent = 0.0;
                if (percent > 100.0) percent = 100.0;
                usagePercent = static_cast<float>(percent);
            }

            ImVec4 barColor = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
            if (usagePercent >= 60.0f) {
                barColor = ImVec4(0.9f, 0.45f, 0.2f, 1.0f);
            }
            if (usagePercent >= 80.0f) {
                barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
            ImGui::Text("[Engine]");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("%s", name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
            ImGui::ProgressBar(usagePercent / 100.0f, ImVec2(-1.0f, 0.0f), "");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("%.1f%%", usagePercent);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", data.LastTimeMs);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", data.MaxTimeMs);
        }

        if (unaccountedMs > 0.001) {
            double unaccountedPercent = (unaccountedMs / totalTime) * 100.0;
            if (unaccountedPercent < 0.0) unaccountedPercent = 0.0;
            if (unaccountedPercent > 100.0) unaccountedPercent = 100.0;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
            ImGui::Text("[Engine] Unattributed");
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
            ImGui::ProgressBar(static_cast<float>(unaccountedPercent / 100.0), ImVec2(-1.0f, 0.0f), "");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("%.1f%%", static_cast<float>(unaccountedPercent));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", unaccountedMs);

            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("-");
        }

        ImGui::EndTable();
    }

    ImGui::Text("Frame split: Systems %.2f ms | Engine scopes %.2f ms | Unattributed %.2f ms",
        static_cast<float>(sumSystemLastMs),
        static_cast<float>(sumEngineLastMs),
        static_cast<float>(unaccountedMs));
}

// Draws live memory-pool and ECS chunk metrics with quick health indicators
void PerformancePanel::_renderMemoryStats() {
    ImGui::Separator();

    ImGui::PushFont(m_boldFont);
    ImGui::Text("Memory Pool");
    ImGui::PopFont();

    // Pull live stats directly from the MemoryManager singleton
    // These reflect the actual state of the pool serving all game objects/entities
    size_t currentUsage = MemoryManager::GetInstance().GetCurrentUsage();
    size_t totalPoolSize = MemoryManager::GetInstance().GetTotalPoolSize();
    size_t totalAllocs = MemoryManager::GetInstance().GetTotalAllocated();
    size_t totalFreed = MemoryManager::GetInstance().GetTotalFreed();
    int freeBlocks = MemoryManager::GetInstance().GetFreeBlockCount();
    int pageCount = MemoryManager::GetInstance().GetPageCount();

    // ECS chunk memory proof (current level data stored in chunks)
    size_t ecsChunkBytes = 0;
    uint32_t ecsChunkCount = 0;

	// Sum up all archetype chunk bytes
    if (m_world) {

		// Get all archetypes in the world
        const auto archetypes = m_world->GetAllArchetypes();

		// Sum up chunk counts and bytes
        for (const auto* archetype : archetypes) {

			// Sanity check
            if (!archetype) continue;

			// Get chunk count and byte size
            const uint32_t chunks = archetype->GetChunkCount();

			// Accumulate
            ecsChunkCount += chunks;
            ecsChunkBytes += static_cast<size_t>(chunks) * archetype->GetChunkByteSize();
        }
    }

	// Determine if Memory Manager covers ECS chunk memory
    const bool hasWorld = (m_world != nullptr);

	// MM covers ECS if current usage >= ecsChunkBytes
    const bool mmCoversEcs = hasWorld && (currentUsage >= ecsChunkBytes);

    // Usage bar: currentUsage / totalPoolSize
	// Handle zero totalPoolSize case
    float usageRatio = (totalPoolSize > 0)
        ? static_cast<float>(currentUsage) / static_cast<float>(totalPoolSize)
        : 0.0f;

    // Color the bar based on how full the pool is
    ImVec4 barColor;
    if (usageRatio >= 0.8f) barColor = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);       // Red
    else if (usageRatio >= 0.6f) barColor = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);  // Orange
    else if (usageRatio >= 0.4f) barColor = ImVec4(0.9f, 0.9f, 0.2f, 1.0f);  // Yellow
    else barColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);                          // Green

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);

	// Usage label: "X KB / Y KB"
    char barLabel[64];

	// Divide by 1024.0f to convert to KB
    snprintf(barLabel, sizeof(barLabel), "%.1f KB / %.1f KB", currentUsage / 1024.0f,
        totalPoolSize / 1024.0f);

    // Cap the bar to half the panel width
    // ProgressBar still uses raw usageRatio so the fill length stays frame-accurate
    float halfWidth = ImGui::GetContentRegionAvail().x * 0.5f;
    ImGui::ProgressBar(usageRatio, ImVec2(halfWidth, 0.0f), barLabel);
    ImGui::PopStyleColor();

    // Match Overview's column layout
    ImGui::Columns(8, "MemPoolColumns", false);

    // Row 1: In Use | Allocs | Freed
    ImGui::Text("In Use:");
    ImGui::NextColumn();
    ImGui::Text("%.2f KB", currentUsage / 1024.0f);
    ImGui::NextColumn();
    ImGui::Text("Allocs:");
    ImGui::NextColumn();
    ImGui::Text("%.2f KB", totalAllocs / 1024.0f);
    ImGui::NextColumn();
    ImGui::Text("Freed:");
    ImGui::NextColumn();
    ImGui::Text("%.2f KB", totalFreed / 1024.0f);
    ImGui::NextColumn();

    // Row 2: Free Blocks | Pages | Pool
    ImGui::Text("Free Blocks:");
    ImGui::NextColumn();
    ImGui::Text("%d", freeBlocks);
    ImGui::NextColumn();
    ImGui::Text("Pages:");
    ImGui::NextColumn();
    ImGui::Text("%d", pageCount);
    ImGui::NextColumn();
    ImGui::Text("Pool:");
    ImGui::NextColumn();
    ImGui::Text("%.2f KB", totalPoolSize / 1024.0f);
    ImGui::NextColumn();

    // Row 3: ECS Chunks | ECS Chunk Bytes | MM Covers ECS
    ImGui::Text("ECS Chunks:");
    ImGui::NextColumn();
    if (hasWorld) ImGui::Text("%u", ecsChunkCount);
    else ImGui::TextDisabled("N/A");
    ImGui::NextColumn();
    ImGui::Text("ECS Chunk Bytes:");
    ImGui::NextColumn();
    if (hasWorld) ImGui::Text("%.2f KB", ecsChunkBytes / 1024.0f);
    else ImGui::TextDisabled("N/A");
    ImGui::NextColumn();
    ImGui::Text("MM Covers ECS:");
    ImGui::NextColumn();

	// Display Yes/No/N/A based on whether we have a world
    if (!hasWorld) ImGui::TextDisabled("N/A");
    else if (mmCoversEcs) ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Yes");
    else ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "No");

    ImGui::NextColumn();

	// End columns
    ImGui::Columns(1);

	// Validate memory stats only once to verify that the Memory Manager is working correctly
    if (!m_statsValidated) {
        m_statsValidated = true;

		// Run validation
        if (!_validateMemoryStats()) LOG_ERROR("Memory Manager stats MISMATCH");
    }
}

// Runs allocation and free probes to verify memory manager counters remain internally consistent
bool PerformancePanel::_validateMemoryStats() {
    MemoryManager& mm = MemoryManager::GetInstance();

	// Each allocation rounded up to nearest 16 bytes
	// This gives us a minimum expected delta; internal fragmentation may add a small overhead
	constexpr int TEST_SIZES[] = { 64, 128, 256 };  // Aligned sizes
	constexpr int NUM_TESTS = 3;                    // Number of test allocations
	constexpr size_t ALIGNMENT = 16;                // Memory alignment

    // If a block cannot be split, the allocator may consume a bit of extra space (internal fragmentation)
    // Worst-case overhead per allocation is < header + alignment
    // We conservatively allow up to 2 * ALIGNMENT per allocation
    constexpr size_t MAX_INTERNAL_FRAG = ALIGNMENT * 2;
    size_t expectedAllocDelta = 0;

	// Calculate expected allocation delta
    for (int i{}; i < NUM_TESTS; i++) {

		// Align size to nearest multiple of ALIGNMENT
		// Adding (ALIGNMENT - 1) ensures that any size that is not already a multiple of ALIGNMENT will round up 
        // to the next multiple when the bitwise AND with the negated ALIGNMENT - 1 is applied
        // This effectively clears the lower bits, aligning the size up; for example, if ALIGNMENT is 16 (0x10), 
        // then ALIGNMENT - 1 is 15 (0x0F)
        // Negating this gives 0xFFFFFFF0, which has the lower 4 bits cleared
        size_t aligned = (static_cast<size_t>(TEST_SIZES[i]) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);

		// Accumulate expected allocation delta
        expectedAllocDelta += aligned;
    }

	// Snapshot before alloc
    size_t allocBefore = mm.GetTotalAllocated();
    size_t freedBefore = mm.GetTotalFreed();
    size_t usageBefore = mm.GetCurrentUsage();
    size_t poolBefore = mm.GetTotalPoolSize();
    int freeBlocksBefore = mm.GetFreeBlockCount();
    int pagesBefore = mm.GetPageCount();

	// Compute page size before
    size_t pageSizeBefore = 0;
    bool pageSizeBeforeOk = false;

	// Compute only if we have pages
    if (pagesBefore > 0) {
        pageSizeBefore = poolBefore / static_cast<size_t>(pagesBefore);
        pageSizeBeforeOk = (poolBefore % static_cast<size_t>(pagesBefore) == 0) && (pageSizeBefore > 0);
    }

	// Store pointers to allocated blocks
    void* ptrs[NUM_TESTS] = {};

	// Perform allocations
    for (int i{}; i < NUM_TESTS; i++)

		// Allocate and store pointer
        ptrs[i] = mm.Allocate(TEST_SIZES[i]);

	// Snapshot after alloc
    size_t allocAfterAlloc = mm.GetTotalAllocated();
    size_t freedAfterAlloc = mm.GetTotalFreed();
    size_t usageAfterAlloc = mm.GetCurrentUsage();
    size_t poolAfterAlloc = mm.GetTotalPoolSize();
    int freeBlocksAfterAlloc = mm.GetFreeBlockCount();
    int pagesAfterAlloc = mm.GetPageCount();

	// Free allocations
    for (int i{}; i < NUM_TESTS; i++)
        if (ptrs[i]) mm.Deallocate(ptrs[i]);

	// Snapshot after free
    size_t allocAfterFree = mm.GetTotalAllocated();
    size_t freedAfterFree = mm.GetTotalFreed();
    size_t usageAfterFree = mm.GetCurrentUsage();
    size_t poolAfterFree = mm.GetTotalPoolSize();
    int freeBlocksAfterFree = mm.GetFreeBlockCount();
    int pagesAfterFree = mm.GetPageCount();

    // How much allocator's tracked bytes changed across alloc/free operations
    const size_t allocDelta = (allocAfterAlloc - allocBefore);
    const size_t freeDelta = (freedAfterFree - freedBefore);
    const size_t usageDelta = (usageAfterAlloc - usageBefore);

    // Fragmentation tolerance exists because if a free block can't be split (too small to hold both request
    // and leftover block with header), the allocator may give a slightly larger chunk than we asked for
    // Rather than leaving an unusably tiny remainder
    const size_t maxAllocDelta = expectedAllocDelta + (NUM_TESTS * MAX_INTERNAL_FRAG);

	// Verify that allocated and freed deltas are within expected range
    // (Allowing small internal fragmentation when blocks cannot be split)
	bool allocDeltaOk = (allocDelta >= expectedAllocDelta) && (allocDelta <= maxAllocDelta);
	bool freeDeltaOk = (freeDelta == allocDelta);
	bool freedStableDuringAllocOk = (freedAfterAlloc == freedBefore); // Freed did not change during alloc

	// Usage checks
	bool usageRestoredOk = (usageAfterFree == usageBefore);           // Usage restored after free
	bool usageDuringOk = (usageDelta == allocDelta);                  // Usage increased correctly during alloc

	// Usage consistency checks
	bool usageConsistencyBeforeOk = (allocBefore >= freedBefore) && (usageBefore == (allocBefore - freedBefore));                          // Before alloc
	bool usageConsistencyAfterAllocOk = (allocAfterAlloc >= freedAfterAlloc) && (usageAfterAlloc == (allocAfterAlloc - freedAfterAlloc));  // After alloc
	bool usageConsistencyAfterFreeOk = (allocAfterFree >= freedAfterFree) && (usageAfterFree == (allocAfterFree - freedAfterFree));        // After free

	// Monotonicity checks
	// Monotonicity = values should not decrease over time
    bool totalsMonotonicOk = (allocAfterAlloc >= allocBefore) && (allocAfterFree >= allocAfterAlloc) && 
		(freedAfterAlloc >= freedBefore) && (freedAfterFree >= freedAfterAlloc);                      // Totals should not decrease

	// Monotonicity for pages and pool size
	bool pagesMonotonicOk = (pagesAfterAlloc >= pagesBefore) && (pagesAfterFree >= pagesAfterAlloc);  // Pages should not decrease
	bool poolMonotonicOk = (poolAfterAlloc >= poolBefore) && (poolAfterFree >= poolAfterAlloc);       // Pool size should not decrease

	// Pool size checks
	bool poolBeforeOk = (pagesBefore > 0) && (poolBefore > 0) && (poolBefore >= usageBefore);         // Before alloc
	bool poolAfterAllocOk = (pagesAfterAlloc > 0) && (poolAfterAlloc >= usageAfterAlloc);             // After alloc
	bool poolAfterFreeOk = (pagesAfterFree > 0) && (poolAfterFree >= usageAfterFree);                 // After free

	// Page size checks
	bool pageSizeAfterAllocOk = (pagesAfterAlloc > 0) && (poolAfterAlloc % static_cast<size_t>(pagesAfterAlloc) == 0);  // After alloc
	bool pageSizeAfterFreeOk = (pagesAfterFree > 0) && (poolAfterFree % static_cast<size_t>(pagesAfterFree) == 0);      // After free

	// Page size consistency checks
    bool pageSizeConsistentOk = true;

	// Check only if we had a valid page size before
    if (pageSizeBeforeOk) {

		// After alloc
        if (pagesAfterAlloc > 0) {

			// Compute page size after alloc
            size_t pageSizeAfterAlloc = poolAfterAlloc / static_cast<size_t>(pagesAfterAlloc);

			// Compare with before
            if (pageSizeAfterAlloc != pageSizeBefore) pageSizeConsistentOk = false;
        }

		// After free
        if (pagesAfterFree > 0) {

			// Compute page size after free
            size_t pageSizeAfterFree = poolAfterFree / static_cast<size_t>(pagesAfterFree);

			// Compare with before
            if (pageSizeAfterFree != pageSizeBefore) pageSizeConsistentOk = false;
        }
    }

    // Free block count checks
    // If no new pages were added, free blocks should not increase after alloc
    bool freeBlocksDroppedOk = true;
    if (pagesAfterAlloc == pagesBefore) freeBlocksDroppedOk = (freeBlocksAfterAlloc <= freeBlocksBefore);

    // After freeing, free blocks should return to at least the original count
    bool freeBlocksRestoredOk = (freeBlocksAfterFree >= freeBlocksBefore);

	// Pointer validity checks
    bool allPtrsOk = true;
    for (int i{}; i < NUM_TESTS; i++)

		// Check each pointer is not nullptr
        if (!ptrs[i]) { allPtrsOk = false; break; }

	// Final overall result
    bool passed = allocDeltaOk && freeDeltaOk && freedStableDuringAllocOk && usageRestoredOk && usageDuringOk &&
        usageConsistencyBeforeOk && usageConsistencyAfterAllocOk && usageConsistencyAfterFreeOk &&
        totalsMonotonicOk && pagesMonotonicOk && poolMonotonicOk && poolBeforeOk && poolAfterAllocOk && 
        poolAfterFreeOk && pageSizeBeforeOk && pageSizeAfterAllocOk && pageSizeAfterFreeOk && pageSizeConsistentOk &&
        allPtrsOk && freeBlocksDroppedOk && freeBlocksRestoredOk;

    if (passed) {
        LOG_INFO("Memory Manager stats OK (allocDelta=" << allocDelta
            << ", freeDelta=" << freeDelta
            << ", usageDelta=" << usageDelta
            << ", expected range=[" << expectedAllocDelta << ", " << maxAllocDelta << "])");
    }

	// Log detailed failure reasons
    if (!allocDeltaOk) LOG_ERROR("FAIL [1] allocDelta: got " << allocDelta << ", expected range [" << expectedAllocDelta << ", " << maxAllocDelta << "]");
    if (!freeDeltaOk) LOG_ERROR("FAIL [2] freeDelta: got " << freeDelta << ", expected " << allocDelta);
    if (!freedStableDuringAllocOk) LOG_ERROR("FAIL [3] freed changed during alloc: before=" << freedBefore << ", after=" << freedAfterAlloc);
    if (!usageRestoredOk) LOG_ERROR("FAIL [4] usageRestored: after=" << usageAfterFree << ", before=" << usageBefore);
    if (!usageDuringOk) LOG_ERROR("FAIL [5] usageDuring: got " << usageDelta << ", expected " << allocDelta);
    if (!usageConsistencyBeforeOk) LOG_ERROR("FAIL [6] usageBefore != allocBefore - freedBefore");
    if (!usageConsistencyAfterAllocOk) LOG_ERROR("FAIL [7] usageAfterAlloc != allocAfterAlloc - freedAfterAlloc");
    if (!usageConsistencyAfterFreeOk) LOG_ERROR("FAIL [8] usageAfterFree != allocAfterFree - freedAfterFree");
    if (!totalsMonotonicOk) LOG_ERROR("FAIL [9] totals not monotonic across snapshots");
    if (!poolBeforeOk) LOG_ERROR("FAIL [10] poolBefore invalid (pages=" << pagesBefore << ", pool=" << poolBefore << ", usage=" << usageBefore << ")");
    if (!poolAfterAllocOk) LOG_ERROR("FAIL [11] poolAfterAlloc invalid (pages=" << pagesAfterAlloc << ", pool=" << poolAfterAlloc << ", usage=" << usageAfterAlloc << ")");
    if (!poolAfterFreeOk) LOG_ERROR("FAIL [12] poolAfterFree invalid (pages=" << pagesAfterFree << ", pool=" << poolAfterFree << ", usage=" << usageAfterFree << ")");
    if (!pageSizeBeforeOk) LOG_ERROR("AIL [13] pageSizeBefore invalid (pool=" << poolBefore << ", pages=" << pagesBefore << ")");
    if (!pageSizeAfterAllocOk) LOG_ERROR("FAIL [14] pageSizeAfterAlloc invalid (pool=" << poolAfterAlloc << ", pages=" << pagesAfterAlloc << ")");
    if (!pageSizeAfterFreeOk) LOG_ERROR("FAIL[15] pageSizeAfterFree invalid(pool = " << poolAfterFree << ", pages = " << pagesAfterFree << ")");
    if (!pageSizeConsistentOk) LOG_ERROR("FAIL [16] page size changed across snapshots");
    if (!allPtrsOk) LOG_ERROR("FAIL [17] one or more ptrs are nullptr");
    if (!freeBlocksDroppedOk) LOG_ERROR("FAIL [18] freeBlocks went UP without new pages: before=" << freeBlocksBefore << ", after=" << freeBlocksAfterAlloc);
    if (!freeBlocksRestoredOk) LOG_ERROR("FAIL [19] freeBlocks not restored: before=" << freeBlocksBefore << ", after=" << freeBlocksAfterFree);
    if (!pagesMonotonicOk) LOG_ERROR("FAIL [20] page count decreased: before=" << pagesBefore << ", afterAlloc=" << pagesAfterAlloc << ", afterFree=" << pagesAfterFree);
    if (!poolMonotonicOk) LOG_ERROR("FAIL [21] pool size decreased: before=" << poolBefore << ", afterAlloc=" << poolAfterAlloc << ", afterFree=" << poolAfterFree);

	// Return overall result (whether all checks passed or not)
    return passed;
}

// Draws allocator benchmark controls and last-run timing feedback
void PerformancePanel::_renderMemoryBenchmark() {
    ImGui::PushFont(m_boldFont);
    ImGui::Text("Memory Benchmark");
    ImGui::PopFont();

    // Memory Allocator Test UI
    // Positioned at the far right of the bar
    ImGui::PushFont(m_mainFont);

    const char* checkLabel = "Custom Allocator";  // Checkbox label
    const char* btnLabel = "Test Allocator";      // Button label
    char timeLabel[32] = "";                      // Buffer for time display

    // Only show time if we have a valid last test time (not sentinel/invalid value of -1.0)
    if (m_lastTestTime >= 0.0) snprintf(timeLabel, sizeof(timeLabel), "Time: %.2f ms", m_lastTestTime);

    // Access current style for size calculations
    // ImGuiStyle& style = ImGui::GetStyle();

    // Vertical alignment fix: Align text to frame padding to center vertically with buttons
    ImGui::AlignTextToFramePadding();
    ImGui::Checkbox(checkLabel, &m_useCustomAllocator);
    ImGui::SameLine();

    // Run test with 10,000 allocations between 16 and 1024 bytes
	// Style button to match frame background
    const ImVec4 btnColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);

	// Hover and active colors
    const ImVec4 btnHover = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered);
    const ImVec4 btnActive = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);

	// Style button colors
    ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnActive);

	// On button press, run benchmark and store last test time
    if (ImGui::Button(btnLabel)) m_lastTestTime = MemoryManager::GetInstance().Benchmark(m_useCustomAllocator, 10000, 16, 1024);
    
	// Pop button style colors
    ImGui::PopStyleColor(3);

    // Tooltip for button
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Performance Test:");
        ImGui::Text("- Allocates & frees 10,000 random blocks");
        ImGui::Text("- Block sizes: 16B to 1024B");
        ImGui::Text("- Measures total execution time");
        ImGui::EndTooltip();
    }

    // Display time if valid
    if (m_lastTestTime >= 0.0) {

        // Color coding based on performance
        ImVec4 color;
        if (m_lastTestTime < 10.0) color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);      // Green for fast (< 10ms)
        else if (m_lastTestTime < 30.0) color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); // Yellow for warning (10-30ms)
        else color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);                            // Red for slow (> 30ms)

        // Display time label with color
        ImGui::TextColored(color, "%s", timeLabel);
    }

    // Reset font scale and pop font stack
    // ImGui::SetWindowFontScale(1.0f); 
    ImGui::PopFont();
}

// -------------------------------------------------------------------------
// Private Data Management
// -------------------------------------------------------------------------

// Pulls latest profiler and world data into cached fields used by render methods
void PerformancePanel::_updateCachedData() {
    m_hasCollectedData = true;

    // Cache live data continuously
    try {
        m_cachedFps = TimeSystem::Instance().GetFPS();
        m_cachedFrameMs = TimeSystem::Instance().GetFrameTimeMs();
        m_cachedTotalTime = TimeSystem::Instance().GetTotalScopeTimes();

        // Cache scope data and track min/max frame times
        const auto liveScopes = TimeSystem::Instance().GetAllScopeData();
        m_cachedScopes.clear();

        // Track frame time variation
        static std::deque<float> frameTimeHistory;
        frameTimeHistory.push_back(m_cachedFrameMs);
        if (frameTimeHistory.size() > 60) {  // Keep last 60 frames
            frameTimeHistory.pop_front();
        }

        if (!frameTimeHistory.empty()) {
            m_cachedMinFrameMs = *std::min_element(frameTimeHistory.begin(), frameTimeHistory.end());
            m_cachedMaxFrameMs = *std::max_element(frameTimeHistory.begin(), frameTimeHistory.end());
        }

        for (const auto& kv : liveScopes) {
            CachedScopeData cached;
            cached.LastTimeMs = kv.second.LastTimeMs;
            cached.AverageTimeMs = kv.second.AverageTimeMs;
            cached.MaxTimeMs = kv.second.MaxTimeMs;

            // Cache system info from SystemManager. Keep non-system scopes
            // visible as generic engine/editor frame scopes.
            if (m_systemManager) {
                if (auto* system = m_systemManager->GetSystem(kv.first)) {
                    cached.isKnownSystem = true;
                    cached.isScripted = m_systemManager->IsScriptedSystem(kv.first);
                    cached.isEnabled = system->IsEnabled();
                }
            }

            m_cachedScopes[kv.first] = cached;
        }

        // Get entity and component count from World if available
        if (m_world) {

            // Count entities by iterating through archetypes
            uint32_t totalEntities = 0;
            uint32_t totalComponents = 0;

            const auto archetypes = m_world->GetAllArchetypes();
            for (const auto* archetype : archetypes) {
                if (archetype) {

                    // Each archetype contains entities with a specific set of components
                    const uint32_t chunkCount = archetype->GetChunkCount();

                    for (uint32_t ci = 0; ci < chunkCount; ++ci) {
                        const auto* chunk = archetype->GetChunk(ci);

                        if (chunk) {
                            uint32_t entityCount = chunk->Count();
                            totalEntities += entityCount;

                            // Each entity in this archetype has the same number of components
                            uint32_t componentsPerEntity = static_cast<uint32_t>(archetype->GetComponents().size());
                            totalComponents += entityCount * componentsPerEntity;
                        }
                    }
                }
            }

            m_cachedEntityCount = totalEntities;
            m_cachedComponentCount = totalComponents;
        }
        else {
            m_cachedEntityCount = 0;
            m_cachedComponentCount = 0;
        }
    }
    catch (...) {
        // Keep existing cached values on error
    }
}
