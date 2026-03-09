/* Start Header *****************************************************************/
/*!
\file   PlaybackControls.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the PlaybackControls class for managing game playback state in the
level editor.

Features:
- Game state management with play/pause/stop/step functionality
- Keyboard shortcuts (Ctrl+P, Ctrl+Shift+P, Alt+P)
- World state serialization and restoration for play/stop
- ImGui UI with tooltips and Material Symbols icons
- Step-by-step physics frame execution for debugging
- State change callbacks for external coordination

Reference:
- ImGui UI layout and button styling (imgui.h)
*/
/* End Header *******************************************************************/

#include "PlaybackControls.h"
#include "services/Input.h"
#include "services/TimeSystem.h"
#include "services/MemoryManager.h"
#include "ecs/World.h"
#include "ecs/Components.h"
#include "ecs/PrefabManager.h"
#include "core/Logger.h"
#include "core/Application.h"
#include "core/ProjectPaths.h"
#include "scene/Scene.h"
#include "serialization/EntitySerializer.h"
#include "scripting/ScriptManager.h"
#include "helpers/EntityUtils.h"
#include "EditorECSUtils.h"
#include "EditorStyle.h"
#include "EditorIcons.h"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>

// Forward declarations for managed component deserialization interop
extern "C" void WorldInterop_DeserializeComponentFromJson(void* worldPtr, uint64_t entityId, uint32_t componentTypeHash, const char* jsonStr);

// Constructor: stores pointer to the world for playback operations.
// Starts with default stopped state and no saved snapshot.
Playback::Playback(ECS::World* world)
    : m_world(world) {
}

Playback::~Playback() {}

// Initialize fonts used by the playback UI and Material Symbols.
// Keeps pointers for drawing icons on control buttons.
void Playback::Initialize(ImFont* mainFont, ImFont* symbolsFont, float toolbarHeight) {
    m_mainFont = mainFont;
    m_symbolsFont = symbolsFont;
    m_toolbarHeight = toolbarHeight;
}

// Process keyboard input for play/stop, pause/resume, and step.
// Maps Ctrl+P, Ctrl+Shift+P, and Alt+P into state changes/flags.
void Playback::ProcessInput() {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput) {
        return;
    }

    const bool ctrlDown = Input::IsKeyDown(KEY_LEFT_CONTROL) || Input::IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shiftDown = Input::IsKeyDown(KEY_LEFT_SHIFT) || Input::IsKeyDown(KEY_RIGHT_SHIFT);
    const bool altDown = Input::IsKeyDown(KEY_LEFT_ALT) || Input::IsKeyDown(KEY_RIGHT_ALT);

    // Play/Stop: Ctrl + P
    if (Input::IsKeyPressed(KEY_P) && ctrlDown && !shiftDown && !altDown)
    {
        if (m_editorState == EditorState::Edit) {
            if (_startPlayFromEdit()) {
                LOG_INFO("Game started (Ctrl+P)");
            }
        }
        else if (m_editorState == EditorState::Play || m_editorState == EditorState::Paused) {
            // Simulate stop button
            _restoreWorldState();
            _changeState(EditorState::Edit);
            LOG_INFO("Game stopped (Ctrl+P)");
        }
    }

    // Pause/Resume: Ctrl + Shift + P
    if (Input::IsKeyPressed(KEY_P) && ctrlDown && shiftDown)
    {
        if (m_editorState == EditorState::Play) {
            _changeState(EditorState::Paused);
            LOG_INFO("Game paused (Ctrl+Shift+P)");
        }
        else if (m_editorState == EditorState::Paused) {
            _changeState(EditorState::Play);
            LOG_INFO("Game resumed (Ctrl+Shift+P)");
        }
    }

    // Step: Alt + P
    if (Input::IsKeyPressed(KEY_P) && altDown) {
        if (m_editorState == EditorState::Paused) {
            _changeState(EditorState::Step);
            m_stepRequested = true;
            LOG_INFO("Stepping 1 physics frame (Alt+P)");
        }
    }
}

