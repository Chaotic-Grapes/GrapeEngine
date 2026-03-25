/* Start Header *****************************************************************/
/*!
\file   AssetBrowserPanel.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th March 2026

\brief
Implements the AssetBrowserPanel which renders the asset browser UI.

Provides:
- Breadcrumb navigation and folder traversal
- File listing with selection and info display
- Import replace and delete actions
- Prefab load and edit workflow
- Status bar updates and file-drop handling
- Integration with AssetLibrary and InspectorPanel
*/
/* End Header *******************************************************************/

#include "AssetBrowserPanel.h"
#include "core/Logger.h"
#include "core/ProjectPaths.h"
#include "serialization/EntitySerializer.h"
#include "ecs/Entity.h"
#include "ecs/PrefabManager.h"
#include "EditorECSUtils.h"
#include "InspectorPanel.h"
#include "services/Input.h"
#include "services/ResourceManager.h"
#include "ScriptTemplates.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include "EditorStyle.h"
#include "EditorIcons.h"

// Only compile this block on Windows
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN  // Strip rarely-used APIs from windows.h to speed up compilation
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX             // Prevent windows.h from defining min/max macros that clash with std::min/max
#define NOMINMAX
#endif
#include <Windows.h>         // Core Win32 API
#include <shellapi.h>        // Shell functions (ShellExecute, drag-drop, etc.)
#include <shlobj.h>          // Shell object interfaces (folder dialogs, known paths, etc.)
#ifdef ERROR                 
#undef ERROR                 // windows.h defines ERROR as 0, which collides with any enum/symbol named ERROR
#endif
#ifdef CreateDirectory
#undef CreateDirectory       // windows.h macro-ifies CreateDirectory, stomping any class method with that name
#endif
#endif

namespace {
    // Lowercase helper for case-insensitive compares
    std::string ToLowerCopy(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    // Visual metadata for one asset type in list/grid views
    // Keeps icon + badge styling centralized so both views stay consistent
    struct AssetBadgeInfo {
        ImVec4 IconColor;      // Icon tint for this asset type
        const char* BadgeText; // Optional short badge text (SCN CS PNG etc)
        const char* IconGlyph; // Icon glyph from symbols font
    };

    // Quick extension check for image previews
    bool IsImageExtension(const std::string& extLower) {
        return extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" ||
            extLower == ".tga" || extLower == ".bmp";
    }

    // Hidden filesystem entries are skipped in browser listings to reduce noise.
    bool IsHiddenEntry(const std::filesystem::directory_entry& entry) {
        // Use leaf name so dot-prefix checks are independent of full path.
        const std::string name = entry.path().filename().string();
        // Dot-prefixed entries are treated as hidden.
        if (!name.empty() && name.front() == '.') {
            return true;
        }

#ifdef _WIN32
        // Query native file attributes on Windows.
        const DWORD attrs = GetFileAttributesA(entry.path().string().c_str());
        // If attribute query succeeds, treat hidden bit as authoritative.
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
        }
#endif
        // Default to visible when no hidden condition matches.
        return false;
    }

    // Captures interaction + bounds for a single grid tile button
    struct GridTileInputState {
        ImVec2 Min;               // Tile min screen position
        ImVec2 Max;               // Tile max screen position
        bool Hovered = false;     // Mouse currently hovering tile
        bool LeftClick = false;   // Left click happened on tile this frame
        bool RightClick = false;  // Right click happened on tile this frame
        bool DoubleClick = false; // Left double click happened on tile this frame
    };

    // Build tile input state + draw tile chrome
    GridTileInputState DrawGridTileButtonAndBackground(const bool isSelected, const size_t index,
        const int columns, const float tileSpacing, const float tileWidth, const float tileHeight,
        const float tileRounding)
    {
        // For all items after first column, continue current row
        // modulo check tells us if this index starts a new row
        if (index > 0 && (static_cast<int>(index) % columns) != 0) {
            // Preserve spacing rhythm between tiles in same row
            ImGui::SameLine(0.0f, tileSpacing);
        }

        GridTileInputState state;
        // Screen-space top-left before issuing invisible button
        // Used later for bg/border/image placement
        state.Min = ImGui::GetCursorScreenPos();

        // No default widget visuals
        // We only need hitbox/click/hover behavior from ImGui
        ImGui::InvisibleButton("##AssetTile", ImVec2(tileWidth, tileHeight));

        // Screen-space bottom-right after the button
        state.Max = ImGui::GetItemRectMax();

        // Hover state for this tile this frame
        state.Hovered = ImGui::IsItemHovered();

        // Left click pressed on this tile this frame
        state.LeftClick = ImGui::IsItemClicked(ImGuiMouseButton_Left);

        // Right click only valid while hovering this tile
        state.RightClick = state.Hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        // Double-click uses hover gate too, avoids accidental neighbor triggers
        state.DoubleClick = state.Hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        // Default tile background
        ImVec4 bg = EditorStyle::FrameBg;
        if (isSelected) {
            // Selected tile uses accent tint
            bg = EditorStyle::Scale(EditorStyle::Accent, 0.45f);
        }
        else if (state.Hovered) {
            // Hover tile uses lighter frame tint
            bg = EditorStyle::Scale(EditorStyle::FrameBgHover, 0.75f);
        }

        // Draw list for this window
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Fill tile body
        drawList->AddRectFilled(state.Min, state.Max, ImGui::ColorConvertFloat4ToU32(bg), tileRounding);

        // Border uses accent when selected, muted border when idle/hovered
        ImVec4 border = isSelected ? EditorStyle::Accent : EditorStyle::Scale(EditorStyle::Border, 0.75f);

        // Stronger alpha for selected tile so active state is obvious
        border.w = isSelected ? 1.0f : 0.6f;

        // Stroke tile border
        drawList->AddRect(state.Min, state.Max, ImGui::ColorConvertFloat4ToU32(border), tileRounding);

        // Return both geometry + interaction flags to caller
        return state;
    }

    // Trim long names so labels stay inside tile bounds
    // Input is by value on purpose so we can mutate and return it
    std::string TruncateGridLabel(std::string text, ImFont* nameFont, const float nameFontSize,
        const float maxWidth)
    {
        // Empty input means nothing to do
        if (text.empty()) {
            return text;
        }

        // Keep shrinking until measured width fits maxWidth
        while (!text.empty()) {
            // Use provided font for exact preview-width match when available
            const ImVec2 size = nameFont
                ? nameFont->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, text.c_str())
                : ImGui::CalcTextSize(text.c_str()); // Fallback to current ImGui font

            // Stop once text already fits
            if (size.x <= maxWidth) {
                break;
            }

            // For tiny strings just collapse directly to ellipsis
            if (text.size() <= 3) {
                text = "...";
                break;
            }

            // Remove one char and keep trailing ellipsis marker
            text.pop_back();
            if (text.size() > 3) {
                text.replace(text.size() - 3, 3, "...");
            }
        }

