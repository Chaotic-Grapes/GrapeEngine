#include "Profiler.h"
#include "systems/Logger.h"
#include <iostream>
#include <stdexcept>
#include <numeric>
#include "DebugUI.h"

/*naming conventions:

public functions = files = FooBar()
private functions = _fooBar()
public data member = FooRealQuick
private data member = m_fooRealQuick
public macro = ALL_CAPS (includes global const)
*/

// Profiler::Profiler();

 //   Logger::Get().Log(INFO, "Profiler system initialized."); // print upon initialization 




  //  DebugUI::NewFrame();

    /*"Logic System Time Consumption: " << PUT_HERE << "%" << std::endl
"Physics System Time Consumption: " << PUT_HERE << "%" << std::endl
"Collision System Time Consumption: " << PUT_HERE << "%" << std::endl
"Transform System Time Consumption: " << PUT_HERE << "%" << std::endl
"Audio System Time Consumption: " << PUT_HERE << "%" << std::endl
"Graphics System Time Consumption: " << PUT_HERE << "%" << std::endl*/


void Profiler::RenderUI() {
    // Create profiler window with position and size (following teammate's pattern)
    ImGui::SetNextWindowPos(ImVec2(10, 350), ImGuiCond_Once);     // Position below other debug windows
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Once);   // Make it wider for the data

    ImGui::Begin("Performance Profiler");

    // Engine status section
    ImGui::Text("Profiler Status: Active");
    ImGui::Separator();  // Visual divider line (like teammate's code)

    // Get the total time for the entire game loop
    float totalFrameTime = 0.0f;
    if (m_scopes.find("Frame") != m_scopes.end()) {
        totalFrameTime = m_scopes["Frame"].lastTimeMs;
        ImGui::Text("Total Frame Time: %.3f ms (%.1f FPS)", totalFrameTime, 1000.0f / totalFrameTime);
    }
    else {
        ImGui::Text("Total Frame Time: N/A");
    }

    ImGui::Separator();  // Another separator
    ImGui::Text("=== System Performance ===");  // Section header like teammate's style

    // Display individual systems as a percentage of the total frame time
    for (auto const& [name, data] : m_scopes) {
        if (name == "Frame") {
            continue; // Already displayed above
        }
        float percentage = 0.0f;
        if (totalFrameTime > 0.0f) {
            percentage = (data.lastTimeMs / totalFrameTime) * 100.0f;
        }
        ImGui::Text("%s: %.2f%% (%.3f ms)", name.c_str(), percentage, data.lastTimeMs);

        // Show the performance graph for each system
        ImGui::PlotLines(("##" + name).c_str(), data.frameTimes.data(), data.frameTimes.size(), 0, "ms", 0.0f, data.maxTimeMs * 1.2f, ImVec2(0, 60));
        ImGui::Separator();
    }

    // Add some interactive buttons (following teammate's button pattern)
    if (ImGui::Button("Clear Performance History")) {
        // Clear all frame time histories
        for (auto& [name, data] : m_scopes) {
            data.frameTimes.clear();
            data.avgTimeMs = 0.0f;
            data.maxTimeMs = 0.0f;
        }
        Logger::Get().Log(LogLevel::INFO, "Performance history cleared.");
    }

    ImGui::End();  // Complete window definition (like teammate's code)

    // Render DebugUI (this includes both the debug console and your profiler UI)
    DebugUI::Render();
}

void Profiler::BeginScope(const std::string& scopeName) {
    m_startTimes[scopeName] = std::chrono::steady_clock::now();
}

void Profiler::EndScope(const std::string& scopeName) {
    auto it = m_startTimes.find(scopeName);
    if (it == m_startTimes.end()) {
        Logger::Get().Log(LogLevel::WARNING, "Profiler scope '" + scopeName + "' not found. Ignoring.");
        return;
    }
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second);
    m_startTimes.erase(it);
    // Get the current scope data, or create it if it doesn't exist
    ScopeData& scopeData = m_scopes[scopeName];
    scopeData.lastTimeMs = static_cast<float>(duration.count()) / 1000.0f;
    // Maintain a history of frame times
    scopeData.frameTimes.push_back(scopeData.lastTimeMs);
    if (scopeData.frameTimes.size() > MAX_HISTORY_FRAMES) {
        scopeData.frameTimes.erase(scopeData.frameTimes.begin());
    }
    // Update avg and max
    float sum = std::accumulate(scopeData.frameTimes.begin(), scopeData.frameTimes.end(), 0.0f);
    scopeData.avgTimeMs = sum / scopeData.frameTimes.size();
    scopeData.maxTimeMs = *std::max_element(scopeData.frameTimes.begin(), scopeData.frameTimes.end());
    // Log to the console for post-mortem analysis
    std::string output = "Scope '" + scopeName + "' took " + std::to_string(scopeData.lastTimeMs) + " ms.";
    Logger::Get().Log(LogLevel::INFO, output);
}