// Render the playback toolbar UI using ImGui.
// Centers buttons and shows tooltips with current state.
void Playback::Render() {
    // Toolbar window flags: NoTitleBar removes title bar, NoScrollbar prevents scrolling,
    // NoResize prevents manual resizing, NoCollapse prevents collapsing
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;

    // Set fixed height for toolbar from config
    ImGui::SetNextWindowSize(ImVec2(-1, m_toolbarHeight), ImGuiCond_Always);

    ImGuiStyle& style = ImGui::GetStyle();
    const ImVec4 baseBg = EditorStyle::WindowBg;
    const ImVec4 playTint = ImVec4(0.12f, 0.24f, 0.18f, 1.0f);
    const ImVec4 pauseTint = ImVec4(0.26f, 0.20f, 0.10f, 1.0f);
    const ImVec4 stepTint = ImVec4(0.14f, 0.20f, 0.26f, 1.0f);

    // Lightweight color blend for mode tinting.
    auto blend = [](const ImVec4& a, const ImVec4& b, float t) {
        return ImVec4(
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w
        );
    };

    ImVec4 tintedBg = baseBg;
    if (m_editorState == EditorState::Play) tintedBg = blend(baseBg, playTint, 0.55f);
    else if (m_editorState == EditorState::Paused) tintedBg = blend(baseBg, pauseTint, 0.55f);
    else if (m_editorState == EditorState::Step) tintedBg = blend(baseBg, stepTint, 0.55f);

    // Mode tint keeps the toolbar state-readable at a glance.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, tintedBg);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::Begin("Game Controls", nullptr, flags);

    const ImVec2 mainBtnSize(42.0f, 26.0f);
    const ImVec2 smallBtnSize(22.0f, 18.0f);

    // Compact icon button with optional active palette.
    auto drawIconButton = [&](const char* icon, bool enabled, const ImVec2& size, const char* tooltip,
                              bool active, const ImVec4& activeColor, std::function<void()> onClick,
                              const ImVec4* baseColor = nullptr,
                              const ImVec4* baseHover = nullptr,
                              const ImVec4* baseActive = nullptr) {
        if (!enabled) ImGui::BeginDisabled();
        if (active) { // Active state color override
            ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(activeColor, 1.1f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(activeColor, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
        }
        if (!active && baseColor && baseHover && baseActive) { // Custom base colors
            ImGui::PushStyleColor(ImGuiCol_Button, *baseColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, *baseHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, *baseActive);
        }

        bool clicked = false;
        ImGui::PushFont(m_symbolsFont);
        // Remove padding inside playback control icon buttons
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
        clicked = ImGui::Button(icon, size);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

        if (tooltip && ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushFont(m_mainFont);
            ImGui::Text("%s", tooltip);
            ImGui::PopFont();
            ImGui::EndTooltip();
        }

        if (!active && baseColor && baseHover && baseActive) ImGui::PopStyleColor(3);
        if (active) ImGui::PopStyleColor(4);
        if (!enabled) ImGui::EndDisabled();
        if (clicked && onClick) onClick();
    };

    // Non-interactive state badge.
    auto drawStatePill = [&](const char* label, const ImVec4& color) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 4));
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
        ImGui::BeginDisabled();
        ImGui::Button(label);
        ImGui::EndDisabled();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    };

    // Top row: centered primary controls, state pill, time scrub on right.
    {
        // Calculate centering offsets
        const float rowStartX = ImGui::GetCursorPosX();
        const float contentWidth = ImGui::GetContentRegionAvail().x;
        const float groupWidth = (mainBtnSize.x * 3.0f) + (style.ItemSpacing.x * 2.0f);

        // Background box around controls
        const ImVec2 controlPadding(8.0f, 4.0f);
        
        // Rounded rectangle parameters
        const float controlRounding = 10.0f;
        const float boxWidth = groupWidth + (controlPadding.x * 2.0f);
        const float boxHeight = mainBtnSize.y + (controlPadding.y * 2.0f);
        const float targetRightWidth = 460.0f;
        const float reservedRight = std::max(0.0f, std::min(targetRightWidth, contentWidth - boxWidth - 20.0f));

        // Center the box within available content width
        float startX = (contentWidth - boxWidth) * 0.5f;

        // Ensure box does not overflow left edge
        startX = std::max(startX, 0.0f);
        ImVec2 rowStartScreen = ImGui::GetCursorScreenPos();
        ImVec2 boxMin(rowStartScreen.x + startX, rowStartScreen.y);
        ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + boxHeight);

        // Draw active scene name on the left without affecting layout.
        const Scenes::Scene* activeScene = Engine::CORE ? Engine::CORE->GetSceneManager().GetActive() : nullptr;
        const char* sceneName = activeScene ? activeScene->GetName().c_str() : "No Scene";
        const float sceneLeftPad = 12.0f;
        const float sceneY = boxMin.y + (boxHeight - ImGui::GetTextLineHeight()) * 0.5f;
        ImFont* sceneFont = m_mainFont ? m_mainFont : ImGui::GetFont();
        ImGui::PushFont(sceneFont);
        const float sceneFontSize = ImGui::GetFontSize();
        ImGui::PopFont();
        ImGui::GetWindowDrawList()->AddText(
            sceneFont,
            sceneFontSize,
            ImVec2(rowStartScreen.x + sceneLeftPad, sceneY),
            ImGui::GetColorU32(EditorStyle::Muted),
            sceneName
        );

        // Draw background box
        ImGui::GetWindowDrawList()->AddRectFilled(
            boxMin, boxMax, ImGui::GetColorU32(EditorStyle::Scale(EditorStyle::FrameBg, 1.1f)),
            controlRounding
        );
        ImGui::GetWindowDrawList()->AddRect(
            boxMin, boxMax, ImGui::GetColorU32(EditorStyle::Scale(EditorStyle::Border, 0.8f)),
            controlRounding
        );

        // Position cursor for first button
        ImGui::SetCursorPosX(rowStartX + startX + controlPadding.x);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + controlPadding.y);

        // Play/Pause button
        const bool canPlay = HasValidWorld();
        const bool canStep = (m_editorState == EditorState::Paused || m_editorState == EditorState::Step);
        const bool isPlaying = (m_editorState == EditorState::Play);
        const bool isPaused = (m_editorState == EditorState::Paused);

        // Determine icon, tooltip, and color based on state
        const char* playIcon = isPlaying ? EditorIcons::Pause : EditorIcons::Play;
        const char* playTooltip = isPlaying ? "Pause (Ctrl+Shift+P)" : (m_editorState == EditorState::Edit ? "Play (Ctrl+P)" : "Resume (Ctrl+Shift+P)");
        const ImVec4 playColor = isPlaying ? EditorStyle::WarningButton : EditorStyle::SuccessButton;
        const bool playActive = isPlaying || isPaused;

        // Draw Play/Pause button
        ImGui::PushID("PlayPause");
        drawIconButton(playIcon, canPlay, mainBtnSize, playTooltip,
            playActive, playColor,
            [this]() {
                // Toggle play/pause/resume based on current state
                if (m_editorState == EditorState::Edit) {
                    if (_startPlayFromEdit()) {
                        LOG_INFO("Game started");
                    }
                } else if (m_editorState == EditorState::Play) { // Pause
                    _changeState(EditorState::Paused);
                    LOG_INFO("Game paused");
                } else { // Resume from Paused or Step
                    _changeState(EditorState::Play);
                    LOG_INFO("Game resumed");
                }
            },
            !playActive ? &EditorStyle::SuccessButton : nullptr, // Custom base colors
            !playActive ? &EditorStyle::SuccessButtonHover : nullptr, // Hover
            !playActive ? &EditorStyle::SuccessButtonActive : nullptr); // Active
        ImGui::PopID();

        // Stop button
        ImGui::SameLine();
        ImGui::PushID("Stop");
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);

        // Draw Stop button
        drawIconButton(EditorIcons::Stop, m_editorState != EditorState::Edit, mainBtnSize,
            "Stop (Ctrl+P)", false, EditorStyle::DangerButton,
            [this]() {
                _restoreWorldState();
                _changeState(EditorState::Edit);
                LOG_INFO("Game stopped");
            });
        ImGui::PopStyleColor(4);
        ImGui::PopID();

        // Step button
        ImGui::SameLine();
        ImGui::PushID("StepPrimary");

        // Draw Step button
        drawIconButton(EditorIcons::Step, canStep, mainBtnSize, "Step (Alt+P)",
            m_editorState == EditorState::Step, EditorStyle::Accent,
            [this]() {
                _changeState(EditorState::Step);
                m_stepRequested = true;
                LOG_INFO("Stepping 1 physics frame");
            });
        ImGui::PopID();

        // State pill: aligned to right side before time scale controls
        float pillX = rowStartX + contentWidth - reservedRight - 72.0f;
        if (pillX > ImGui::GetCursorPosX() + style.ItemSpacing.x) { // Enough space to right-align
            ImGui::SameLine();
            ImGui::SetCursorPosX(pillX);
        } else {
            ImGui::SameLine();
        }

        // Draw state pill based on current editor state
        if (m_editorState == EditorState::Play) { // Green for Play
            drawStatePill("PLAY", EditorStyle::SuccessButton);
        } else if (m_editorState == EditorState::Paused) { // Yellow for Paused
            drawStatePill("PAUSED", EditorStyle::WarningButton);
        } else if (m_editorState == EditorState::Step) { // Accent for Step
            drawStatePill("STEP", EditorStyle::Accent);
        }

        // Time scale presets and slider on far right
        if (reservedRight > 0.0f) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowStartX + contentWidth - reservedRight);

            // Time scale presets
            ImGui::PushFont(m_mainFont);
            const float presets[] = { 0.25f, 0.5f, 1.0f, 2.0f };
            const float presetButtonWidth = 57.0f;
            for (float preset : presets) {
                bool active = std::fabs(m_userTimeScale - preset) < 0.01f;
                // Highlight active preset for quick feedback.
                if (active) {
                    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Accent);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::AccentHover);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::AccentActive);
                    ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
                }

                // Preset button
                char label[12];
                snprintf(label, sizeof(label), "%.2fx", preset);
                if (ImGui::Button(label, ImVec2(presetButtonWidth, 0))) {
                    m_userTimeScale = preset;
                    if (m_editorState == EditorState::Play) {
                        TimeSystem::Instance().SetTimeScale(m_userTimeScale);
                    }
                }

                if (active) ImGui::PopStyleColor(4);
                ImGui::SameLine();
            }

            // Custom time scale slider, kept compact.
            ImGui::SetNextItemWidth(110.0f);
            float sliderScale = m_userTimeScale;
            if (ImGui::SliderFloat("##TimeScale", &sliderScale, 0.0f, 2.0f, "x%.2f")) {
                m_userTimeScale = std::max(0.0f, sliderScale);
                if (m_editorState == EditorState::Play) {
                    TimeSystem::Instance().SetTimeScale(m_userTimeScale);
                }
            }

            ImGui::PopFont();
        }
    }

	// If the user tries to Play without a saved scene path, we need to prompt them to save first
    if (m_showSaveScenePrompt) {
        ImGui::OpenPopup("Save Scene?##PlaySavePrompt");
        m_showSaveScenePrompt = false;
    }

	// Center the modal over the main viewport to ensure it appears prominently
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport) {
        ImGui::SetNextWindowViewport(viewport->ID);
		// Calculate the center of the viewport for modal positioning
        const ImVec2 center(
            viewport->Pos.x + viewport->Size.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f
        );
		// Set the next window position
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

	// Modal popup to prompt user to save the scene before playing, if there is no existing save path
    if (ImGui::BeginPopupModal("Save Scene?##PlaySavePrompt", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		// Wrap the message text at 20% of the viewport width
        const float viewportWidth = viewport ? viewport->WorkSize.x : 0.0f;
        const float wrapWidth = (viewportWidth > 0.0f) ? (viewportWidth * 0.2f) : 0.0f;

		// Show a warning message about the missing save path, with text wrapping for readability
        if (wrapWidth > 0.0f) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapWidth);
        }
        ImGui::TextDisabled("This scene has no save location yet. Play needs a saved scene so tile collisions (if any) can load.");
        ImGui::Dummy(ImVec2(0.0f, 0.2f));
        ImGui::TextDisabled("You can still play without saving.");
        
        if (wrapWidth > 0.0f) {
            ImGui::PopTextWrapPos();
        }
        ImGui::Separator();

		// Styled "Yes" option for saving and playing, with a success color to indicate the recommended action
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SuccessButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SuccessButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SuccessButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);

        // Save and play
        if (ImGui::Button("Yes", ImVec2(90, 0))) {
            if (m_saveSceneCallback) {
                m_saveSceneCallback();
            }
            // If there's no scene path, we have to do a blocking save dialog before we can start play mode
            if (_hasScenePath()) {
                // Which can cause a huge delta time on the first frame
                // To mitigate this, we set a flag to zero out the time scale on the next frame after starting play mode, 
                // and restore it immediately after.
                m_zeroTimeOnNextPlay = true;
                _saveWorldState();
                _changeState(EditorState::Play);
                ImGui::CloseCurrentPopup();
            }
        }

		// Styled "No" option for playing without saving, with a warning color to indicate potential data loss
        ImGui::PopStyleColor(4);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);

        // Play without saving
        if (ImGui::Button("No", ImVec2(90, 0))) {
            _saveWorldState();
            _changeState(EditorState::Play);
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(4);
        ImGui::EndPopup();
    }

    // If we suppressed the first Play frame time scale, restore it on the next frame
    if (m_restoreTimeScaleFrame >= 0 &&
        m_editorState == EditorState::Play &&
        TimeSystem::Instance().GetFrameCount() >= m_restoreTimeScaleFrame) {
        TimeSystem::Instance().SetTimeScale(m_userTimeScale);
        m_restoreTimeScaleFrame = -1;
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::End();
}