        return text;
    }

    // Measure icon footprint used by layout
    // Image assets reserve fixed preview box instead of glyph-width box
    ImVec2 MeasureGridIconSize(ImFont* symbolsFont, const AssetBadgeInfo& badgeInfo,
        const float iconFontSize, const bool isImageAsset)
    {
        // Default measurement is the glyph itself in symbols font
        ImVec2 iconSize = symbolsFont
            ? symbolsFont->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, badgeInfo.IconGlyph)
            : ImGui::CalcTextSize(badgeInfo.IconGlyph);

        // Image preview tiles use a larger square reservation for texture preview
        if (isImageAsset) {
            iconSize = ImVec2(iconFontSize + 6.0f, iconFontSize + 6.0f);
        }

        return iconSize;
    }

    // Draw icon/preview + type badge for one tile
    // Handles both texture preview path and glyph fallback path
    void DrawGridEntryIconAndBadge(const std::string& entryPath, const AssetBadgeInfo& badgeInfo,
        const bool isImageAsset, ImFont* symbolsFont, const float iconFontSize, const float textPaddingX,
        const ImVec2& tileMin, const ImVec2& tileMax, const float tileWidth, const ImVec2& iconSize,
        const float contentTop)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();                             // Draw on current window list
        const ImVec2 iconPos(tileMin.x + (tileWidth - iconSize.x) * 0.5f, contentTop); // Center icon/preview horizontally

        if (isImageAsset) {
            // Try pulling texture from cache so image files show previews
            uint32_t textureId = 0;
            float texW = 0.0f;
            float texH = 0.0f;
            if (auto tex = RM.Get<Texture>(entryPath)) {
                textureId = tex->ID();                    // GL texture id
                texW = static_cast<float>(tex->Width());  // Source width
                texH = static_cast<float>(tex->Height()); // Source height
            }

            if (textureId != 0) {
                // Fit preview into reserved icon rectangle while preserving aspect ratio
                const float maxPreviewW = iconSize.x;
                const float maxPreviewH = iconSize.y;
                float drawW = maxPreviewW;
                float drawH = maxPreviewH;
                if (texW > 0.0f && texH > 0.0f) {
                    const float scale = std::min(maxPreviewW / texW, maxPreviewH / texH); // Uniform scale
                    drawW = std::max(1.0f, texW * scale);                                 // Clamp to visible minimum
                    drawH = std::max(1.0f, texH * scale);
                }

                // Center scaled preview inside reserved preview box
                const ImVec2 previewMin(tileMin.x + (tileWidth - drawW) * 0.5f, contentTop + (maxPreviewH - drawH) * 0.5f);
                const ImVec2 previewMax(previewMin.x + drawW, previewMin.y + drawH);

                // Draw textured quad (flip UVs for OpenGL texture orientation)
                drawList->AddImage((ImTextureID)(uintptr_t)textureId, previewMin, previewMax, ImVec2(0, 1), ImVec2(1, 0));

                // Draw subtle frame around preview so light images are still readable
                drawList->AddRect(previewMin, previewMax, 
                    ImGui::ColorConvertFloat4ToU32(EditorStyle::Scale(EditorStyle::Border, 0.8f)), 3.0f);
            }
            else {
                // Fallback icon when texture is missing/not loaded
                drawList->AddText(symbolsFont, iconFontSize, iconPos,
                    ImGui::ColorConvertFloat4ToU32(badgeInfo.IconColor), badgeInfo.IconGlyph);
            }
        }
        else {
            // Non-image assets always use glyph icon path
            drawList->AddText(symbolsFont, iconFontSize, iconPos,
                ImGui::ColorConvertFloat4ToU32(badgeInfo.IconColor), badgeInfo.IconGlyph);
        }

        // Optional extension badge in top-right
        if (badgeInfo.BadgeText && badgeInfo.BadgeText[0] != '\0') {
            const ImVec2 badgeSize = ImGui::CalcTextSize(badgeInfo.BadgeText);                  // Badge text dimensions
            drawList->AddText(ImVec2(tileMax.x - badgeSize.x - textPaddingX, tileMin.y + 6.0f), // Right align with padding
                ImGui::ColorConvertFloat4ToU32(EditorStyle::Muted), badgeInfo.BadgeText);
        }
    }

    // Resolve badge style for a directory/extension combination
    // Keeps mapping logic centralized so list + grid stay identical
    AssetBadgeInfo GetAssetBadgeInfo(const bool isDirectory, const std::string& extLower) {
        // Directories always use folder icon and warning tint
        if (isDirectory) {
            return { EditorStyle::WarningText, "", EditorIcons::Folder };
        }

        // Scene assets
        if (extLower == ".scn" || extLower == ".scene") {
            return { EditorStyle::LogDebug, "SCN", EditorIcons::Scene };
        }

        // Script assets
        if (extLower == ".cs") {
            return { EditorStyle::SuccessText, "CS", EditorIcons::Script };
        }

        // Prefab assets
        if (extLower == ".prefab") {
            return { EditorStyle::Accent, "PREFAB", EditorIcons::Prefab };
        }

        // Image assets
        if (extLower == ".png") {
            return { EditorStyle::AccentHover, "PNG", EditorIcons::Texture };
        }
        if (extLower == ".jpg" || extLower == ".jpeg") {
            return { EditorStyle::AccentHover, "JPG", EditorIcons::Texture };
        }
        if (extLower == ".tga") {
            return { EditorStyle::AccentHover, "TGA", EditorIcons::Texture };
        }
        if (extLower == ".bmp") {
            return { EditorStyle::AccentHover, "BMP", EditorIcons::Texture };
        }

        // Audio assets
        if (extLower == ".wav") {
            return { EditorStyle::LogCritical, "WAV", EditorIcons::Audio };
        }
        if (extLower == ".mp3") {
            return { EditorStyle::LogCritical, "MP3", EditorIcons::Audio };
        }
        if (extLower == ".ogg") {
            return { EditorStyle::LogCritical, "OGG", EditorIcons::Audio };
        }

        // Shader assets
        if (extLower == ".vert") {
            return { EditorStyle::Muted, "VERT", EditorIcons::Shader };
        }
        if (extLower == ".frag") {
            return { EditorStyle::Muted, "FRAG", EditorIcons::Shader };
        }

        // Font assets
        if (extLower == ".ttf") {
            return { EditorStyle::Muted, "TTF", EditorIcons::Font };
        }
        if (extLower == ".otf") {
            return { EditorStyle::Muted, "OTF", EditorIcons::Font };
        }

        // Generic fallback for unsupported/unknown file types
        return { EditorStyle::Muted, "", EditorIcons::File };
    }
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

// Initialize the Asset Browser with fonts and a world reference
// Also hook file-drop messages so imports work from the OS
void AssetBrowserPanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, ECS::World* world) {
    // Cache font handles used across all child UI
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    // Cache world pointer for prefab load/edit actions
    m_world = world;

    // Set initial path to game project root folder
    m_currentPath = Engine::ProjectPaths::GetProjectRoot();

    // Initialize helper modules
    m_assetLibrary.Initialize(mainFont, boldFont, symbolsFont);

    // OS file drops are consumed via Input::ConsumeDroppedFiles() during Render()
}

// Update the world reference when scene changes
void AssetBrowserPanel::SetWorld(ECS::World* world) {
    // Keep panel + asset library in sync with active world
    m_world = world;
    m_assetLibrary.SetWorld(world);
}

// Connect the InspectorPanel so prefab actions in Asset Browser
// are inspected in the unified prefab editor UI
void AssetBrowserPanel::SetInspector(InspectorPanel* inspector) {
    m_inspector = inspector;
    // Propagate to AssetLibrary for double-click open behavior
    m_assetLibrary.SetInspector(inspector);
}

// Update editor settings reference and apply view mode from config
void AssetBrowserPanel::SetEditorSettings(EditorSettings* settings) {
    m_editorSettings = settings;
    if (m_editorSettings) {
        m_viewMode = (m_editorSettings->AssetBrowserViewMode == 0) ? ViewMode::List : ViewMode::Grid;
    }
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

// Render the Asset Browser window with breadcrumbs, actions and panels
// Uses child regions to split file list and file info side-by-side
void AssetBrowserPanel::Render() {
    // Consume OS drop queue from Input directly
    // This is module-safe even when message-bus subscriptions are split across engine/editor binaries
    const std::vector<std::string> droppedFiles = Input::ConsumeDroppedFiles();

    // Handle each dropped file
    for (const auto& droppedPath : droppedFiles) {
        // Reuse AssetLibrary importer path handling + status wiring
        m_assetLibrary._handleFileDrop(droppedPath, m_currentPath, m_selectedAsset, m_statusMessage, m_statusTimer);
    }

    // Use panel main font for all standard text in this window
    ImGui::PushFont(m_mainFont);
    // Window flags: NoScrollbar removes the vertical scrollbar; child regions handle scrolling
    ImGui::Begin("Asset Browser", nullptr, ImGuiWindowFlags_NoScrollbar);

    _renderNavigationBar();
    _renderActionButtons();
    _renderContentArea();

    // Dialog open state is controlled by context and prefab popup actions
    _renderCreateDialog();

    // Handle keyboard shortcuts ONLY when Asset Browser window is focused
    // This prevents conflicts with other panels (e.g. Hierarchy) that also use DELETE key
    if (!m_selectedAssets.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        // DELETE key: delete selected assets
        if (Input::IsKeyDown(KEY_DELETE)) {
            // Immediate delete shortcut for selected assets
            _deleteSelectedAssets();
        }

        // Ctrl+C: copy
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
            // Copy current selection to internal clipboard
            _copySelectedAssets();
        }

        // Ctrl+X: cut (copy + mark for deletion after paste)
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
            // Cut = copy first then mark clipboard as move mode
            _copySelectedAssets();
            m_clipboardIsCut = true;
        }
    }

    // Ctrl+V: paste (works even with no selection)
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        // Paste works even when nothing is selected
        _pasteAssets();
    }

    // F2: rename (single selection only)
    if (m_selectedAssets.size() == 1 && ImGui::IsKeyPressed(ImGuiKey_F2)) {
        // Rename only valid for single selection
        _startRename();
    }

    // Click on empty space in parent Asset Browser window to clear everything
    _selectEmptySpace();

    // Only render status bar if we have an active message
    if (m_statusTimer > 0.0f) {
        // Render transient status feedback while timer is alive
        _renderStatusBar();
    }

    ImGui::End();
    ImGui::PopFont();

    // Prefab editing is absorbed by InspectorPanel; no separate window to render
}

// -------------------------------------------------------------------------
// UI Sections
// -------------------------------------------------------------------------

// Render the breadcrumb navigation bar at the top
void AssetBrowserPanel::_renderNavigationBar() {
    // Display clickable breadcrumb navigation with drop target support
    const std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
    std::filesystem::path pathObj(m_currentPath);
    std::vector<std::filesystem::path> pathParts;

    // Use project folder name as breadcrumb root label
    std::string projectName = projectRoot.filename().string();
    if (projectName.empty()) {
        projectName = "Project";
    }

    std::error_code ec;
    std::filesystem::path rel = std::filesystem::relative(pathObj, projectRoot, ec);
    // Clamp back to project root when current path escapes or is invalid
    if (ec || rel.empty() || rel.string().rfind("..", 0) == 0) {
        pathObj = projectRoot;
        rel = std::filesystem::path();
    }

    pathParts.emplace_back(projectName);
    for (const auto& part : rel) {
        std::string partStr = part.string();
        // Skip empty + traversal tokens to keep breadcrumb clean
        if (!partStr.empty() && partStr != "." && partStr != ".." && partStr != "/" && partStr != "\\") {
            pathParts.emplace_back(partStr);
        }
    }

    // Display each part as clickable button with separators and drop targets
    std::filesystem::path accumulatedPath = projectRoot;
    for (size_t i = 0; i < pathParts.size(); i++) {
        // Rebuild absolute path represented by each breadcrumb segment
        if (i == 0) {
            accumulatedPath = projectRoot;
        }
        else {
            accumulatedPath /= pathParts[i].string();
        }

        // Add ">" separator between breadcrumb parts
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }

        // Display breadcrumb part
        if (i == pathParts.size() - 1) {
            // Last part (current directory): display as plain text
            ImGui::Text("%s", pathParts[i].string().c_str());

            // Make current directory text a drop target
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                _handleFolderDropTarget(accumulatedPath.string());
            }
        }
        else {
            // Previous parts: display as clickable blue buttons
            ImGui::PushID(static_cast<int>(i));

            ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Accent);
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::AccentHover, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::AccentActive, 0.5f));

            // When clicked, navigate to that folder level
            if (ImGui::SmallButton(pathParts[i].string().c_str())) {
                // Jump to clicked breadcrumb level
                m_currentPath = accumulatedPath.string();
                m_selectedAssets.clear();
                m_selectedAsset.clear();
            }

            ImGui::PopStyleColor(4);

            // Make breadcrumb button a drop target
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
                _handleFolderDropTarget(accumulatedPath.string());
            }

            ImGui::PopID();
        }
    }

    // Add spacing after breadcrumbs
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Separator();
}

