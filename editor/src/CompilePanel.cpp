/*
 * CompilePanel.cpp
 * Implements a small modal UI that shows compilation status/progress
 * and logs diagnostics to the engine logger so ConsolePanel captures them.
 */

#include "CompilePanel.h"
#include "core/Application.h"
#include "core/Logger.h"
#include "scripting/ScriptManager.h"
#include <imgui.h>
#include <algorithm>
#include <string>
#include <vector>

bool s_compileModalOpened = false;

void CompilePanel::Initialize() {
    // Nothing for now
}

void CompilePanel::Shutdown() {
    // Nothing for now
}

void CompilePanel::Render() {
    ECS::ScriptManager* scriptMgr = Engine::CORE ? Engine::CORE->GetScriptManager() : nullptr;

    int status = 0;
    int progress = -1;
    std::string msg;
    if (scriptMgr) {
        scriptMgr->GetCompileStatus(status, progress, msg);
    }

    const char* statusText = "Idle";
    if (status == 1 || status == 2) statusText = "Compiling C# Scripts...";
    else if (status == 3) statusText = "Last compile: OK";
    else if (status == 4) statusText = "Last compile: ERROR";

    // Open modal when compilation starts
    if (status == 1 && !s_compileModalOpened) {
        ImGui::OpenPopup("Compile Status");
        s_compileModalOpened = true;
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (vp) {
        ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if (ImGui::BeginPopupModal("Compile Status", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        const ImVec4 okColor(0.35f, 0.85f, 0.55f, 1.0f);
        const ImVec4 warnColor(0.95f, 0.75f, 0.25f, 1.0f);
        const ImVec4 errColor(0.92f, 0.35f, 0.35f, 1.0f);
        const ImVec4 textMuted(1.0f, 1.0f, 1.0f, 0.75f);

        // If compilation finished (success or failure), close the popup immediately
        if (status == 3 || status == 4) {
            ImGui::CloseCurrentPopup();
            s_compileModalOpened = false;
            ImGui::PopStyleVar(3);
            ImGui::EndPopup();
            return;
        }

        ImVec4 headerColor = textMuted;
        if (status == 1 || status == 2) headerColor = warnColor;
        if (status == 4) headerColor = errColor;
        if (status == 3) headerColor = okColor;

        ImGui::TextColored(headerColor, "%s", statusText);
        if (!msg.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, textMuted);
            ImGui::TextWrapped("%s", msg.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Separator();

        if (status == 1 || status == 2) {
            const float barWidth = std::max(400.0f, ImGui::GetContentRegionAvail().x);
            if (progress >= 0) {
                float frac = std::clamp(progress / 100.0f, 0.0f, 1.0f);
                ImGui::ProgressBar(frac, ImVec2(barWidth, 0.0f));
                // ImGui::PushStyleColor(ImGuiCol_Text, textMuted);
                // ImGui::Text("%d%%", progress);
                // ImGui::PopStyleColor();
            }
            else {
                ImGui::ProgressBar(0.5f, ImVec2(barWidth, 0.0f));
            }
        }
        else {
            if (status == 4) {
                std::vector<std::string> diags;
                if (scriptMgr) {
                    scriptMgr->GetLastDiagnosticsLines(diags);
                }

                if (!diags.empty()) {
                    for (const auto& d : diags) {
                        ImGui::TextWrapped("%s", d.c_str());
                    }
                }
                else if (!msg.empty()) {
                    ImGui::TextWrapped("%s", msg.c_str());
                }
                else {
                    ImGui::TextUnformatted("Compilation failed");
                }
            }
            else {
                ImGui::TextUnformatted("Done");
            }

            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
                s_compileModalOpened = false;
            }
        }

        ImGui::PopStyleVar(3);
        ImGui::EndPopup();
    }
}