// Save the current world into a JSON snapshot that preserves entity IDs.
// Instead of serializing to flat arrays, we save each entity's state by ID.
// This allows us to restore in-place without destroying/recreating entities.
void Playback::_saveWorldState() {
    if (!HasValidWorld()) return;

    LOG_INFO("Saving world state.");
    m_savedWorldPtr = m_world;

    nlohmann::json worldJson = nlohmann::json::object();
    nlohmann::json entitiesMap = nlohmann::json::object(); // Changed from array to object
    
    size_t entityCount = 0;

    // Save all entities (except editor camera) with their IDs as keys
    m_world->Each([&](ECS::Entity entity) {
        // Skip editor camera
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
            return;
        }

        // Serialize entity with all its components
        auto entityJson = Serialization::EntitySerializer::SerializeEntity(*m_world, entity);
        
        // Store by entity ID (as string key for JSON)
        std::string entityKey = std::to_string(entity.Index);
        entitiesMap[entityKey] = entityJson;
        ++entityCount;
    });

    // Save hierarchy relationships separately
    nlohmann::json hierarchyArray = nlohmann::json::array();
    m_world->Each([&](ECS::Entity entity) {
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
            return;
        }
        
        ECS::Entity parent = m_world->ParentOf(entity);
        if (!parent.IsNull() && m_world->IsAlive(parent)) {
            // Skip if parent is editor camera
            if (Editor::ECSUtils::HasComponent(m_world, parent, "CameraEditor3D")) {
                return;
            }
            
            nlohmann::json hierarchyEntry;
            hierarchyEntry["child"] = entity.Index;
            hierarchyEntry["parent"] = parent.Index;
            hierarchyArray.push_back(hierarchyEntry);
        }
    });

    worldJson["entities"] = entitiesMap;
    worldJson["hierarchy"] = hierarchyArray;
    m_savedWorldState = worldJson;
    
    LOG_INFO("Saved " << entityCount << " entities with hierarchy preserved");
}