// Render the action buttons (import, replace, prefab management)
void AssetBrowserPanel::_renderActionButtons() {
    // Import button (upload icon)
    // Push symbols font so the button renders an icon glyph
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button(EditorIcons::Import)) {
        // Open import flow rooted at current folder
        m_assetLibrary._importAsset(m_currentPath, m_selectedAsset, m_statusMessage, m_statusTimer);
    }
    ImGui::PopFont();

    // Tooltip
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Import new assets into current folder");
    }

    ImGui::SameLine();

    // Replace button (enabled only if a file is selected, not a folder)
    // True if something is selected at all
    const bool hasSelection = !m_selectedAsset.empty();
    bool isFolderSelection = false;

    // If something is selected, check if it's a directory
    // Use error_code so is_directory doesn't throw on invalid paths
    if (hasSelection) {
        std::error_code ec;
        // is_directory may fail on stale path so keep no-throw check
        isFolderSelection = std::filesystem::is_directory(m_selectedAsset, ec) && !ec;
    }

    // Replace is allowed only when a file is selected (not a folder)
    const bool enableReplace = hasSelection && !isFolderSelection;

    // Wrap controls in BeginDisabled/EndDisabled to gray-out and block interaction when not applicable
    if (!enableReplace) ImGui::BeginDisabled();

    // Use symbols font for the replace icon button
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button(EditorIcons::Replace)) {
        // Replace only supports file targets not folders
        m_assetLibrary._replaceTexture(m_selectedAsset, m_statusMessage, m_statusTimer);
    }

    ImGui::PopFont();
    if (!enableReplace) ImGui::EndDisabled();

    // Allow tooltips even when the button is disabled to explain why
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (enableReplace) {
            ImGui::SetTooltip("Replace the selected file with another of the same extension");
        }
        else if (hasSelection && isFolderSelection) {
            ImGui::SetTooltip("Replace is disabled for folders");
        }
        else {
            ImGui::SetTooltip("Replace (disabled)");
        }
    }

    ImGui::SameLine();
    // Render create/prefab management button and popup
    _renderPrefabButton();

    ImGui::SameLine();
    ImGui::PushFont(m_symbolsFont);
    const bool isListView = (m_viewMode == ViewMode::List);
    // Toggle button icon shows destination mode
    const char* viewToggleIcon = isListView ? EditorIcons::Layout4 : EditorIcons::Layout2;
    if (ImGui::Button(viewToggleIcon)) {
        // Flip active mode + persist to editor settings
        m_viewMode = isListView ? ViewMode::Grid : ViewMode::List;
        if (m_editorSettings) {
            m_editorSettings->AssetBrowserViewMode = (m_viewMode == ViewMode::List) ? 0 : 1;
        }
    }
    ImGui::PopFont();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isListView ? "Switch to grid view" : "Switch to list view");
    }
}

// Render the prefab management button and popup
void AssetBrowserPanel::_renderPrefabButton() {
    // Plus button: always enabled now (for Create + Prefab management)
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button(EditorIcons::NewFolder)) {
        ImGui::OpenPopup("CreateAndPrefabs");
    }

    ImGui::PopFont();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Create new assets and manage prefabs");
    }

    // Render the combined create/prefab popup menu
    _renderPrefabPopup();
}

// Render the combined create/prefab popup menu
void AssetBrowserPanel::_renderPrefabPopup() {
    // Check if a prefab is selected for conditional enabling
    const bool isPrefab = !m_selectedAsset.empty() && std::filesystem::path(m_selectedAsset).extension() == ".prefab";

    if (ImGui::BeginPopup("CreateAndPrefabs")) {
        ImGui::PushFont(m_mainFont);

        // Load Prefab: only enabled when prefab is selected
        if (!isPrefab) ImGui::BeginDisabled();
        if (ImGui::MenuItem("Load Prefab")) {
            _loadPrefab();
        }
        if (!isPrefab) ImGui::EndDisabled();

        // Edit Prefab: only enabled when prefab is selected
        if (!isPrefab) ImGui::BeginDisabled();
        if (ImGui::MenuItem("Edit Prefab")) {
            _editPrefab();
        }
        if (!isPrefab) ImGui::EndDisabled();

        ImGui::Separator();

        // Create submenu: uses the same menu items as right-click context menu
        if (ImGui::BeginMenu("Create")) {
            // Use shared helper function
            if (_renderCreateMenuItems()) {
                m_openCreateDialog = true;
            }
            ImGui::EndMenu();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }

    // Asset creation dialog is rendered by the context menu handler to avoid duplicate popups.
}

// Render the main content area (file list and file info panels)
void AssetBrowserPanel::_renderContentArea() {
    // Reserve space for status bar at bottom
    const float windowWidth = ImGui::GetContentRegionAvail().x;
    // Slightly shorten the status bar to give the content area a bit more height
    const float statusBarHeight = 24.0f;

    // Child fills remaining height; negative height reserves space for the fixed status bar
    ImGui::BeginChild("ContentRegion", ImVec2(0, -statusBarHeight), false);

    _renderFileListPanel(windowWidth);
    ImGui::SameLine();
    _renderFileInfoPanel();

    // End content region above status bar
    ImGui::EndChild();
}

// Render the left file/folder list panel
void AssetBrowserPanel::_renderFileListPanel(const float windowWidth) {
    // Left side: File/folder list (65% width)
    // Third arg 'true' draws a frame (border) around the child
    ImGui::BeginChild("FileList", ImVec2(windowWidth * 0.65f, 0), true);

    // Custom folder display with multi-selection support
    ImGui::PushFont(m_mainFont);

    if (!std::filesystem::exists(m_currentPath) || !std::filesystem::is_directory(m_currentPath)) {
        // Current path may become invalid after external file operations
        ImGui::TextColored(EditorStyle::DangerText, "Folder not found");
    }
    else {
        // Shared sorted list used by both list and grid modes
        const std::vector<std::filesystem::directory_entry> entries = _getSortedEntriesForCurrentPath();
        if (m_viewMode == ViewMode::List) {
            _renderFileListEntries(entries);
        }
        else {
            _renderFileGridEntries(entries);
        }
    }

    ImGui::PopFont();

    // Create invisible button covering remaining empty space as drop target
    const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    if (contentAvail.y > 0) {
        // Capture empty area for right-click create + drop target behavior
        ImGui::InvisibleButton("##EmptySpaceDropTarget", contentAvail);

        // Left-click on remaining empty area clears selection
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            _clearSelection();
            _notifySelectionChanged();
        }

        // Right-click on empty space to create new assets
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("AssetContextMenu");
        }

        _handleFolderDropTarget(m_currentPath);
    }

    // Render both context menus
    _renderContextMenu();
    _renderItemContextMenu();

    // Clicking empty space in file list clears EVERYTHING (File/folder list)
    AssetBrowserPanel::_selectEmptySpace();
    ImGui::EndChild();
}

// Enumerate + sort entries (folders first then by name)
std::vector<std::filesystem::directory_entry> AssetBrowserPanel::_getSortedEntriesForCurrentPath() const {
    std::vector<std::filesystem::directory_entry> entries;

    // Gather entries first so we can sort and render in a stable order
    for (const auto& entry : std::filesystem::directory_iterator(m_currentPath)) {
        // Skip hidden files and folders from browser listing.
        if (IsHiddenEntry(entry)) {
            continue;
        }

        // Collect first so render order is deterministic after sorting
        entries.push_back(entry);
    }

    // Keep folder-first then alphabetical ordering for predictable list/grid layout
    std::sort(entries.begin(), entries.end(), [](const std::filesystem::directory_entry& a,
        const std::filesystem::directory_entry& b) 
    {
        const bool aDir = a.is_directory();
        const bool bDir = b.is_directory();

        // Folder rows always before file rows
        if (aDir != bDir) {
            return aDir && !bDir;
        }

        // Case-insensitive name ordering for stable UX
        return ToLowerCopy(a.path().filename().string()) < ToLowerCopy(b.path().filename().string());
    });

    return entries;
}

