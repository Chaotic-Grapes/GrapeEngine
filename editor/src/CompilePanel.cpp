/* Start Header *****************************************************************/
/*!
\file    CompilePanel.cpp
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file defines the CompilePanel class which is responsible for displaying 
compilation status, progress, and logging diagnostics to the engine logger.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#include "CompilePanel.h"
#include "core/Application.h"
#include "core/Logger.h"
#include "scripting/ScriptManager.h"
#include <imgui.h>
#include <algorithm>
#include <string>
#include <vector>

bool s_compileModalOpened = false;

// Store references and prepare the compile panel for use
void CompilePanel::Initialize() {
    // Nothing for now
}

// Release compile panel resources
void CompilePanel::Shutdown() {
    // Nothing for now
}

// Render the compile status modal, showing progress bar or error diagnostics
void CompilePanel::Render() {
    // Grab the script manager from the engine core; may be null if engine is not yet ready
    ECS::ScriptManager* scriptMgr = Engine::CORE ? Engine::CORE->GetScriptManager() : nullptr;

    // Poll current compile state: status code, percent progress, and optional message string
    // Status codes: 0=Idle, 1=Started, 2=In-progress, 3=Success, 4=Error
    int status = 0;
    int progress = -1;  // -1 means indeterminate (no percentage available yet)
    std::string msg;
    if (scriptMgr) {
        scriptMgr->GetCompileStatus(status, progress, msg);
    }

    // Map status code to a human-readable label shown in the modal header
    const char* statusText = "Idle";
    if (status == 1 || status == 2) statusText = "Compiling C# Scripts...";
    else if (status == 3) statusText = "Last compile: OK";
    else if (status == 4) statusText = "Last compile: ERROR";

    // Open the modal once when compilation first starts; guard prevents re-opening each frame
    if (status == 1 && !s_compileModalOpened) {
        ImGui::OpenPopup("Compile Status");
        s_compileModalOpened = true;
    }

    // Center the modal on the main viewport so it does not appear at a stale position
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Compile Status", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        // Apply comfortable padding and spacing for the modal contents
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        // Color palette used for status-dependent header text
        const ImVec4 okColor(0.35f, 0.85f, 0.55f, 1.0f);    // green: success
        const ImVec4 warnColor(0.95f, 0.75f, 0.25f, 1.0f);  // yellow: in progress
        const ImVec4 errColor(0.92f, 0.35f, 0.35f, 1.0f);   // red: error
        const ImVec4 textMuted(1.0f, 1.0f, 1.0f, 0.75f);    // dim white: idle/secondary text

        // Auto-dismiss once the compiler reports a terminal state (success or failure)
        // The caller is expected to re-open on the next compile cycle
        if (status == 3 || status == 4) {
            ImGui::CloseCurrentPopup();
            s_compileModalOpened = false;
            ImGui::PopStyleVar(3);
            ImGui::EndPopup();
            return;
        }

        // Pick header color based on current status
        ImVec4 headerColor = textMuted;
        if (status == 1 || status == 2) headerColor = warnColor;
        if (status == 4) headerColor = errColor;
        if (status == 3) headerColor = okColor;

        // Render the colored status label at the top of the modal
        ImGui::TextColored(headerColor, "%s", statusText);

        // Show the compiler's detail message below the label if one is available
        if (!msg.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, textMuted);
            ImGui::TextWrapped("%s", msg.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Separator();

        if (status == 1 || status == 2) {
            // Compile is in progress — show a determinate or indeterminate progress bar
            const float barWidth = std::max(400.0f, ImGui::GetContentRegionAvail().x);
            if (progress >= 0) {
                // Clamp to [0,1] in case the compiler reports out-of-range values
                float frac = std::clamp(progress / 100.0f, 0.0f, 1.0f);
                ImGui::ProgressBar(frac, ImVec2(barWidth, 0.0f));
            }
            else {
                // No percentage available; use a fixed 50% bar as an indeterminate indicator
                ImGui::ProgressBar(0.5f, ImVec2(barWidth, 0.0f));
            }
        }
        else {
            if (status == 4) {
                // Compile failed — fetch per-line diagnostics from the script manager
                std::vector<std::string> diags;
                if (scriptMgr) {
                    scriptMgr->GetLastDiagnosticsLines(diags);
                }

                if (!diags.empty()) {
                    // Print each diagnostic line (errors, warnings) so the user can read them
                    for (const auto& d : diags) {
                        ImGui::TextWrapped("%s", d.c_str());
                    }
                }
                else if (!msg.empty()) {
                    // Fall back to the status message if no per-line diagnostics were captured
                    ImGui::TextWrapped("%s", msg.c_str());
                }
                else {
                    // Last resort: generic failure message
                    ImGui::TextUnformatted("Compilation failed");
                }
            }
            else {
                // Status is neither in-progress nor an error at this point; show a done label
                ImGui::TextUnformatted("Done");
            }

            // Allow the user to manually dismiss the modal
            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
                s_compileModalOpened = false;
            }
        }

        ImGui::PopStyleVar(3);
        ImGui::EndPopup();
    }
}