void Playback::_restoreWorldState() {
    // Restore the world from the previously saved snapshot.
    // This version preserves entity IDs by restoring component values in-place
    // instead of destroying and recreating entities.
    if (!HasValidWorld() || m_savedWorldState.is_null()) {
        if (!m_suppressRestoreWarning) {
            LOG_WARNING("No saved state to restore");
        }
        m_suppressRestoreWarning = false;
        return;
    }
    m_suppressRestoreWarning = false;
    if (m_savedWorldPtr && m_savedWorldPtr != m_world) {
        LOG_WARNING("Saved state world does not match current world; skipping restore.");
        return;
    }

    // Check for new format (object with entities map)
    if (!m_savedWorldState.is_object() || !m_savedWorldState.contains("entities")) {
        LOG_ERROR("Saved world state has invalid format.");
        return;
    }

    LOG_INFO("Restoring world state.");

    const auto& entitiesMap = m_savedWorldState["entities"];
    
    // Track which entities existed in the snapshot
    std::unordered_set<uint32_t> snapshotEntityIds;
    for (auto it = entitiesMap.begin(); it != entitiesMap.end(); ++it) {
        uint32_t entityId = std::stoul(it.key());
        snapshotEntityIds.insert(entityId);
    }

    // First pass: Destroy any entities that were created during play mode
    // (entities that exist now but weren't in the snapshot)
    std::vector<ECS::Entity> entitiesToDestroy;
    m_world->Each([&](ECS::Entity entity) {
        // Skip editor camera
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
            return;
        }
        
        // If this entity wasn't in the snapshot, it was created during play - destroy it
        if (snapshotEntityIds.find(entity.Index) == snapshotEntityIds.end()) {
            entitiesToDestroy.push_back(entity);
        }
    });

    for (const auto& entity : entitiesToDestroy) {
        m_world->Destroy(entity);
    }

    // Second pass: Detach all entities from hierarchy (will restore later)
    m_world->Each([&](ECS::Entity entity) {
        if (Editor::ECSUtils::HasComponent(m_world, entity, "CameraEditor3D")) {
            return;
        }
        m_world->Detach(entity);
    });

    size_t restoredCount = 0;
    size_t recreatedCount = 0;

    // Third pass: Restore or recreate entities from snapshot
    for (auto it = entitiesMap.begin(); it != entitiesMap.end(); ++it) {
        uint32_t entityId = std::stoul(it.key());
        const auto& entityJson = it.value();
        
        ECS::Entity entity = m_world->Resolve(entityId);
        
        if (m_world->IsAlive(entity)) {
            // Entity still exists - restore its state in-place
            // This preserves the entity ID and prevents reference breakage
            _restoreEntityState(entity, entityJson);
            ++restoredCount;
        }
        else {
            // Entity was destroyed during play - recreate it with the same ID
            // This can happen if an entity was explicitly destroyed by game logic
            entity = _recreateEntityWithId(entityId, entityJson);
            if (m_world->IsAlive(entity)) {
                ++recreatedCount;
            }
        }
    }

      // Fourth pass: Restore hierarchy relationships
      if (m_savedWorldState.contains("hierarchy") && m_savedWorldState["hierarchy"].is_array()) {
          const auto& hierarchyArray = m_savedWorldState["hierarchy"];
          for (const auto& hierarchyEntry : hierarchyArray) {
            if (!hierarchyEntry.contains("child") || !hierarchyEntry.contains("parent")) {
                continue;
            }
            
            uint32_t childId = hierarchyEntry["child"].get<uint32_t>();
            uint32_t parentId = hierarchyEntry["parent"].get<uint32_t>();
            
            ECS::Entity child = m_world->Resolve(childId);
            ECS::Entity parent = m_world->Resolve(parentId);
            
            if (m_world->IsAlive(child) && m_world->IsAlive(parent)) {
                m_world->Attach(child, parent);
              }
          }
      }

      // Rebuild prefab instance tracking so editor links remain valid after restore.
      if (auto* prefabManager = m_world->GetPrefabManager()) {
          prefabManager->ClearInstanceTracking();
          size_t tracked = 0;
          size_t skipped = 0;
          m_world->Each<ECS::Components::PrefabInstanceMetadata>(
              [&](ECS::Entity entity, ECS::Components::PrefabInstanceMetadata& meta) {
                  if (!prefabManager->IsRegistered(meta.PrefabHash)) {
                      ++skipped;
                      return;
                  }
                  prefabManager->TrackInstance(entity, meta.PrefabHash);
                  ++tracked;
              }
          );
          if (skipped > 0) {
              LOG_WARNING("Prefab tracking rebuild skipped " << skipped << " instance(s) with unregistered prefab hashes.");
          } else {
              LOG_INFO("Prefab tracking rebuild tracked " << tracked << " instance(s).");
          }
      }

      LOG_INFO("Restored " << restoredCount << " entities, recreated " << recreatedCount << " entities with hierarchy.");
  }