// Render list view rows
void AssetBrowserPanel::_renderFileListEntries(const std::vector<std::filesystem::directory_entry>& entries) {
    // List view: single row per item (icon, name, optional badge), with multi-select behavior
    for (const auto& entry : entries) {
        const std::string entryPath = entry.path().string();
        const std::string entryName = entry.path().filename().string();
        const bool isSelected = m_selectedAssets.contains(entryPath);
        const bool isDirectory = entry.is_directory();
        const std::string extLower = isDirectory ? std::string() : ToLowerCopy(entry.path().extension().string());
        const AssetBadgeInfo badgeInfo = GetAssetBadgeInfo(isDirectory, extLower);

        // Render type icon (folder/scene/script/etc.) using the symbols font
        ImGui::PushFont(m_symbolsFont);
        ImGui::PushStyleColor(ImGuiCol_Text, badgeInfo.IconColor);
        ImGui::Text(badgeInfo.IconGlyph); // Per-type icon
        ImGui::PopStyleColor();
        ImGui::PopFont();
        const bool iconHovered = ImGui::IsItemHovered();

        ImGui::SameLine();

        // Rename mode replaces the label with an inline text box
        if (m_renamingAsset == entryPath) {
            // Rename input consumes whole row width
            ImGui::PushItemWidth(-1);
            if (m_focusRenameInput) {
                ImGui::SetKeyboardFocusHere();
                m_focusRenameInput = false;
            }

            if (ImGui::InputText("##Rename", m_renameBuffer, sizeof(m_renameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
                // Commit rename + sync selection state
                _commitRename(entry, entryPath);
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_renamingAsset.clear();
            }

            ImGui::PopItemWidth();
        }
        else {
            // Normal row selectable, double-click opens assets/folders
            if (ImGui::Selectable(entryName.c_str(), isSelected,
                ImGuiSelectableFlags_AllowDoubleClick)) {
                // Shared list/grid selection logic
                _applyEntrySelection(entries, entryPath, isSelected, ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left),
                    isDirectory, extLower);
            }

            // Enable drag source/target behavior for this item row
            _handleAssetDragDrop(entryPath);

            const ImVec2 itemMin = ImGui::GetItemRectMin();
            const ImVec2 itemMax = ImGui::GetItemRectMax();
            // Hover if either icon or row text is hovered
            bool rowHovered = ImGui::IsItemHovered() || iconHovered;

            // Draw type badge on the far right (.scn/.cs/etc)
            if (badgeInfo.BadgeText && badgeInfo.BadgeText[0] != '\0') {
                const ImVec2 badgeSize = ImGui::CalcTextSize(badgeInfo.BadgeText);
                const ImVec2 windowPos = ImGui::GetWindowPos();
                const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
                const float badgeX = (windowPos.x + contentMax.x) - badgeSize.x - ImGui::GetStyle().ItemSpacing.x;
                ImGui::SameLine(badgeX - windowPos.x);
                ImGui::TextDisabled("%s", badgeInfo.BadgeText);
                rowHovered = rowHovered || ImGui::IsItemHovered();
            }

            // Full-row hover background improves visual scan in list mode
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
            const ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
            const float rowMinX = windowPos.x + contentMin.x;
            const float rowMaxX = windowPos.x + contentMax.x;

            if (rowHovered) {
                ImVec4 hoverColor = EditorStyle::FrameBgHover;
                hoverColor.w = 0.20f;
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(rowMinX, itemMin.y),
                    ImVec2(rowMaxX, itemMax.y),
                    ImGui::ColorConvertFloat4ToU32(hoverColor));
            }

            // Right-click selects row (if needed) and opens item context menu
            if (rowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                // Select this item if not already selected
                if (!m_selectedAssets.contains(entryPath)) {
                    // Right-click should target the clicked row
                    _setSingleSelection(entryPath, true);
                    _notifySelectionChanged();
                }
                ImGui::OpenPopup("ItemContextMenu");
            }

            // Folder rows accept drops and support drag-hover auto-open
            if (entry.is_directory()) {
                _handleFolderHoverAutoOpen(
                    entryPath,
                    ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                    ImGui::GetDragDropPayload() != nullptr);
                _handleFolderDropTarget(entryPath);
            }
        }
    }
}

// Compute a single tile size for this folder so the grid stays uniform
ImVec2 AssetBrowserPanel::_calculateGridTileSize(const std::vector<std::filesystem::directory_entry>& entries,
    ImFont* nameFont, const float nameFontSize, const float iconFontSize, const float textPaddingX,
    const float tilePaddingY, const float contentGap) const
{
    float maxIconWidth = 0.0f;
    float maxIconHeight = 0.0f;
    float maxNameWidth = 0.0f;
    float maxNameHeight = 0.0f;

    // Pre-pass measurement to keep tile dimensions consistent per directory
    for (const auto& entry : entries) {
        const bool isDirectory = entry.is_directory();

        // Files use extension badges, folders use folder badge
        const std::string extLower = isDirectory ? std::string() : ToLowerCopy(entry.path().extension().string());
        const AssetBadgeInfo badgeInfo = GetAssetBadgeInfo(isDirectory, extLower);
        const std::string entryName = entry.path().filename().string();

        ImVec2 iconSize = m_symbolsFont
            ? m_symbolsFont->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, badgeInfo.IconGlyph)
            : ImGui::CalcTextSize(badgeInfo.IconGlyph);

        if (!isDirectory && IsImageExtension(extLower)) {
            // Reserve square preview footprint for images
            iconSize = ImVec2(iconFontSize + 6.0f, iconFontSize + 6.0f);
        }

        const ImVec2 nameSize = nameFont
            ? nameFont->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, entryName.c_str())
            : ImGui::CalcTextSize(entryName.c_str());

        maxIconWidth = std::max(maxIconWidth, iconSize.x);
        maxIconHeight = std::max(maxIconHeight, iconSize.y);
        maxNameWidth = std::max(maxNameWidth, nameSize.x);
        maxNameHeight = std::max(maxNameHeight, nameSize.y);
    }

    // Final tile dimensions with minimum size constraints
    const float tileWidth = std::max(96.0f, std::max(maxIconWidth, maxNameWidth) + textPaddingX * 2.0f + 44.0f);
    const float tileHeight = std::max(96.0f, maxIconHeight + contentGap + maxNameHeight + tilePaddingY * 2.0f);

    // Single tile size reused for all items in current folder
    return ImVec2(tileWidth, tileHeight);
}

// Render one grid tile and delegate smaller draw/interaction pieces
void AssetBrowserPanel::_renderFileGridEntry(const std::vector<std::filesystem::directory_entry>& entries,
    const size_t index, const int columns, const float tileWidth, const float tileHeight, const float tileSpacing,
    const float tileRounding, const float iconFontSize, const float nameFontSize, const float textPaddingX,
    const float contentGap, ImFont* nameFont)
{
    // Resolve entry metadata for this tile
    const auto& entry = entries[index];
    const std::string entryPath = entry.path().string();
    const std::string entryName = entry.path().filename().string();
    const bool isSelected = m_selectedAssets.contains(entryPath);
    const bool isDirectory = entry.is_directory();
    const std::string extLower = isDirectory ? std::string() : ToLowerCopy(entry.path().extension().string());
    const bool isImageAsset = !isDirectory && IsImageExtension(extLower);
    const AssetBadgeInfo badgeInfo = GetAssetBadgeInfo(isDirectory, extLower);

    // Unique ID per tile keeps ImGui item state stable
    ImGui::PushID(entryPath.c_str());

    // 1) Tile frame + hover/click input state
    const GridTileInputState tile = DrawGridTileButtonAndBackground(isSelected, index, columns,
        tileSpacing, tileWidth, tileHeight, tileRounding);

    // 2) Label text and metrics
    const bool isRenaming = (m_renamingAsset == entryPath);
    const std::string displayName = isRenaming
        ? entryName
        : TruncateGridLabel(entryName, nameFont, nameFontSize, tileWidth - textPaddingX * 2.0f);

    const ImVec2 nameSize = nameFont
        ? nameFont->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, displayName.c_str())
        : ImGui::CalcTextSize(displayName.c_str());

    const ImVec2 iconSize = MeasureGridIconSize(m_symbolsFont, badgeInfo, iconFontSize, isImageAsset);

    const float nameLineHeight = isRenaming ? ImGui::GetFrameHeight() : nameSize.y;
    const float contentHeight = iconSize.y + contentGap + nameLineHeight;
    const float opticalOffset = isImageAsset ? 0.0f : -6.0f;
    const float contentTop = tile.Min.y + (tileHeight - contentHeight) * 0.5f + opticalOffset;

    // 3) Icon/preview + badge
    DrawGridEntryIconAndBadge(entryPath, badgeInfo, isImageAsset, m_symbolsFont, iconFontSize,
        textPaddingX, tile.Min, tile.Max, tileWidth, iconSize, contentTop);

    // 4) Name or inline rename field
    _renderGridEntryLabel(entry, entryPath, isRenaming, contentTop, iconSize, contentGap,
        textPaddingX, tileWidth, nameFont, nameFontSize, nameSize, displayName);

    // 5) Selection context menu drag/drop
    _handleGridEntryInteractions(entries, entryPath, isSelected, isDirectory, extLower, tile.LeftClick,
        tile.DoubleClick, tile.RightClick, tile.Hovered);

    ImGui::PopID();
}

// Render tile label area
// In rename mode this is an inline text field
void AssetBrowserPanel::_renderGridEntryLabel(const std::filesystem::directory_entry& entry,
    const std::string& entryPath, const bool isRenaming, const float contentTop, const ImVec2& iconSize,
    const float contentGap, const float textPaddingX, const float tileWidth, ImFont* nameFont,
    const float nameFontSize, const ImVec2& nameSize, const std::string& displayName)
{
    if (isRenaming) {
        // Place rename box where normal filename sits
        const float renameY = contentTop + iconSize.y + contentGap;
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetItemRectMin().x + textPaddingX, renameY));
        ImGui::PushItemWidth(tileWidth - textPaddingX * 2.0f);

        if (m_focusRenameInput) {
            // Focus once when rename mode starts
            ImGui::SetKeyboardFocusHere();
            m_focusRenameInput = false;
        }

        if (ImGui::InputText("##Rename", m_renameBuffer, sizeof(m_renameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            // Enter confirms rename
            _commitRename(entry, entryPath);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            // Escape cancels rename UI
            m_renamingAsset.clear();
        }
        ImGui::PopItemWidth();
        return;
    }

    const float nameY = contentTop + iconSize.y + contentGap;
    const ImVec2 tileMin = ImGui::GetItemRectMin();
    const ImVec2 namePos(tileMin.x + (tileWidth - nameSize.x) * 0.5f, nameY);

    // Center text under icon for cleaner tile rhythm
    ImGui::GetWindowDrawList()->AddText(nameFont, nameFontSize, namePos,
        ImGui::ColorConvertFloat4ToU32(EditorStyle::Text), displayName.c_str());
}

// Handle all per-tile interactions
void AssetBrowserPanel::_handleGridEntryInteractions(const std::vector<std::filesystem::directory_entry>& entries,
    const std::string& entryPath, const bool isSelected, const bool isDirectory, const std::string& extLower,
    const bool tileLeftClick, const bool tileDoubleClick, const bool tileRightClick, const bool tileHovered)
{
    if (tileLeftClick) {
        // Shared logic handles single/multi/range/double-click
        _applyEntrySelection(entries, entryPath, isSelected, tileDoubleClick, isDirectory, extLower);
    }

    // Tile also acts as drag source
    _handleAssetDragDrop(entryPath);

    if (tileRightClick) {
        if (!m_selectedAssets.contains(entryPath)) {
            // Right-click should target this tile first
            _setSingleSelection(entryPath, true);
            _notifySelectionChanged();
        }
        ImGui::OpenPopup("ItemContextMenu");
    }

    if (isDirectory) {
        // While dragging, hovering a folder tile can auto-enter it
        _handleFolderHoverAutoOpen(entryPath, tileHovered && ImGui::GetDragDropPayload() != nullptr);
        _handleFolderDropTarget(entryPath);
    }
}

