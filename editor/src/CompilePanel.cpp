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
    if (status == 1) statusText = "Compiling...";
    else if (status == 3) statusText = "Last compile: OK";
    else if (status == 4) statusText = "Last compile: ERR";

    // Open/close modal according to compile state
    if (status == 1 && !s_compileModalOpened) {
        ImGui::OpenPopup("Compile Status");
        s_compileModalOpened = true;
    }
    if (status != 1 && s_compileModalOpened) {
        if (status == 4 && !msg.empty()) LOG_ERROR("Script compile failed:\n" << msg);
        else if (status == 3) LOG_INFO("Scripts compiled successfully.");

        ImGui::CloseCurrentPopup();
        s_compileModalOpened = false;
    }

    if (ImGui::BeginPopupModal("Compile Status", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted(statusText);
        ImGui::Separator();

        if (status == 1) {
            if (progress >= 0) {
                float frac = std::clamp(progress / 100.0f, 0.0f, 1.0f);
                ImGui::ProgressBar(frac, ImVec2(400, 0));
                ImGui::Text("%d%%", progress);
            }
            else {
                ImGui::TextUnformatted("Compiling... Please wait.");
                ImGui::ProgressBar(0.5f, ImVec2(400, 0));
            }
        }
        else {
            if (status == 4 && !msg.empty()) {
                ImGui::TextWrapped("%s", msg.c_str());
            }
            else {
                ImGui::TextUnformatted("Done");
            }

            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
                s_compileModalOpened = false;
            }
        }

        ImGui::EndPopup();
    }
}