// Internal state change handler that manages time scale and callbacks
void Playback::_changeState(EditorState newState) {
    if (m_editorState == newState) return;

    EditorState oldState = m_editorState;
    m_editorState = newState;

    // Handle time scale changes based on state
    switch (newState) {
    case EditorState::Edit:
		m_zeroTimeOnNextPlay = false;  // Reset flag when returning to Edit
		m_restoreTimeScaleFrame = -1;  // Ensure time scale is normal in Edit mode
        TimeSystem::Instance().SetTimeScale(1.0);
        break;
    case EditorState::Paused:
		m_restoreTimeScaleFrame = -1;  // Clear any pending time scale restore since we're now paused
        TimeSystem::Instance().SetTimeScale(0.0);
        break;
    case EditorState::Play:
		// If we flagged to zero time on the first frame of Play (due to starting play without a saved scene), 
        // do that now and set up to restore on the next frame
        if (m_zeroTimeOnNextPlay) {
            TimeSystem::Instance().SetTimeScale(0.0);
            m_restoreTimeScaleFrame = TimeSystem::Instance().GetFrameCount() + 1;
            m_zeroTimeOnNextPlay = false;
        }
		// Otherwise, apply the user's desired time scale immediately when entering Play mode
        else {
            TimeSystem::Instance().SetTimeScale(m_userTimeScale);
        }
        break;
    case EditorState::Step:
		m_restoreTimeScaleFrame = -1;  // Clear any pending time scale restore since we're now stepping
        TimeSystem::Instance().SetTimeScale(0.0);  // Don't advance time, single frame only
        break;
    }

    // Notify external listeners about state change
    if (m_onStateChanged) {
        m_onStateChanged(oldState, newState);
    }
}