// Render the grid view
void AssetBrowserPanel::_renderFileGridEntries(const std::vector<std::filesystem::directory_entry>& entries) {
    // Grid view: dynamic tile sizing based on largest icon/text in current folder
    const float tileSpacing = 10.0f;
    const float textPaddingX = 8.0f;
    const float tilePaddingY = 8.0f;
    const float tileRounding = 6.0f;
    const float iconFontSize = 66.0f;
    const float nameFontSize = ImGui::GetFontSize();
    const float contentGap = 4.0f;
    ImFont* nameFont = m_mainFont ? m_mainFont : ImGui::GetFont();

    // Find sizes + available width for tiles
    const ImVec2 tileSize = _calculateGridTileSize(entries, nameFont, nameFontSize, iconFontSize, textPaddingX,
        tilePaddingY, contentGap);
    const float tileWidth = tileSize.x;
    const float tileHeight = tileSize.y;
    const float availWidth = ImGui::GetContentRegionAvail().x;

    // Derive how many tiles fit in current row
    const int columns = std::max(1, static_cast<int>((availWidth + tileSpacing) / (tileWidth + tileSpacing)));

    // For every tile
    for (size_t i = 0; i < entries.size(); i++) {
        _renderFileGridEntry(entries, i, columns, tileWidth, tileHeight, tileSpacing, tileRounding, iconFontSize,
            nameFontSize, textPaddingX, contentGap, nameFont);
    }
}

// Render the right file info panel with delete button
void AssetBrowserPanel::_renderFileInfoPanel() {
    // Right side: File info panel (35% width)
    ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);

    // Show multi-selection info or single file info
    if (m_selectedAssets.size() > 1) {
        // Multi-select summary only
        ImGui::Text("Multiple Selection");
        ImGui::Separator();
        ImGui::Text("%zu items selected", m_selectedAssets.size());
    }
    else if (!m_selectedAsset.empty()) {
        // Delegate rich metadata rendering to AssetLibrary
        m_assetLibrary._displaySelectedFileInfo(m_selectedAsset);
    }
    else {
        ImGui::TextDisabled("No file selected");
    }

    // Clicking empty space in file info clears EVERYTHING (File info)
    _selectEmptySpace();

    _renderDeleteButton();
    ImGui::EndChild();
}

// Render the delete button for selected assets
void AssetBrowserPanel::_renderDeleteButton() {
    // Only show delete button if something is selected
    if (!m_selectedAssets.empty()) {
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 5));

        const bool isSingleSelection = m_selectedAssets.size() == 1;
        bool isFolderSelection = false;
        if (isSingleSelection) {
            std::error_code ec;
            // Decide tooltip wording + explorer button visibility
            isFolderSelection = std::filesystem::is_directory(*m_selectedAssets.begin(), ec) && !ec;
        }

        // Style the delete button: icon font + transparent background + red text
        ImGui::PushFont(m_symbolsFont);
        // Button: fully transparent; Hover/Active: subtle grays; Text: red to indicate destructive action
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::FrameBgHover, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::FrameBgActive, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::DangerText);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        // Render delete icon button
        if (ImGui::SmallButton((std::string(EditorIcons::Delete) + "##Delete2").c_str())) {
            // Destructive action applies to full selection
            _deleteSelectedAssets();
        }
        ImGui::PopFont();

        // Show tooltip on hover
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (m_selectedAssets.size() == 1) {
                ImGui::SetTooltip(isFolderSelection ? "Delete selected folder and all contents" : "Delete selected file");
            }
            else {
                ImGui::SetTooltip("Delete %zu selected items", m_selectedAssets.size());
            }
        }

        // Restore style overrides and font state
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        if (isSingleSelection && !isFolderSelection) {
            ImGui::SameLine();
            ImGui::PushFont(m_symbolsFont);
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::FrameBgHover, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::FrameBgActive, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
            if (ImGui::SmallButton((std::string(EditorIcons::Browse) + "##RevealInExplorer").c_str())) {
                // Reveal selected file in explorer
                _openInExplorer(*m_selectedAssets.begin());
            }
            ImGui::PopStyleColor(4);
            ImGui::PopFont();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Show in Explorer");
            }
        }
    }
}

// Render the status bar and update its timer
void AssetBrowserPanel::_renderStatusBar() {
    constexpr float statusBarHeight = 24.0f;
    ImGui::BeginChild("StatusBar", ImVec2(0, statusBarHeight), false, ImGuiWindowFlags_NoScrollbar);

    // Pick text color based on whether the message indicates failure
    const ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
        ? EditorStyle::DangerText
        : EditorStyle::SuccessText;
    ImGui::SetCursorPosX(3);
    ImGui::TextColored(color, "%s", m_statusMessage.c_str());

    // ImGui provides delta time; clamp large spikes caused by file dialogs,
    // breakpoints or window focus loss to avoid instantly wiping the timer
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.2f) {
        dt = 0.0f;
    }

    // Decrement the status timer safely
    m_statusTimer -= dt;
    ImGui::EndChild();
}