// Local guard for scene dirtiness checks.
bool Playback::_hasUnsavedChanges() const {
    return m_hasUnsavedChangesProvider ? m_hasUnsavedChangesProvider() : false;
}

// Local guard for scene path existence checks
bool Playback::_hasScenePath() const {
    return m_hasScenePathProvider ? m_hasScenePathProvider() : true;
}

// Entry point that defers to the unsaved-changes guard if enabled
bool Playback::_startPlayFromEdit() {
    // If the scene has never been saved, prompt Save As... before playing
    if (!_hasScenePath()) {
        m_showSaveScenePrompt = true;
        return false;
    }
	// If the scene has unsaved changes, auto-save before playing
    else if (_hasUnsavedChanges()) {
        if (m_saveSceneCallback) {
            m_saveSceneCallback();
        }
        // If save fails or is canceled, abort play
        if (_hasUnsavedChanges()) {
            return false;
        }
    }

    _saveWorldState();
    _changeState(EditorState::Play);
    return true;
}

// Register callback for state change events
void Playback::OnStateChanged(std::function<void(EditorState, EditorState)> callback) {
    m_onStateChanged = callback;
}

// Injected from the editor to share its dirty-state tracking.
void Playback::SetUnsavedChangesProvider(std::function<bool()> provider) {
    m_hasUnsavedChangesProvider = std::move(provider);
}

// Injected save callback so playback can offer save-and-play.
void Playback::SetSaveSceneCallback(std::function<void()> callback) {
    m_saveSceneCallback = std::move(callback);
}

// Injected from the editor to check if the current scene has a valid save path
void Playback::SetHasScenePathProvider(std::function<bool()> provider) {
    m_hasScenePathProvider = std::move(provider);
}

// Query: Is the game currently in the Playing state?
bool Playback::IsPlaying() const {
    return m_editorState == EditorState::Play;
}

// Query: Was a single-step frame requested while paused?
bool Playback::IsStepRequested() const {
    return m_stepRequested;
}

// Clear the step request flag
// Clear any outstanding step request flag.
void Playback::ClearStepRequest() {
    m_stepRequested = false;
}

// Get current editor state
EditorState Playback::GetEditorState() const {
    return m_editorState;
}

void Playback::ClearSavedState() {
    // Explicitly drop snapshot when the world changes during Play.
    m_savedWorldState = nullptr;
    m_savedWorldPtr = nullptr;
    m_suppressRestoreWarning = true;
}

// Update the world reference safely when scenes change
// Update the bound world reference used by playback operations.
// Clears saved snapshot since it no longer matches the world.
void Playback::SetWorld(ECS::World* world, bool preserveState) {
    m_world = world;
    if (!world) {
        m_editorState = EditorState::Edit;
        m_stepRequested = false;
        ClearSavedState();
        return;
    }

    // When preserving state (e.g. during scene transitions in Play),
    // keep playback state but drop any snapshot tied to another world.
    if (preserveState) {
        if (m_savedWorldPtr && m_savedWorldPtr != world) {
            ClearSavedState();
        }
        return;
    }

    m_editorState = EditorState::Edit;
    m_stepRequested = false;
    // Reset time scale on world swap to avoid stale values.
    m_userTimeScale = 1.0f;
    ClearSavedState();
}

// Helper: Restore an entity's state in-place from a JSON snapshot
void Playback::_restoreEntityState(ECS::Entity entity, const nlohmann::json& entityJson) {
    if (!entityJson.contains("Components") || !entityJson["Components"].is_array()) {
        return;
    }

    // Helper to normalize type names by stripping ECS::Components:: prefix
    auto normalizeTypeName = [](const std::string& typeName) {
        constexpr const char* kPrefix = "ECS::Components::";
        if (typeName.rfind(kPrefix, 0) == 0) {
            return typeName.substr(std::strlen(kPrefix));
        }
        return typeName;
    };

    const auto& componentsArray = entityJson["Components"];
    
    // Build a set of ComponentTypeIds that should exist after restore
    std::unordered_set<ECS::ComponentTypeId> snapshotComponentIds;
    auto& registry = Serialization::EntitySerializer::Registry();
    
    // Map component names from snapshot to ComponentTypeIds
    for (const auto& comp : componentsArray) {
        if (comp.contains("TypeName")) {
            std::string typeName = comp["TypeName"].get<std::string>(); // Original type name from snapshot
            std::string normalizedTypeName = normalizeTypeName(typeName); // Normalized name without prefix
            uint32_t hash = Editor::ECSUtils::FNV1aHash(normalizedTypeName.c_str()); // Compute hash
            ECS::ComponentTypeId id = ECS::ComponentRegistry::GetComponentIdFromHash(hash); // Lookup ID by hash
            if (id != ECS::NULL_COMPONENT_ID) {
                snapshotComponentIds.insert(id); // Track that this component should exist
            }
        }
    }
    
    // Get entity's current archetype to see what components it has
    const auto* location = m_world->LocationOf(entity);
    if (location && location->ArchetypePtr) {
        const auto& currentComponents = location->ArchetypePtr->GetComponents();
        
        // Check each component on the entity to see if it should be removed
        std::vector<ECS::ComponentTypeId> componentsToRemove;
        for (const auto& compInfo : currentComponents) {
            // Skip if this component is in the snapshot (we want to keep it)
            if (snapshotComponentIds.find(compInfo.Id) != snapshotComponentIds.end()) {
                continue;
            }
            
            // Note: Hierarchy relationships (Parent component) are handled separately
            // in the restore process, so we don't need to explicitly skip them here
            
            // This component exists on entity but not in snapshot - mark for removal
            componentsToRemove.push_back(compInfo.Id);
        }
        
        // Remove components that exist on entity but not in snapshot
        for (ECS::ComponentTypeId id : componentsToRemove) {
            m_world->RemoveById(entity, id);
        }
    }
    
    // Restore each component from the snapshot using the EntitySerializer registry
    for (const auto& componentJson : componentsArray) {
        if (!componentJson.contains("TypeName") || !componentJson.contains("Data")) {
            continue;
        }
        
        std::string typeName = componentJson["TypeName"];
        std::string normalizedTypeName = normalizeTypeName(typeName);
        const auto& componentData = componentJson["Data"];
        
        // First, try to deserialize using the C++ EntitySerializer registry
        bool foundInCppRegistry = false;
        for (const auto& [typeHash, info] : registry) {
            if (info.Name == typeName || info.Name == normalizedTypeName) {
                try {
                    // The deserializer will use Set or Add as appropriate
                    info.Deserialize(*m_world, entity, componentData);
                } 
                catch (const std::exception& ex) {
                    LOG_ERROR("Failed to restore component " << typeName << ": " << ex.what());
                }
                foundInCppRegistry = true;
                break;
            }
        }
        
        // If not found in C++ registry, try managed components via interop
        if (!foundInCppRegistry) {
            // Look up the component in the native ComponentRegistry
            auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
            for (ECS::ComponentTypeId id : allIds) {
                const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
                if (nativeMeta.TypeHash == 0 || !nativeMeta.IsManaged) continue;
                
                std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
                if (nativeName == typeName || nativeName == normalizedTypeName) {
                    // Found the managed component by name
                    // Check if the component exists on the entity
                    if (!m_world->HasById(entity, id)) {
                        // Component doesn't exist, add it with zero-initialized data
                        std::vector<uint8_t> buffer(nativeMeta.Size, 0);
                        m_world->AddComponentById(entity, id, buffer.data(), nativeMeta.Size);
                    }
                    
                    // Deserialize the JSON into the component via interop
                    std::string jsonStr = componentData.dump();
                    try {
                        WorldInterop_DeserializeComponentFromJson(
                            m_world,
                            ECS::EntityUtils::Pack(entity),
                            nativeMeta.TypeHash,
                            jsonStr.c_str()
                        );
                        LOG_DEBUG("Restored managed component " << typeName << " on entity " << entity.Index);
                    }
                    catch (const std::exception& ex) {
                        LOG_ERROR("Failed to deserialize managed component " << typeName << ": " << ex.what());
                    }
                    break;
                }
            }
        }
    }
}