// Render create-asset modal (script scene folder)
void AssetBrowserPanel::_renderCreateDialog() {
    const char* dialogTitle = "Create Asset";
    if (m_creationType == AssetCreationType::SCRIPT) dialogTitle = "Create Script";
    else if (m_creationType == AssetCreationType::SCENE) dialogTitle = "Create Scene";
    else if (m_creationType == AssetCreationType::FOLDER) dialogTitle = "Create Folder";
    const std::string popupTitle = std::string(dialogTitle) + "##CreateAssetDialog";

    if (m_openCreateDialog) {
        // OpenPopup only once per request
        ImGui::OpenPopup(popupTitle.c_str());
        m_openCreateDialog = false;
    }

    // Asset creation dialog (modal)
    if (ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(m_mainFont);
        ImGui::Dummy(ImVec2(0, 5));

        // Show script template selector when creating a script
        if (m_creationType == AssetCreationType::SCRIPT) {
            ImGui::Text("Template:");
            ImGui::SameLine();

            int templateCount = 0;
            const char* const* templateNames = Editor::Templates::ScriptTemplates::GetTemplateNames(templateCount);

            const char* currentTemplateName = nullptr;
            switch (m_selectedScriptTemplate) {
            case Editor::Templates::ScriptTemplateType::BasicSystem:
                currentTemplateName = "BasicSystem";
                break;
            case Editor::Templates::ScriptTemplateType::EditModeSystem:
                currentTemplateName = "EditModeSystem";
                break;
            }

            if (ImGui::BeginCombo("##ScriptTemplate", currentTemplateName)) {
                for (int i = 0; i < templateCount; i++) {
                    const bool isSelected = (currentTemplateName && strcmp(templateNames[i], currentTemplateName) == 0);
                    if (ImGui::Selectable(templateNames[i], isSelected)) {
                        // Update selected generator template
                        m_selectedScriptTemplate = Editor::Templates::ScriptTemplates::GetTemplateTypeFromName(templateNames[i]);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const std::string description = Editor::Templates::ScriptTemplates::GetTemplateDescription(m_selectedScriptTemplate);
            ImGui::TextDisabled("%s", description.c_str());
            ImGui::Dummy(ImVec2(0, 5));
        }

        // Name input
        ImGui::Text("Name:");
        ImGui::SameLine();
        if (m_focusNameInput) {
            // Focus field when popup opens
            ImGui::SetKeyboardFocusHere();
            m_focusNameInput = false;
        }

        const bool enterPressed = ImGui::InputText("##AssetName", m_newAssetNameBuffer, sizeof(m_newAssetNameBuffer),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Dummy(ImVec2(0, 5));

        // Buttons
        const bool createClicked = ImGui::Button("Create") || enterPressed;
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
        const bool cancelClicked = ImGui::Button("Cancel");
        ImGui::PopStyleColor(3);

        if (createClicked && strlen(m_newAssetNameBuffer) > 0) {
            // Route create action by requested asset type
            if (m_creationType == AssetCreationType::SCRIPT) {
                _createScript();
            }
            else if (m_creationType == AssetCreationType::SCENE) {
                _createScene();
            }
            else if (m_creationType == AssetCreationType::FOLDER) {
                _createFolder();
            }
            ImGui::CloseCurrentPopup();
        }

        if (cancelClicked) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Prefab Operations and Selection
// -------------------------------------------------------------------------

// Load the selected prefab into the scene
void AssetBrowserPanel::_loadPrefab() {
    // Only proceed if an asset is selected and world pointer is valid
    if (!m_selectedAsset.empty() && m_world) {
        try {
            // Open the selected prefab file
            std::ifstream file(m_selectedAsset);
            // Failure, log it
            if (!file.is_open()) {
                LOG_ERROR("Cannot open file: " << m_selectedAsset);
                m_statusMessage = "Failed to open prefab";
                m_statusTimer = 3.0f;
            }
            else {
                nlohmann::json entityJson;
                file >> entityJson;
                file.close();

                // Deserialize JSON into an entity in the current world
                ECS::Entity entity;
                if (entityJson.contains("Entity")) {
                    // If JSON has Entity key (there's hierarchy)
                    entity = Serialization::EntitySerializer::DeserializeEntityHierarchy(*m_world, entityJson["Entity"]);
                }
                // Load just 1 entity without children
                else if (entityJson.contains("Components")) {
                    entity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);
                }

                // Add PrefabInstanceMetadata component to store prefab hash
                std::filesystem::path p(m_selectedAsset);
                std::string normalizedPath = p.lexically_normal().string();

                uint32_t hash = 0;
                ECS::PrefabManager* prefabManager = m_world->GetPrefabManager();
                if (prefabManager) {
                    // Register/track prefab instance for override workflows
                    hash = prefabManager->RegisterPrefab(normalizedPath);
                    prefabManager->TrackInstance(entity, hash);
                }
                else {
                    // Fallback path when prefab manager unavailable
                    hash = ECS::PrefabManager::ComputeHash(
                        ECS::PrefabManager::NormalizePath(normalizedPath)
                    );
                }

                ECS::Components::PrefabInstanceMetadata meta;
                meta.PrefabHash = hash;
                meta.Flags = 0;
                m_world->Add<ECS::Components::PrefabInstanceMetadata>(entity, meta);

                LOG_INFO("Loaded prefab: " << std::filesystem::path(m_selectedAsset).filename().string());
                m_statusMessage = "Prefab loaded successfully";
                m_statusTimer = 3.0f;
            }
        }
        catch (const std::exception& e) {
            // Catch JSON parse or deserialization errors
            LOG_ERROR("Failed to parse prefab file: " << e.what());
            m_statusMessage = "Failed to load prefab";
            m_statusTimer = 3.0f;
        }
    }
}

// Open the selected prefab in the prefab editor for editing
void AssetBrowserPanel::_editPrefab() {
    // Check if prefab editor instance is available
    if (!m_inspector) {
        LOG_WARNING("Prefab editor not available for prefab editing");
        m_statusMessage = "Prefab editor not available";
        m_statusTimer = 3.0f;
    }
    // Check if prefab is selected
    else if (m_selectedAsset.empty()) {
        m_statusMessage = "Failed to open prefab: none selected";
        m_statusTimer = 3.0f;
    }
    else {
        try {
            // Validate the prefab can be opened and parsed
            std::ifstream file(m_selectedAsset);
            if (!file.is_open()) {
                LOG_ERROR("Cannot open prefab file: " << m_selectedAsset);
                m_statusMessage = "Failed to open prefab";
                m_statusTimer = 3.0f;
            }
            else {
                nlohmann::json prefabJson;
                file >> prefabJson;
                file.close();

                // If parsing succeeded, open in prefab editor and report success
                m_inspector->InspectPrefab(m_selectedAsset);
                m_statusMessage = "Prefab opened";
                m_statusTimer = 3.0f;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to parse prefab file: " << e.what());
            m_statusMessage = "Failed to open prefab";
            m_statusTimer = 3.0f;
        }
    }
}

// Clear all selection state
void AssetBrowserPanel::_clearSelection() {
    // Clear active item path
    m_selectedAsset.clear();
    // Clear multi-select set
    m_selectedAssets.clear();
    // Clear shift-range anchor
    m_anchorAsset.clear();
}

// Set a single selected entry and optionally move anchor
void AssetBrowserPanel::_setSingleSelection(const std::string& entryPath, const bool updateAnchor) {
    // Replace previous selection with current entry
    m_selectedAssets.clear();
    m_selectedAssets.insert(entryPath);
    m_selectedAsset = entryPath;
    if (updateAnchor) {
        // Anchor used by next Shift+click
        m_anchorAsset = entryPath;
    }
}

// Notify listeners when active selection changes
void AssetBrowserPanel::_notifySelectionChanged() const {
    if (m_selectionCallback) {
        m_selectionCallback(m_selectedAsset);
    }
}

// Handle double-click open behavior
bool AssetBrowserPanel::_handleEntryDoubleClick(const std::string& entryPath, const std::string& extLower,
    const bool isDirectory)
{
    if (isDirectory) {
        // Double-click folder enters folder
        m_currentPath = entryPath;
        _clearSelection();
        return true;
    }
    if (extLower == ".prefab" && m_inspector) {
        // Double-click prefab opens inspector prefab mode
        m_inspector->InspectPrefab(entryPath);
        return true;
    }
    if ((extLower == ".scn" || extLower == ".scene") && m_sceneOpenCallback) {
        // Double-click scene delegates open callback
        m_sceneOpenCallback(entryPath);
        return true;
    }
    if (extLower == ".cs") {
        // Double-click script opens project + focuses file
        _openProjectFile(entryPath);
        return true;
    }
    return false;
}

// Apply plain / Ctrl / Shift selection behavior
void AssetBrowserPanel::_applyEntrySelection(const std::vector<std::filesystem::directory_entry>& entries,
    const std::string& entryPath, const bool isSelected, const bool isDoubleClick, const bool isDirectory,
    const std::string& extLower)
{
    if (isDoubleClick && _handleEntryDoubleClick(entryPath, extLower, isDirectory)) {
        // Double-click handled so skip normal selection changes
        return;
    }

    const bool ctrlPressed = ImGui::GetIO().KeyCtrl;
    const bool shiftPressed = ImGui::GetIO().KeyShift;

    if (ctrlPressed) {
        // Ctrl toggles current entry in selection
        if (isSelected) {
            m_selectedAssets.erase(entryPath);
        }
        else {
            m_selectedAssets.insert(entryPath);
            m_anchorAsset = entryPath;
        }
        m_selectedAsset = m_selectedAssets.empty() ? "" : *m_selectedAssets.begin();
        return;
    }

    if (shiftPressed && !m_anchorAsset.empty()) {
        // Shift selects contiguous range from anchor to clicked item
        m_selectedAssets.clear();
        bool inRange = false;
        for (const auto& e : entries) {
            const std::string path = e.path().string();
            if (path == m_anchorAsset || path == entryPath) {
                // Toggle inRange at both endpoints and include them
                m_selectedAssets.insert(path);
                inRange = !inRange;
                if (!inRange) {
                    break;
                }
            }
            else if (inRange) {
                // Include everything between anchor and clicked row
                m_selectedAssets.insert(path);
            }
        }
        m_selectedAsset = entryPath;
        _notifySelectionChanged();
        return;
    }

    // Plain click = single selection
    _setSingleSelection(entryPath, true);
    _notifySelectionChanged();
}

// Auto-enter hovered folder while dragging after a short delay
void AssetBrowserPanel::_handleFolderHoverAutoOpen(const std::string& folderPath, const bool isHoveredWithPayload) {
    static std::string s_hoveredFolder;
    static float s_hoverStartTime = 0.0f;

    if (!isHoveredWithPayload) {
        // Hover ended so reset pending auto-open state
        if (s_hoveredFolder == folderPath) {
            s_hoveredFolder.clear();
        }
        return;
    }

    if (s_hoveredFolder != folderPath) {
        // Started hovering a new folder target
        s_hoveredFolder = folderPath;
        s_hoverStartTime = static_cast<float>(ImGui::GetTime());
        return;
    }

    if (ImGui::GetTime() - s_hoverStartTime > 0.75f) {
        // Dwell long enough so auto-enter folder
        m_currentPath = folderPath;
        _clearSelection();
        s_hoveredFolder.clear();
    }
}

// Apply inline rename and keep selection pointing at renamed path
bool AssetBrowserPanel::_commitRename(const std::filesystem::directory_entry& entry, const std::string& entryPath) {
    try {
        // Build destination path in same parent folder
        const std::filesystem::path newPath = entry.path().parent_path() / m_renameBuffer;
        // Rename on disk
        std::filesystem::rename(entry.path(), newPath);
        // Swap selected path to renamed asset
        m_selectedAssets.erase(entryPath);
        m_selectedAssets.insert(newPath.string());
        m_selectedAsset = newPath.string();
        m_statusMessage = "Renamed to: " + std::string(m_renameBuffer);
        m_statusTimer = 2.0f;
    }
    catch (const std::exception& e) {
        // Keep popup alive and surface error through status bar
        m_statusMessage = "Rename failed: " + std::string(e.what());
        m_statusTimer = 3.0f;
    }

    // Exit rename mode regardless of success
    m_renamingAsset.clear();
    return true;
}

// Clear selection when clicking empty space
void AssetBrowserPanel::_selectEmptySpace() {
    // Click on empty background in this window/child to clear selection
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)
        && !ImGui::IsAnyItemHovered())
    {
        _clearSelection();
        _notifySelectionChanged();
    }
}

// -------------------------------------------------------------------------
// Context Menu
// -------------------------------------------------------------------------

// Helper function to render create menu items (used by both context menu and + button submenu)
bool AssetBrowserPanel::_renderCreateMenuItems() {
    bool openDialog = false;

    // Create Script option
    if (ImGui::MenuItem("Create Script")) {
        // Prime dialog state for script creation
        m_creationType = AssetCreationType::SCRIPT;
        strcpy_s(m_newAssetNameBuffer, "NewScript");
        m_focusNameInput = true;
        openDialog = true;
    }

    // Create Scene option
    if (ImGui::MenuItem("Create Scene")) {
        // Prime dialog state for scene creation
        m_creationType = AssetCreationType::SCENE;
        strcpy_s(m_newAssetNameBuffer, "NewScene");
        m_focusNameInput = true;
        openDialog = true;
    }

    // Create Folder option
    if (ImGui::MenuItem("Create Folder")) {
        // Prime dialog state for folder creation
        m_creationType = AssetCreationType::FOLDER;
        strcpy_s(m_newAssetNameBuffer, "NewFolder");
        m_focusNameInput = true;
        openDialog = true;
    }

    return openDialog;
}

// Context menu shown when right-clicking empty area
void AssetBrowserPanel::_renderContextMenu() {
    if (ImGui::BeginPopup("AssetContextMenu")) {
        ImGui::PushFont(m_mainFont);

        // Use shared helper function
        m_openCreateDialog = _renderCreateMenuItems() || m_openCreateDialog;

        ImGui::Separator();

        // Open Project menu item
        if (ImGui::MenuItem("Open Project")) {
            _openProjectFile();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }

    // Open the dialog outside of the popup to avoid nesting issues
}

// Context menu shown for selected asset rows/tiles
void AssetBrowserPanel::_renderItemContextMenu() {
    if (ImGui::BeginPopup("ItemContextMenu")) {
        ImGui::PushFont(m_mainFont);

        // Show selected count
        if (m_selectedAssets.size() > 1) {
            ImGui::Text("%zu items selected", m_selectedAssets.size());
            ImGui::Separator();
        }

        // Rename (only for single selection)
        if (m_selectedAssets.size() == 1) {
            if (ImGui::MenuItem("Rename", "F2")) {
                _startRename();
            }
        }

        // Copy
        if (ImGui::MenuItem("Copy", "Ctrl+C")) {
            _copySelectedAssets();
        }

        // Cut
        if (ImGui::MenuItem("Cut", "Ctrl+X")) {
            _copySelectedAssets();
            m_clipboardIsCut = true;
        }

        // Paste (if clipboard has items)
        if (!m_clipboardAssets.empty()) {
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {
                _pasteAssets();
            }
        }

        // Delete
        if (ImGui::MenuItem("Delete", "Del")) {
            _deleteSelectedAssets();
        }

        ImGui::PopFont();
        ImGui::EndPopup();
    }
}

// Create script file in current folder using selected template
void AssetBrowserPanel::_createScript() {
    // Create in current directory
    std::filesystem::path targetDir = m_currentPath;
    std::cout << "Creating script in directory: " << targetDir.string() << std::endl;

    // Ensure the directory exists
    if (!exists(targetDir)) {
        // Ensure directory exists before writing file
        create_directories(targetDir);
    }

    // Create file path with .cs extension
    std::string fileName = m_newAssetNameBuffer;
    if (fileName.find(".cs") == std::string::npos) {
        // Auto-append script extension for convenience
        fileName += ".cs";
    }
    std::filesystem::path filePath = targetDir / fileName;

    // Check if file already exists
    if (exists(filePath)) {
        // Stop early if same filename already exists
        m_statusMessage = "Script already exists: " + fileName;
        m_statusTimer = 3.0f;
        LOG_WARNING("Script file already exists: " << filePath.string());
        return;
    }

    // Generate script content using the selected template
    std::string className = m_newAssetNameBuffer;

    // Create namespace from the target directory by replacing
    // path separators with '.' and sanitizing invalid characters.
    std::string ns;
    try {
        // Try to make the path relative to the project root so namespaces
        // are nicer (e.g., "Assets.Scripts") when possible.
        std::filesystem::path projRoot = Engine::ProjectPaths::GetProjectRoot();
        std::filesystem::path rel = std::filesystem::relative(targetDir, projRoot);
        // Start namespace from project-relative directory path
        ns = rel.string();
    }
    catch (...) {
        ns = targetDir.string();
    }

    // Replace separators with dots and sanitize a few problematic chars
    for (char& c : ns) {
        // Convert path separators to namespace separators
        if (c == '/' || c == '\\')
            c = '.';
        // Replace invalid namespace chars with underscore
        else if (c == ':' || c == ' ')
            c = '_';
    }

    // Trim leading/trailing dots if any
    while (!ns.empty() && ns.front() == '.')
        // Strip leading separators from generated namespace
        ns.erase(ns.begin());
    while (!ns.empty() && ns.back() == '.')
        // Strip trailing separators from generated namespace
        ns.pop_back();

    // Fallback namespace if resulting string is empty
    if (ns.empty())
        ns = Engine::ProjectPaths::GetProjectRoot();

    // Generate script using the selected template
    std::string scriptContent = Editor::Templates::ScriptTemplates::GenerateScript(
        m_selectedScriptTemplate,
        className,
        ns
    );

    // Write file
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            m_statusMessage = "Failed to create script: " + fileName;
            m_statusTimer = 3.0f;
            LOG_ERROR("Failed to create script file: " << filePath.string());
            return;
        }

        file << scriptContent;
        // Flush/close explicit so we can safely open immediately after
        file.close();

        m_statusMessage = "Created script: " + fileName;
        m_statusTimer = 3.0f;
        LOG_INFO("Created script: " << filePath.string());

        // Select the newly created file
        m_selectedAsset = filePath.string();

    }
    catch (const std::exception& e) {
        m_statusMessage = "Error creating script: " + std::string(e.what());
        m_statusTimer = 3.0f;
        LOG_ERROR("Exception creating script: " << e.what());
    }
}

// Create new scene file in current folder
void AssetBrowserPanel::_createScene() {
    // Create file path with .scn extension
    std::string fileName = m_newAssetNameBuffer;
    if (fileName.find(".scn") == std::string::npos) {
        // Auto-append scene extension
        fileName += ".scn";
    }
    std::filesystem::path filePath = std::filesystem::path(m_currentPath) / fileName;

    // Check if file already exists
    if (exists(filePath)) {
        // Avoid clobbering existing scene file
        m_statusMessage = "Scene already exists: " + fileName;
        m_statusTimer = 3.0f;
        LOG_WARNING("Scene file already exists: " << filePath.string());
        return;
    }

    // Create empty scene template
    std::string sceneContent =
        "{\n"
        "  \"Version\": \"1.0\",\n"
        "  \"Name\": \"" + std::string(m_newAssetNameBuffer) + "\",\n"
        "  \"Entities\": []\n"
        "}\n";

    // Write file
    try {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            m_statusMessage = "Failed to create scene: " + fileName;
            m_statusTimer = 3.0f;
            LOG_ERROR("Failed to create scene file: " << filePath.string());
            return;
        }

        file << sceneContent;
        // Persist scene template to disk
        file.close();

        m_statusMessage = "Created scene: " + fileName;
        m_statusTimer = 3.0f;
        LOG_INFO("Created scene: " << filePath.string());

        // Select the newly created file
        m_selectedAsset = filePath.string();

    }
    catch (const std::exception& e) {
        m_statusMessage = "Error creating scene: " + std::string(e.what());
        m_statusTimer = 3.0f;
        LOG_ERROR("Exception creating scene: " << e.what());
    }
}

// Create folder in current path
void AssetBrowserPanel::_createFolder() {
    // Create folder path
    std::string folderName = m_newAssetNameBuffer;
    std::filesystem::path folderPath = std::filesystem::path(m_currentPath) / folderName;

    // Use ResourceManager to create directory
    if (RM.CreateDirectory(folderPath.string())) {
        // Surface success and keep new folder selected
        m_statusMessage = "Created folder: " + folderName;
        m_statusTimer = 3.0f;

        // Select the newly created folder
        m_selectedAsset = folderPath.string();
    }
    else {
        m_statusMessage = "Failed to create folder: " + folderName;
        m_statusTimer = 3.0f;
    }
}

// -------------------------------------------------------------------------
// Copy/Paste Operations
// -------------------------------------------------------------------------

// Copy selected assets into clipboard buffer
void AssetBrowserPanel::_copySelectedAssets() {
    // Reset clipboard then copy selected paths
    m_clipboardAssets.clear();
    m_clipboardAssets.assign(m_selectedAssets.begin(), m_selectedAssets.end());
    m_clipboardIsCut = false;

    m_statusMessage = "Copied " + std::to_string(m_clipboardAssets.size()) + " item(s)";
    m_statusTimer = 2.0f;
}

// Paste clipboard assets into current path
void AssetBrowserPanel::_pasteAssets() {
    if (m_clipboardAssets.empty()) {
        m_statusMessage = "Clipboard is empty";
        m_statusTimer = 2.0f;
        return;
    }

    try {
        if (m_clipboardIsCut) {
            // Move operation
            _moveAssetsToDirectory(m_clipboardAssets, m_currentPath);
            // Clear cut clipboard after successful move call
            m_clipboardAssets.clear();
            m_clipboardIsCut = false;
        }
        else {
            // Copy operation
            _copyAssetsToDirectory(m_clipboardAssets, m_currentPath);
        }
    }
    catch (const std::exception& e) {
        m_statusMessage = "Paste failed: " + std::string(e.what());
        m_statusTimer = 3.0f;
        LOG_ERROR("Paste operation failed: " << e.what());
    }
}

// Delete all selected assets
void AssetBrowserPanel::_deleteSelectedAssets() {
    if (m_selectedAssets.empty()) return;

    size_t deleteCount = 0;
    for (const auto& assetPath : m_selectedAssets) {
        // Use ResourceManager to delete the asset (handles cache cleanup automatically)
        if (RM.DeleteAsset(assetPath)) {
            deleteCount++;
        }
    }

    // Clear selection after delete pass
    m_selectedAssets.clear();
    m_selectedAsset.clear();
    m_statusMessage = "Deleted " + std::to_string(deleteCount) + " item(s)";
    m_statusTimer = 2.0f;
}

// Start inline rename for single selection
void AssetBrowserPanel::_startRename() {
    if (m_selectedAssets.size() != 1) return;

    m_renamingAsset = *m_selectedAssets.begin();
    const std::filesystem::path path(m_renamingAsset);
    const std::string filename = path.filename().string();

    strncpy_s(m_renameBuffer, filename.c_str(), sizeof(m_renameBuffer) - 1);
    // Ensure null-termination for safety
    m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
    // Request focus for rename text box next frame
    m_focusRenameInput = true;
}

// -------------------------------------------------------------------------
// Drag-Drop Operations
// -------------------------------------------------------------------------

// Start drag source payload (single or multi-select)
void AssetBrowserPanel::_handleAssetDragDrop(const std::string& assetPath) {
    // Drag source: make selected assets draggable
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // If this asset is part of selection, drag all selected assets
        std::vector<std::string> draggedAssets;
        if (m_selectedAssets.contains(assetPath)) {
            // Dragging selected item drags the whole selection set
            draggedAssets.assign(m_selectedAssets.begin(), m_selectedAssets.end());
        }
        else {
            // Dragging unselected item drags only that item
            draggedAssets.push_back(assetPath);
        }

        // Serialize paths as null-terminated strings concatenated together
        std::string serialized;
        for (const auto& path : draggedAssets) {
            serialized += path;
            serialized += '\0';
        }

        // Set payload with serialized string buffer
        ImGui::SetDragDropPayload("ASSET_PATHS", serialized.data(), serialized.size());

        // Drag preview
        ImGui::PushFont(m_symbolsFont);
        ImGui::Text(EditorIcons::Drag);
        ImGui::PopFont();
        ImGui::SameLine();
        if (draggedAssets.size() == 1) {
            // Show filename in preview for single drag
            const std::filesystem::path path(draggedAssets[0]);
            ImGui::Text("%s", path.filename().string().c_str());
        }
        else {
            // Show count in preview for multi-drag
            ImGui::Text("%zu items", draggedAssets.size());
        }

        ImGui::EndDragDropSource();
    }
}

// Accept drop payload on folder targets and do copy/move
void AssetBrowserPanel::_handleFolderDropTarget(const std::string& folderPath) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATHS")) {
            // Deserialize null-terminated strings from payload
            std::vector<std::string> draggedAssets;
            const char* data = static_cast<const char*>(payload->Data);
            const char* end = data + payload->DataSize;

            while (data < end) {
                std::string path(data);
                if (!path.empty()) {
                    // Skip empty split segments
                    draggedAssets.push_back(path);
                }
                data += path.size() + 1; // Move past null terminator
            }

            try {
                if (ImGui::GetIO().KeyCtrl) {
                    // Ctrl: copy
                    _copyAssetsToDirectory(draggedAssets, folderPath);
                }
                else {
                    // No modifier: move
                    _moveAssetsToDirectory(draggedAssets, folderPath);
                }
            }
            catch (const std::exception& e) {
                m_statusMessage = "Drop failed: " + std::string(e.what());
                m_statusTimer = 3.0f;
                LOG_ERROR("Drop operation failed: " << e.what());
            }
        }
        ImGui::EndDragDropTarget();
    }
}