// Helper: Recreate an entity with a specific ID from a JSON snapshot
ECS::Entity Playback::_recreateEntityWithId(uint32_t targetId, const nlohmann::json& entityJson) {
    // This is a fallback for when an entity was destroyed during play
    // Use CreateWithId to preserve the exact entity ID from the snapshot
    
    ECS::Entity newEntity = m_world->CreateWithId(targetId);
    
    // Deserialize components into the newly created entity
    if (entityJson.contains("Components") && entityJson["Components"].is_array()) {
        auto normalizeTypeName = [](const std::string& typeName) {
            constexpr const char* kPrefix = "ECS::Components::";
            if (typeName.rfind(kPrefix, 0) == 0) {
                return typeName.substr(std::strlen(kPrefix));
            }
            return typeName;
        };

        auto& registry = Serialization::EntitySerializer::Registry();
        for (const auto& comp : entityJson["Components"]) {
            if (!comp.contains("TypeName") || !comp.contains("Data")) {
                continue;
            }
            
            std::string typeName = comp["TypeName"].get<std::string>();
            std::string normalizedTypeName = normalizeTypeName(typeName);
            const auto& componentData = comp["Data"];
            
            // First, try to deserialize using the C++ EntitySerializer registry
            bool foundInCppRegistry = false;
        for (const auto& [typeHash, info] : registry) {
            if (info.Name == typeName || info.Name == normalizedTypeName) {
                try {
                    info.Deserialize(*m_world, newEntity, componentData);
                    } 
                    catch (const std::exception& ex) {
                        LOG_ERROR("Failed to restore component " << typeName << ": " << ex.what());
                    }
                    foundInCppRegistry = true;
                    break;
                }
            }
            
            // If not found in C++ registry, try managed components via interop
            if (!foundInCppRegistry) {
                // Look up the component in the native ComponentRegistry
                auto allIds = ECS::ComponentRegistry::GetAllComponentIds();
                for (ECS::ComponentTypeId id : allIds) {
                    const auto& nativeMeta = ECS::ComponentRegistry::Meta(id);
                    if (nativeMeta.TypeHash == 0 || !nativeMeta.IsManaged) continue;
                    
                    std::string nativeName = ECS::ComponentRegistry::GetComponentNameFromHash(nativeMeta.TypeHash);
                    if (nativeName == typeName || nativeName == normalizedTypeName) {
                        // Found the managed component by name
                        // Add component with zero-initialized data
                        std::vector<uint8_t> buffer(nativeMeta.Size, 0);
                        m_world->AddComponentById(newEntity, id, buffer.data(), nativeMeta.Size);
                        
                        // Deserialize the JSON into the component via interop
                        std::string jsonStr = componentData.dump();
                        try {
                            WorldInterop_DeserializeComponentFromJson(
                                m_world,
                                ECS::EntityUtils::Pack(newEntity),
                                nativeMeta.TypeHash,
                                jsonStr.c_str()
                            );
                            LOG_DEBUG("Restored managed component " << typeName << " on recreated entity " << newEntity.Index);
                        }
                        catch (const std::exception& ex) {
                            LOG_ERROR("Failed to deserialize managed component " << typeName << ": " << ex.what());
                        }
                        break;
                    }
                }
            }
        }
    }
    
    return newEntity;
}