// Move assets into target directory with conflict-safe names
void AssetBrowserPanel::_moveAssetsToDirectory(const std::vector<std::string>& assets,
    const std::string& targetDir) {
    size_t moveCount = 0;

    for (const auto& assetPath : assets) {
        try {
            std::filesystem::path srcPath(assetPath);
            std::filesystem::path destPath = std::filesystem::path(targetDir) / srcPath.filename();

            // Skip if source and destination are the same
            if (std::filesystem::equivalent(srcPath.parent_path(), targetDir)) {
                continue;
            }

            // Handle name conflicts
            if (std::filesystem::exists(destPath)) {
                std::string baseName = destPath.stem().string();
                std::string extension = destPath.extension().string();
                int counter = 1;

                do {
                    // Suffix with numeric index until path is free
                    destPath = std::filesystem::path(targetDir) /
                        (baseName + "_" + std::to_string(counter) + extension);
                    counter++;
                } while (std::filesystem::exists(destPath));
            }

            std::filesystem::rename(srcPath, destPath);
            moveCount++;
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to move " << assetPath << ": " << e.what());
        }
    }

    m_statusMessage = "Moved " + std::to_string(moveCount) + " item(s)";
    m_statusTimer = 2.0f;
}

// Copy assets into target directory with conflict-safe names
void AssetBrowserPanel::_copyAssetsToDirectory(const std::vector<std::string>& assets,
    const std::string& targetDir) {
    size_t copyCount = 0;

    for (const auto& assetPath : assets) {
        try {
            std::filesystem::path srcPath(assetPath);
            std::filesystem::path destPath = std::filesystem::path(targetDir) / srcPath.filename();

            // Skip if source and destination are the same
            if (std::filesystem::equivalent(srcPath.parent_path(), targetDir)) {
                continue;
            }

            // Handle name conflicts
            if (std::filesystem::exists(destPath)) {
                std::string baseName = destPath.stem().string();
                std::string extension = destPath.extension().string();
                int counter = 1;

                do {
                    // Use _copy suffix to indicate duplicate copy
                    destPath = std::filesystem::path(targetDir) /
                        (baseName + "_copy" + std::to_string(counter) + extension);
                    counter++;
                } while (std::filesystem::exists(destPath));
            }

            // Use ResourceManager to add the asset (handles both files and directories)
            if (RM.AddAsset(srcPath.string(), destPath.string())) {
                // Count only successful copy operations
                copyCount++;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to copy " << assetPath << ": " << e.what());
        }
    }

    m_statusMessage = "Copied " + std::to_string(copyCount) + " item(s)";
    m_statusTimer = 2.0f;
}

// Open csproj and optional script in VS/default handler
void AssetBrowserPanel::_openProjectFile(const std::string& fileToOpen) {
    // Get the project root directory
    std::filesystem::path projectRoot = Engine::ProjectPaths::GetProjectRoot();
    std::string csprojDir = Engine::ProjectPaths::GetCsProjPath();
    // Keep local copy for future script-root logic if needed
    std::string scriptsRootStr = projectRoot.string();
    std::string projectName = std::filesystem::path(projectRoot).filename().string();
    std::string csprojPath = csprojDir + "/" + projectName + ".csproj";

    // Check if the csproj file exists
    if (!std::filesystem::exists(csprojPath)) {
        m_statusMessage = "Project file not found: " + std::filesystem::path(csprojPath).filename().string();
        m_statusTimer = 3.0f;
        LOG_WARNING("Project file not found: " << csprojPath);
        return;
    }

    if (!fileToOpen.empty() && !std::filesystem::exists(fileToOpen)) {
        // Prevent opening stale script paths
        m_statusMessage = "Script file not found";
        m_statusTimer = 3.0f;
        LOG_WARNING("Script file not found: " << fileToOpen);
        return;
    }

    try {
        // Try to open Visual Studio with the project file
#ifdef _WIN32
        // List of common Visual Studio installation paths to try
        std::vector<std::string> vsSearchPaths = {
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\devenv.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\devenv.exe",
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\IDE\\devenv.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE\\devenv.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\Common7\\IDE\\devenv.exe",
            "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\Common7\\IDE\\devenv.exe",
        };

        std::string devenvPath;
        for (const auto& path : vsSearchPaths) {
            if (std::filesystem::exists(path)) {
                // First valid installation path wins
                devenvPath = path;
                break;
            }
        }

        if (devenvPath.empty()) {
            // Try to find devenv in PATH
            int result = system("where devenv.exe > nul 2>&1");
            if (result == 0) {
                devenvPath = "devenv.exe";
            }
        }

        if (!devenvPath.empty()) {
            // Found Visual Studio, open the project
            std::string command = "start \"\" \"" + devenvPath + "\" \"" + csprojPath + "\"";
            if (!fileToOpen.empty()) {
                // /Edit jumps straight to script when provided
                command += " /Edit \"" + fileToOpen + "\"";
            }
            int result = system(command.c_str());
            if (result == 0) {
                // Reflect whether we opened just project or project+script
                m_statusMessage = fileToOpen.empty()
                    ? "Opened project in Visual Studio"
                    : "Opened script in Visual Studio";
                m_statusTimer = 2.0f;
                LOG_INFO("Opened project in Visual Studio: " << csprojPath);
            }
            else {
                m_statusMessage = "Failed to open in Visual Studio";
                m_statusTimer = 3.0f;
                LOG_ERROR("Failed to open project in Visual Studio, system() returned: " << result);
            }
        }
        else {
            // Visual Studio not found, try to open with default application
            std::string command = "start \"\" \"" + csprojPath + "\"";
            int result = system(command.c_str());
            if (result == 0) {
                m_statusMessage = "Opened project file";
                m_statusTimer = 2.0f;
                LOG_INFO("Opened project file: " << csprojPath);
            }
            else {
                m_statusMessage = "Failed to open project file. Please ensure Visual Studio is installed.";
                m_statusTimer = 3.0f;
                LOG_WARNING("Could not find Visual Studio. Tried to open with default application.");
            }

            if (!fileToOpen.empty()) {
                // Fallback open script directly with default app
                std::string fileCommand = "start \"\" \"" + fileToOpen + "\"";
                system(fileCommand.c_str());
            }
        }
#endif
    }
    catch (const std::exception& e) {
        m_statusMessage = "Failed to open project";
        m_statusTimer = 3.0f;
        LOG_ERROR("Failed to open project: " << e.what());
    }
}

// Reveal selected asset in Windows Explorer
void AssetBrowserPanel::_openInExplorer(const std::string& assetPath) {
#ifdef _WIN32
    std::filesystem::path targetPath = std::filesystem::absolute(std::filesystem::path(assetPath));
    if (!std::filesystem::exists(targetPath)) {
        // Guard against deleted/moved assets
        m_statusMessage = "Asset not found";
        m_statusTimer = 3.0f;
        return;
    }

    HINSTANCE result = nullptr;
    if (std::filesystem::is_directory(targetPath)) {
        // target is a folder, so open it directly in Explorer as a new window
        result = ShellExecuteW(nullptr, L"open", targetPath.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    else {
        // target is a file; we want Explorer to open its parent folder with the file highlighted
        // SHOpenFolderAndSelectItems does this reliably; the "explorer /select,<path>" approach
        // breaks on paths with spaces or special characters, so we avoid it
        PIDLIST_ABSOLUTE pidl = nullptr;
        const std::wstring targetWide = targetPath.wstring();

        // SHParseDisplayName converts the file path string into a PIDL (shell item identifier),
        // which is the format SHOpenFolderAndSelectItems requires
        HRESULT hr = SHParseDisplayName(targetWide.c_str(), nullptr, &pidl, 0, nullptr);
        if (SUCCEEDED(hr) && pidl) {
            // Passing 0 items to select means the shell selects the PIDL target itself
            hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
            // PIDL was allocated by SHParseDisplayName, caller is responsible for freeing it
            CoTaskMemFree(pidl);

            if (SUCCEEDED(hr)) {
                m_statusMessage = "Opened in Explorer";
                m_statusTimer = 2.0f;
                return;
            }
        }

        // Fallback when shell selection fails
        const std::wstring params = L"/select,\"" + targetWide + L"\"";
        result = ShellExecuteW(nullptr, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
    }

    if (reinterpret_cast<INT_PTR>(result) > 32) {
        m_statusMessage = "Opened in Explorer";
        m_statusTimer = 2.0f;
    }
    else {
        m_statusMessage = "Failed to open in Explorer";
        m_statusTimer = 3.0f;
    }
#else
    m_statusMessage = "Open in Explorer not supported on this platform";
    m_statusTimer = 3.0f;
#endif
}
