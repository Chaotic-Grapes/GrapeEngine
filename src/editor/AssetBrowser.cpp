/* Start Header *****************************************************************/
/*!
\file   AssetBrowser.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the AssetBrowser class for browsing and managing game assets.

Features:
- File browser with breadcrumb navigation
- File selection with info display
- Display of assets folder structure
- Prefab editing and instance synchronization

References:
- Windows file dialog using Win32 API (commdlg.h)
- ImGui styling and layout functions (imgui.h)
- Breadcrumb navigation pattern & button customization adapted from ImGui examples
*/
/* End Header *******************************************************************/

// Windows-specific includes must come first
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used Windows stuff
#define NOMINMAX             // Prevent min/max macro conflicts
#include <windows.h>
#include <commdlg.h> 
#endif

#include "../editor/AssetBrowser.h"
#include "core/Logger.h" 
#include <vector>
#include <services/ResourceManager.h>
#include "serialization/EntitySerializer.h"
#include "ecs/World.h"
#include "ecs/Entity.h"

void AssetBrowser::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont, World* world) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
}

// Render the asset browser UI window
void AssetBrowser::Render() {
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Asset Browser");

    // Apply font scale to this window
    ImGui::SetWindowFontScale(m_fontScale);

    // Accept files dragged from Windows Explorer
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("FILES")) {
            // ImGui doesn't handle OS drag-drop by default
            // So we need to enable this in window creation
            LOG_INFO("File dropped from OS");
        }
        ImGui::EndDragDropTarget();
    }

    // Display clickable breadcrumb navigation
    _displayBreadcrumbs();

    // Import button (upload icon)
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEF\x82\x9B")) {
        _importAsset();
    }
    ImGui::PopFont();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Import new assets into current folder");
    }

    ImGui::SameLine();

    // Replace button (only enabled if a file is selected)
    bool hasSelection = !m_selectedAsset.empty();
    if (!hasSelection) ImGui::BeginDisabled();

    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEE\xA3\x94")) {
        _replaceTexture();
    }

    ImGui::PopFont();
    if (!hasSelection) ImGui::EndDisabled();

    // Show tooltip even when disabled
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (hasSelection) {
            ImGui::SetTooltip("Replace the selected texture with a new file while keeping the original name");
        }
        else {
            ImGui::SetTooltip("Replace selected texture with a new file (disabled)");
        }
    }

    ImGui::SameLine();

    // + button (only enabled if a prefab is selected)
    bool isPrefab = !m_selectedAsset.empty() && std::filesystem::path(m_selectedAsset).extension() == ".prefab";
    if (!isPrefab) ImGui::BeginDisabled();

    // + button containing load and edit prefab buttons
    ImGui::PushFont(m_symbolsFont);
    if (ImGui::Button("\xEE\x85\x85\xEE\x8C\x93")) {
        ImGui::OpenPopup("Prefabs");
    }

    ImGui::PopFont();
    if (!isPrefab) ImGui::EndDisabled();

    // Tooltip for prefab button
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (isPrefab) {
            ImGui::SetTooltip("Prefab management (load/edit)");
        }
        else {
            ImGui::SetTooltip("Prefab management (disabled)");
        }
    }

    // Popup window with prefab options
    if (ImGui::BeginPopup("Prefabs")) {
        // Load prefab option: instantiate into world
        if (ImGui::Selectable("Load Prefab")) {
            _loadPrefab();
        }
        // Edit prefab option: open prefab editor
        if (ImGui::Selectable("Edit Prefab")) {
            _editPrefab();
        }

        ImGui::EndPopup();
    }

    // Font scale controls
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 193);

    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("##Scale", &m_fontScale, 0.5f, 1.5f, "%.1fx")) {
        m_fontScale = std::clamp(m_fontScale, 0.5f, 1.5f);
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Global UI Scale");
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset##Scale")) {
        m_fontScale = 1.0f;
    }

    // Two-column layout (.x is width): file list on left, info panel on right
    float windowWidth = ImGui::GetContentRegionAvail().x;

    // Left side: File/folder list (65% width)
    // Child window = scrollable region within parent window
    ImGui::BeginChild("FileList", ImVec2(windowWidth * 0.65f, 0), true);
    ImGui::SetWindowFontScale(m_fontScale);
    _displayFolder(m_currentPath);
    ImGui::EndChild();

    ImGui::SameLine();

    // Right side: File info panel (35% width)
    // ImVec2(0, 0) = take up remaining horizontal + vertical space
    ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
    ImGui::SetWindowFontScale(m_fontScale);
    _displaySelectedFileInfo();
    ImGui::EndChild();

    // Status message popup (shows success/error messages for 3 seconds)
    if (m_statusTimer > 0.0f) {
        // Position at bottom of the window
        ImGui::SetCursorPosX(20);
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 30);

        // Green text for success, red for errors
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)  // Red
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);  // Green

        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;  // Countdown timer
    }

    ImGui::SameLine();

    ImGui::End();
    ImGui::PopFont();

    // Show prefab editor window if user clicked "Edit Prefab"
    if (m_editingPrefab) {
        _showPrefabEditor();
    }
}

// Display clickable breadcrumb trail (e.g., assets > Audio > Music)
void AssetBrowser::_displayBreadcrumbs() {
    std::filesystem::path currentPath(m_currentPath);
    std::vector<std::filesystem::path> pathParts;

    // Build path parts from root to current (filter out empty/special directory entries)
    for (const auto& part : currentPath) {
        std::string partStr = part.string();
        if (!partStr.empty() && partStr != "." && partStr != ".." && partStr != "/" && partStr != "\\") {
            pathParts.push_back(partStr);
        }
    }

    // Display each part as clickable button with separators
    std::string accumulatedPath;
    for (size_t i = 0; i < pathParts.size(); i++) {
        // Build accumulated path up to this part (e.g., "assets", "assets\Audio", etc.)
        if (i > 0) accumulatedPath += "\\";
        accumulatedPath += pathParts[i].string();

        // Add ">" separator between breadcrumb parts (but not before first one)
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }

        // Display breadcrumb part
        if (i == pathParts.size() - 1) {
            // Last part (current directory): display as plain text, not clickable
            ImGui::Text("%s", pathParts[i].string().c_str());
        }
        else {
            // Previous parts: display as clickable blue buttons
            ImGui::PushID(static_cast<int>(i));  // Unique ID so ImGui can distinguish buttons

            // Style buttons to look like clickable links (blue text, transparent background)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));           // Blue text
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));                     // Transparent
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.4f, 0.8f, 0.3f));  // Subtle hover
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.8f, 0.5f));   // Subtle click

            // When clicked, navigate to that folder level
            if (ImGui::SmallButton(pathParts[i].string().c_str())) {
                m_currentPath = accumulatedPath;
                m_selectedAsset.clear();  // Clear file selection when changing folders
            }

            ImGui::PopStyleColor(4);  // Pop all 4 color style overrides (Text, Button, ButtonHovered, ButtonActive)
            ImGui::PopID();           // Pop the unique ID for this button
        }
    }
}

// Display all files and folders in the given directory
void AssetBrowser::_displayFolder(const std::filesystem::path& folderPath) {
    ImGui::PushFont(m_mainFont);

    // Check if path exists or if path exists but it's a file, not a folder
    if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Folder not found");
        ImGui::PopFont();
        return;
    }

    // Iterate through each item in directory (file or folder)
    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        // If item is folder
        if (entry.is_directory()) {
            // Render folder icon with symbols font
            ImGui::PushFont(m_symbolsFont);
            ImGui::Text("\xEE\x8B\x87");
            ImGui::PopFont();

            ImGui::SameLine();

            // Render folder name with main font
            std::string folderName = entry.path().filename().string();
            if (ImGui::Selectable(folderName.c_str())) {
                // If clicked, navigate into it by changing current path
                m_currentPath = entry.path().string();
                // Clear selection when navigating
                m_selectedAsset.clear();
            }
        }
        // If item is file
        else {
            _displayFile(entry.path());
        }
    }
    ImGui::PopFont();
}

// Display a single file as a selectable entry
void AssetBrowser::_displayFile(const std::filesystem::path& filePath) {
    // Extract just the filename and then check if THIS file is the currently selected one
    std::string filename = filePath.filename().string();
    bool isSelected = (m_selectedAsset == filePath.string());

    // Render icon with symbols font
    ImGui::PushFont(m_symbolsFont);
    ImGui::Text("\xEE\xA1\xB3");
    ImGui::PopFont();

    ImGui::SameLine();
    // Selectable file entry
    if (ImGui::Selectable(filename.c_str(), isSelected)) {
        // If clicked, remember it as selected
        m_selectedAsset = filePath.string();
    }

    // Enable file dragging
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // Store the file path as payload
        std::string path = filePath.string();
        // ASSET_PATH is data's type name; size includes null terminator (hence +1)
        ImGui::SetDragDropPayload("ASSET_PATH", path.c_str(), path.size() + 1);

        // Show preview while dragging
        ImGui::PushFont(m_symbolsFont);
        ImGui::Text("\xEF\x8E\xB2");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Text("%s", filename.c_str());
        ImGui::EndDragDropSource();
    }
}

// Display information about the currently selected file in the right panel
void AssetBrowser::_displaySelectedFileInfo() {
    if (m_selectedAsset.empty()) {
        ImGui::TextDisabled("No file selected");
        return;
    }

    std::filesystem::path selectedPath(m_selectedAsset);

    // Check if file still exists
    if (!std::filesystem::exists(selectedPath)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected file no longer exists");
        return;
    }

    ImGui::Text("Selected File:");
    ImGui::Indent();

    // File name
    ImGui::Text("Name: %s", selectedPath.filename().string().c_str());

    // File path
    ImGui::Text("Path: %s", m_selectedAsset.c_str());

    // File extension
    std::string extension = selectedPath.extension().string();
    if (!extension.empty()) {
        ImGui::Text("Type: %s", extension.c_str());
    }

    // File size (display in bytes, KB or MB depending on size)
    try {
        // Get file size in bytes
        auto fileSize = std::filesystem::file_size(selectedPath);

        // If less than 1KB, show in bytes
        if (fileSize < 1024) {
            ImGui::Text("Size: %llu bytes", fileSize);
        }
        // If less than 1MB, show in KB (divide by 1024)
        else if (fileSize < 1024 * 1024) {
            ImGui::Text("Size: %.2f KB", fileSize / 1024.0);
        }
        // Otherwise show in MB (divide by 1024 * 1024)
        else {
            ImGui::Text("Size: %.2f MB", fileSize / (1024.0 * 1024.0));
        }
    }
    catch (const std::exception& e) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Could not get file size");
        LOG_ERROR("Failed to get file size: " << e.what());
    }

    // Stop indenting text
    ImGui::Unindent();
}

// Import a new asset file into the current folder using Windows file dialog
void AssetBrowser::_importAsset() {
#ifdef _WIN32  // Only compile this code on Windows
    char filename[512] = "";

    // OPENFILENAMEA is a Windows struct that configures the file dialog
    OPENFILENAMEA ofn = {};                                  // Initialize all fields to 0/null
    ofn.lStructSize = sizeof(ofn);                           // Tell Windows how big this struct is
    ofn.hwndOwner = nullptr;                                 // No parent window (dialog is standalone)
    ofn.lpstrFile = filename;                                // Point to buffer where Windows will write the selected path
    ofn.nMaxFile = sizeof(filename);                         // Tell Windows max size of buffer (512 bytes)

    // Support multiple file types
    ofn.lpstrFilter = "All Files\0*.*\0PNG Files\0*.png\0WAV Files\0*.wav\0JSON Files\0*.json\0Prefab Files\0*.prefab\0";
    ofn.nFilterIndex = 1;                                    // Default to "All Files"
    ofn.lpstrTitle = "Select Asset to Import";               // Dialog window title

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    // OFN_PATHMUSTEXIST: Folder must exist
    // OFN_FILEMUSTEXIST: File must exist (can't type fake name)
    // OFN_NOCHANGEDIR: Don't change working directory after dialog closes

    // Show file dialog: returns true if user selected a file, false if cancelled
    if (GetOpenFileNameA(&ofn)) {
        // User selected a file; the path is now in 'filename' buffer
        std::filesystem::path sourcePath(filename);

        // Where to copy it: current folder + just the filename (e.g. "assets/player.png")
        std::filesystem::path destPathBuild = std::filesystem::path(m_currentPath) / sourcePath.filename();

        // Also copy to source assets folder (relative to build/)
        std::string sourceAssetsPath = m_currentPath;

        // Check if path contains "assets"
        if (sourceAssetsPath.find("assets") != std::string::npos) {
            // Replace build/assets with ../assets
            std::filesystem::path destPathSource = std::filesystem::path("..") / m_currentPath / sourcePath.filename();

            try {
                // Copy to build/assets (runtime)
                // If file exists, overwrite it
                std::filesystem::copy_file(sourcePath, destPathBuild, std::filesystem::copy_options::overwrite_existing);

                // Copy to ../assets (source)
                std::filesystem::create_directories(destPathSource.parent_path());
                std::filesystem::copy_file(sourcePath, destPathSource, std::filesystem::copy_options::overwrite_existing);

                LOG_INFO("Successfully imported to both locations");

                // Auto-select the newly imported file
                m_selectedAsset = destPathBuild.string();
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to import texture: " << e.what());
            }
        }
    }
    // User clicked cancel
    else {
        LOG_INFO("Import cancelled by user");
    }

#else  // Not Windows (Linux, Mac, etc.)
    LOG_WARNING("File dialog not implemented for this platform");
#endif
}

// Replace the currently selected texture file with a new one (hot reload support)
void AssetBrowser::_replaceTexture() {
    if (m_selectedAsset.empty()) {
        LOG_WARNING("No file selected to replace");
        return;
    }

#ifdef _WIN32
    // Same as above (import textures)
    char filename[512] = "";

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = "Select PNG to Replace With";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Show file dialog
    if (GetOpenFileNameA(&ofn)) {
        std::filesystem::path sourcePath(filename);
        std::filesystem::path destPathBuild(m_selectedAsset);  // Use selected file's path

        // Also replace in source assets folder
        std::string sourceAssetsPath = m_selectedAsset;

        if (sourceAssetsPath.find("assets") != std::string::npos) {
            // Construct path to source assets (replace build/assets with ../assets)
            std::filesystem::path relativePath = std::filesystem::relative(destPathBuild, "assets");
            std::filesystem::path destPathSource = std::filesystem::path("..") / "assets" / relativePath;

            try {
                // Replace file in build/assets (runtime)
                std::filesystem::copy_file(sourcePath, destPathBuild, std::filesystem::copy_options::overwrite_existing);

                // Replace file in ../assets (source)
                if (std::filesystem::exists(destPathSource.parent_path())) {
                    std::filesystem::copy_file(sourcePath, destPathSource, std::filesystem::copy_options::overwrite_existing);
                }

                // Hot reload: force ResourceManager to reload the texture
                // Basically update assets while the program is running without restarting it
                RM.UnloadAsset(m_selectedAsset);   // Remove old cached version
                RM.Get<Texture>(m_selectedAsset);  // Load new version into cache

                LOG_INFO("Successfully replaced texture in both locations: " << destPathBuild.filename().string());
                m_statusMessage = "Texture replaced successfully";
                m_statusTimer = 3.0f; // Show for 3 seconds
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to replace texture: " << e.what());
                m_statusMessage = "Failed to replace texture";
                m_statusTimer = 3.0f;
            }
        }
    }
    else {
        LOG_INFO("Replace cancelled by user");
    }

#else
    LOG_WARNING("File dialog not implemented for this platform");
#endif
}

// Load and instantiate selected prefab into the level
void AssetBrowser::_loadPrefab() {
    // Ensure prefab is selected
    if (m_selectedAsset.empty()) {
        LOG_WARNING("No prefab selected");
        return;
    }

    // Ensure valid world reference exists
    if (!m_world) {
        LOG_ERROR("Cannot load prefab: No world reference");
        return;
    }

    // Try to open the selected prefab file
    std::ifstream file(m_selectedAsset);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open file: " << m_selectedAsset);
    }
    else {
        try {
            // Parse prefab JSON
            auto entityJson = nlohmann::json::parse(file);
            file.close();

            // Deserialize creates the entity internally
            auto entity = Serialization::EntitySerializer::DeserializeEntity(*m_world, entityJson);

            // Tag entity with prefab link so we can update it later when prefab changes
            entity.AddComponent<Component::PrefabLink>(m_selectedAsset);

            LOG_INFO("Loaded prefab: " << std::filesystem::path(m_selectedAsset).filename().string());
            m_statusMessage = "Prefab loaded successfully";
            m_statusTimer = 3.0f;
        }
        catch (const std::exception& e) {
            // Handle JSON parsing or deserialization errors
            LOG_ERROR("Failed to parse prefab file: " << e.what());
            m_statusMessage = "Failed to load prefab";
            m_statusTimer = 3.0f;
        }
    }
}

// Open prefab for editing (loads JSON into memory and sets flag to show editor window)
void AssetBrowser::_editPrefab() {
    // Ensure prefab is selected
    if (m_selectedAsset.empty()) {
        LOG_WARNING("No prefab selected to edit");
        return;
    }

    // Try to open the prefab file
    std::ifstream file(m_selectedAsset);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open prefab file: " << m_selectedAsset);
        m_statusMessage = "Failed to open prefab";
        m_statusTimer = 3.0f;
        return;
    }

    try {
        // Parse prefab JSON into memory
        m_prefabData = nlohmann::json::parse(file);
        m_editingPrefabPath = m_selectedAsset;
        m_editingPrefab = true;  // This flag triggers _showPrefabEditor() to render
        file.close();

        LOG_INFO("Opened prefab for editing: " << std::filesystem::path(m_selectedAsset).filename().string());
    }
    catch (const std::exception& e) {
        // Handle parsing errors
        LOG_ERROR("Failed to parse prefab: " << e.what());
        m_statusMessage = "Failed to parse prefab";
        m_statusTimer = 3.0f;
        m_editingPrefab = false;
    }
}

// Find all entities using this prefab and update them with new prefab data
void AssetBrowser::_updatePrefabInstances() {
    // Safety checks
    if (!m_world || m_editingPrefabPath.empty()) return;
    if (!m_prefabData.contains("Components")) return;

    int updatedCount = 0;  // Track successful updates
    auto allEntities = m_world->GetEntityManager().GetAllEntities();

    // Check each entity to see if it uses this prefab
    for (auto entityId : allEntities) {
        auto entity = m_world->GetEntityManager().GetEntity(entityId);

        // Check if entity is linked to this prefab
        auto* prefabLink = entity.GetComponent<Component::PrefabLink>();
        if (!prefabLink || prefabLink->prefabPath != m_editingPrefabPath) {
            continue;  // Skip if not linked to this prefab
        }

        // Update this prefab instance with new data
        if (_updateEntityFromPrefab(entity)) {
            updatedCount++;
        }
    }

    LOG_INFO("Updated " << updatedCount << " prefab instances");
}

// Update single entity's components from prefab data (synchronizes instance with prefab)
bool AssetBrowser::_updateEntityFromPrefab(Entity& entity) {
    try {
        // Iterate through each component defined in the prefab JSON
        // The prefab JSON structure is: { "Components": [ { "Type": "Transform", "Data": {...} }, ... ] }
        for (const auto& componentEntry : m_prefabData["Components"]) {
            std::string typeName = componentEntry["Type"];  // Get component type name (e.g. "Transform")

            // Helper lambda that handles the repetitive pattern ([&] captures everything by reference)
            // Checks if component matches type name; if so, tries to get the actual component via GetComponent<T>()
            // Also calls from_json which reads JSON data and updates component's fields
            auto updateComponent = [&]<typename T>(const std::string& name) {
                if (typeName == name) {
                    if (auto* component = entity.GetComponent<T>()) {
                        from_json(componentEntry["Data"], *component);
                    }
                    return true;
                }
                return false;
            };

            // For each component type, call the lambda with the type + JSON type name
            // Updates Position, Rotation, Scale
            if (updateComponent.operator()<Component::Transform>("Transform")) continue;
            // Updates TexturePath, Color, FlipX, etc.
            if (updateComponent.operator()<Component::SpriteRenderer>("SpriteRenderer")) continue;
            // Updates Mass, Velocity, BodyType, etc.
            if (updateComponent.operator()<Component::Rigidbody2D>("Rigidbody2D")) continue;
            // Updates Radius, Offset, IsTrigger, etc.
            if (updateComponent.operator()<Component::CircleCollider2D>("CircleCollider2D")) continue;
            // Updates Radius, Offset, IsTrigger, etc.
            if (updateComponent.operator()<Component::BoxCollider2D>("BoxCollider2D")) continue;
            // Updates Type, FillColor, Radius, etc.
            if (updateComponent.operator()<Component::ShapeRenderer2D>("ShapeRenderer2D")) continue;
            // Updates Start, End, Thickness, etc.
            if (updateComponent.operator()<Component::LineRenderer>("LineRenderer")) continue;
        }

        LOG_INFO("Updated entity " << entity.GetId() << " from prefab");
        return true;  // Success
    }
    catch (const std::exception& e) {
        // JSON parsing or component update failed
        LOG_ERROR("Failed to update entity " << entity.GetId() << ": " << e.what());
        return false;
    }
}

// Render a property with X and Y fields (works for Position, Scale, Velocity, Size, etc.)
void AssetBrowser::_renderVector2DRow(const std::string& label, nlohmann::json& data, const std::string& xKey,
    const std::string& yKey, float dragSpeed, float labelOffset)
{
    // Strip ## suffix for display
    std::string displayLabel = label;
    size_t pos = label.find("##");
    if (pos != std::string::npos) {
        displayLabel = label.substr(0, pos);
    }

    ImGui::Text(displayLabel.c_str());

    // Extract current values from JSON
    float x = data[xKey];
    float y = data[yKey];

    // X field
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);  // Align with other fields
    ImGui::Text("X");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    // Hold to drag, double click to type
    if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[xKey] = x;  // Write back to JSON
    }

    // Y field
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::Text("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[yKey] = y;  // Write back to JSON
    }
}

// Render a single float property with custom field label (works for Mass, Rotation, Volume, etc.)
void AssetBrowser::_renderFloatRow(const std::string& label, const std::string& fieldLabel, nlohmann::json& data,
    const std::string& key, float dragSpeed, float labelOffset)
{
    // Strip ## suffix for display
    std::string displayLabel = label;
    size_t pos = label.find("##");
    if (pos != std::string::npos) {
        displayLabel = label.substr(0, pos);
    }

    ImGui::Text(displayLabel.c_str());
    float value = data[key];

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);  // Align with other fields
    ImGui::Text(fieldLabel.c_str());  // Field label like "kg", "m", etc.
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed, 0.0f, 0.0f, "%.2f")) {
        data[key] = value;  // Write back to JSON
    }
}

// Render a text input property (works for TexturePath, Name, Tag, etc.)
void AssetBrowser::_renderTextProperty(const std::string& label, nlohmann::json& data, const std::string& key,
    float labelOffset)
{
    std::string value = data[key];
    char buf[128];
    strncpy_s(buf, value.c_str(), sizeof(buf) - 1);

    ImGui::Text(label.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::SetNextItemWidth(100);

    // Text input field
    if (ImGui::InputText(("##" + key).c_str(), buf, sizeof(buf))) {
        data[key] = std::string(buf);  // Write back to JSON
    }
}

// Render an integer drag property (works for SortingOrder, MaxParticles, FontSize, etc.)
void AssetBrowser::_renderIntProperty(const std::string& label, nlohmann::json& data, const std::string& key,
    float labelOffset)
{
    int value = data[key];

    // Strip ## suffix for display
    std::string displayLabel = label;
    size_t pos = label.find("##");
    if (pos != std::string::npos) {
        displayLabel = label.substr(0, pos);
    }

    ImGui::Text(displayLabel.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::SetNextItemWidth(100);

    // Integer drag field
    if (ImGui::DragInt(("##" + label).c_str(), &value)) {
        data[key] = value;  // Write back to JSON
    }
}

// Render a color picker property (works for any RGBA color in JSON)
void AssetBrowser::_renderColorProperty(const std::string& label, nlohmann::json& colorData, float labelOffset) {
    // Strip ## suffix for display
    std::string displayLabel = label;
    size_t pos = label.find("##");
    if (pos != std::string::npos) {
        displayLabel = label.substr(0, pos);
    }

    ImGui::Text(displayLabel.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);

    // Convert JSON RGBA (0-255) to ImGui color (0-1)
    // Normalize it first
    ImVec4 color(
        colorData["R"].get<float>() / 255.0f,
        colorData["G"].get<float>() / 255.0f,
        colorData["B"].get<float>() / 255.0f,
        colorData["A"].get<float>() / 255.0f
    );

    // IMGUI HAS BUILT-IN COLOR EDITORS AND PICKERS
    // ColorEdit4 = RGBA editor with sliders + color square
    // NoInputs cause we don't want to show the RGBA values before clicking into the color picker
    // NoDragDrop = can't drag and drop color between widgets 
    // AlphaBar = show alpha (like Unity)
    // NoLabel = removes the title next to the freaking box thing
    // PickerHueWheel = color picker wheel (not default vertical hue bar)
    if (ImGui::ColorEdit4(label.c_str(), &color.x, ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_NoLabel | 
        ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaBar))
    {
        // When user edits color, ImGui gives a float (0-1 range) back
        // Write back to JSON (convert 0-1 back to 0-255 for saving)
        colorData["R"] = color.x * 255.0f;
        colorData["G"] = color.y * 255.0f;
        colorData["B"] = color.z * 255.0f;
        colorData["A"] = color.w * 255.0f;
    }
}

// Render read-only text with label (for displaying non-editable info like file paths)
void AssetBrowser::_renderReadOnlyText(const std::string& label, const std::string& value, float labelOffset) {
    ImGui::Text(label.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::TextDisabled("%s", value.c_str());  // Gray text = read-only
}

// Render two checkboxes on same row (works for FlipX/FlipY, Loop/PlayOnAwake, etc.)
void AssetBrowser::_renderCheckboxRow(const std::string& label, nlohmann::json& data, const std::string& key1,
    const std::string& label1, const std::string& key2, const std::string& label2, float labelOffset)
{
    ImGui::Text(label.c_str());
    ImGui::SameLine();

    bool value1 = data[key1];
    bool value2 = data[key2];

    // First checkbox
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::SetNextItemWidth(20);
    if (ImGui::Checkbox(("##" + key1).c_str(), &value1)) data[key1] = value1;
    ImGui::SameLine();
    ImGui::Text(label1.c_str());

    // Second checkbox
    ImGui::SameLine();
    ImGui::SetNextItemWidth(20);
    if (ImGui::Checkbox(("##" + key2).c_str(), &value2)) data[key2] = value2;
    ImGui::SameLine();
    ImGui::Text(label2.c_str());
}

// Generic component section renderer: wraps content in a collapsing header
// Lambda function allows flexible rendering of any component's properties
template <typename T>
void AssetBrowser::_renderComponentSection(const std::string& headerName, nlohmann::json& data, T renderContent) {
    // Collapsing header (click triangle to expand/collapse)
    if (ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        renderContent(data);  // Call the lambda to render component-specific properties
    }
}

// Check if prefab already has a specific component type
bool AssetBrowser::_prefabHasComponent(const std::string& componentType) {
    if (!m_prefabData.contains("Components")) return false;

    // Self-explanatory
    for (const auto& component : m_prefabData["Components"]) {
        if (component["Type"] == componentType) {
            return true;
        }
    }
    return false;
}

// Add a new component to the prefab with default values
void AssetBrowser::_addComponentToPrefab(const std::string& componentType) {
    // Prevent duplicate components
    if (_prefabHasComponent(componentType)) {
        LOG_WARNING("Component " << componentType << " already exists in prefab");
        m_statusMessage = "Component already exists";
        m_statusTimer = 3.0f;
        return;
    }

    // Ensure the "Components" array exists in the prefab JSON
    if (!m_prefabData.contains("Components")) {
        m_prefabData["Components"] = nlohmann::json::array();
    }

    // Create a new JSON entry for the component
    nlohmann::json newComponent;
    newComponent["Type"] = componentType;

    // Create default data for each component type
    // So that when they're added the default data is loaded
    if (componentType == "Transform") {
        newComponent["Data"] = {
            {"Position", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Rotation", 0.0f},
            {"Scale", {{"X", 1.0f}, {"Y", 1.0f}}}
        };
    }
    else if (componentType == "SpriteRenderer") {
        newComponent["Data"] = {
            {"TexturePath", ""},
            {"Sprite", ""},
            {"Width", 0},
            {"Height", 0},
            {"Color", {{"R", 255.0f}, {"G", 255.0f}, {"B", 255.0f}, {"A", 255.0f}}},
            {"FlipX", false},
            {"FlipY", false},
            {"SortingOrder", 0},
            {"SortingLayerName", "Default"}
        };
    }
    else if (componentType == "Rigidbody2D") {
        newComponent["Data"] = {
            {"Mass", 1.0f},
            {"LinearDamping", 0.0f},
            {"AngularDamping", 0.05f},
            {"GravityScale", 1.0f},
            {"BodyType", 0}, // Dynamic
            {"FreezeRotation", false},
            {"LinearVelocity", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"AngularVelocity", 0.0f},
            {"Inertia", 0.0f},
            {"CenterOfMass", {{"X", 0.0f}, {"Y", 0.0f}}}
        };
    }
    else if (componentType == "CircleCollider2D") {
        newComponent["Data"] = {
            {"IsTrigger", false},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Radius", 0.5f},
            {"Layer", 0}
        };
    }
    else if (componentType == "BoxCollider2D") {
        newComponent["Data"] = {
            {"IsTrigger", false},
            {"Offset", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Size", {{"X", 1.0f}, {"Y", 1.0f}}},
            {"Layer", 0}
        };
    }
    else if (componentType == "ShapeRenderer2D") {
        newComponent["Data"] = {
            {"Type", 0}, // Rectangle
            {"FillColor", {{"R", 255.0f}, {"G", 255.0f}, {"B", 255.0f}, {"A", 255.0f}}},
            {"OutlineColor", {{"R", 0.0f}, {"G", 0.0f}, {"B", 0.0f}, {"A", 255.0f}}},
            {"OutlineThickness", 1.0f},
            {"Size", {{"X", 100.0f}, {"Y", 100.0f}}},
            {"Radius", 50.0f},
            {"Points", nlohmann::json::array()},
            {"Closed", true}
        };
    }
    else if (componentType == "LineRenderer") {
        newComponent["Data"] = {
            {"Start", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"End", {{"X", 0.0f}, {"Y", 0.0f}}},
            {"Thickness", 1.0f},
            {"Color", {{"R", 255.0f}, {"G", 255.0f}, {"B", 255.0f}, {"A", 255.0f}}}
        };
    }

    // Add the new component to the prefab's component list
    m_prefabData["Components"].push_back(newComponent);
    LOG_INFO("Added " << componentType << " to prefab");
}

// Prefab editor window: displays editable properties for the selected prefab
// (This will eventually move to Inspector when it's implemented)
void AssetBrowser::_showPrefabEditor() {
    // Window with close button (X) that sets m_editingPrefab to false
    if (ImGui::Begin("Prefab Editor", &m_editingPrefab)) {
        ImGui::SetWindowFontScale(m_fontScale);
        // Header: Display which prefab we're editing
        ImGui::Text("Prefab");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::TextDisabled("%s", m_editingPrefabPath.empty() ? "None" :
            std::filesystem::path(m_editingPrefabPath).filename().string().c_str());

        ImGui::Separator();

        // Render all components in the prefab
        if (m_prefabData.contains("Components")) {
            for (auto& componentEntry : m_prefabData["Components"]) {
                std::string componentType = componentEntry["Type"];
                auto& data = componentEntry["Data"];

                // TRANSFORM
                // Position, rotation, scale
                if (componentType == "Transform") {
                    // [this] lets the lambda call member functions of this class (e.g. _renderFloatRow)
                    // Without it, these function calls wouldn't compile because the lambda wouldn't
                    // have access to the current class instance
                    _renderComponentSection("Transform", data, [this](nlohmann::json& d) {
                        _renderFloatRow("Local Rotation", "R", d, "Rotation", 1.0f, 19.0f);
                        _renderVector2DRow("Local Position", d["Position"], "X", "Y", 1.0f);
                        _renderVector2DRow("Local Scale", d["Scale"], "X", "Y", 0.01f, 41.0f);
                        });
                }
                // SPRITE RENDERER
                // Texture, color tint, flip options, sorting
                else if (componentType == "SpriteRenderer") {
                    _renderComponentSection("Sprite Renderer", data, [this](nlohmann::json& d) {
                        // Sprite texture path
                        std::string texPath = d.value("TexturePath", "");
                        ImGui::Text("Sprite");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 107.0f);

                        // Make the text a drop target
                        ImGui::TextDisabled("%s", texPath.empty() ? "None"
                            : std::filesystem::path(texPath).filename().string().c_str());

                        // Drop target (accepts dragged files)
                        if (ImGui::BeginDragDropTarget()) {
                            // Customize drop target appearance
                            ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.2f, 0.5f, 1.0f, 1.0f)); // Blue highlight

                            // A payload is the data we attach to a drag operation, so what we're "carrying"
                            // When we start dragging something
                            if (const ImGuiPayload* payLoad = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                                std::string droppedPath = static_cast<const char*>(payLoad->Data);

                                // Only accept .png files for sprites
                                if (std::filesystem::path(droppedPath).extension() == ".png") {
                                    d["TexturePath"] = droppedPath;
                                    d["Sprite"] = droppedPath;
                                    LOG_INFO("Dropped texture: " << droppedPath);
                                }
                            }
                            ImGui::PopStyleColor(); // Pop the color style
                            ImGui::EndDragDropTarget();
                        }

                        // Color tint picker
                        _renderColorProperty("Color##Sprite", d["Color"], 110.0f);

                        // Flip X/Y checkboxes
                        _renderCheckboxRow("Flip", d, "FlipX", "X", "FlipY", "Y", 126.0f);

                        ImGui::Separator();
                        ImGui::PushFont(m_boldFont);
                        // Save current header colors
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));           // Transparent when not hovered
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));    // Transparent when hovered
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));     // Transparent when active
                        // Collapsing header (click triangle to expand/collapse)
                        if (ImGui::CollapsingHeader("Additional Settings")) {
                            ImGui::PopStyleColor(3);                                          // Pop all 3 color styles
                            ImGui::PopFont();
                            // Sorting layer name (for render order grouping)
                            _renderTextProperty("Sorting Layer", d, "SortingLayerName", 44.0f);

                            // Order in layer (fine-grained sorting within a layer)
                            _renderIntProperty("Order in Layer", d, "SortingOrder", 37.0f);
                        }
                        else {
                            // Also pop if header is collapsed
                            ImGui::PopStyleColor(3);
                            ImGui::PopFont();
                        }
                        });
                }
                // RIGIDBODY 2D
                else if (componentType == "Rigidbody2D") {
                    _renderComponentSection("Rigidbody2D", data, [this](nlohmann::json& d) {
                        // Body Type dropdown
                        ImGui::Text("Body Type");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 65.0f);
                        ImGui::SetNextItemWidth(100);

                        const char* bodyTypes[] = { "Dynamic", "Kinematic", "Static" };
                        int currentType = d["BodyType"];
                        if (ImGui::Combo("##BodyType", &currentType, bodyTypes, 3)) {
                            d["BodyType"] = currentType;
                        }

                        _renderFloatRow("Mass", "kg", d, "Mass", 0.1f, 82.0f);
                        _renderFloatRow("Linear Damping", "", d, "LinearDamping", 0.01f, 16.0f);
                        _renderFloatRow("Angular Damping", "", d, "AngularDamping", 0.01f, 3.0f);
                        _renderFloatRow("Gravity Scale", "", d, "GravityScale", 0.1f, 37.0f);

                        // Freeze Rotation
                        ImGui::Text("Freeze Rotation");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(172);

                        bool freezeRot = d.value("FreezeRotation", false);
                        if (ImGui::Checkbox("##FreezeRotation", &freezeRot)) {
                            d["FreezeRotation"] = freezeRot;
                        }
                        ImGui::SameLine();
                        ImGui::Text("Z");

                        ImGui::Separator();
                        ImGui::PushFont(m_boldFont);
                        // Save current header colors
                        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));           
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));    
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));     
                        // Collapsing header (click triangle to expand/collapse)
                        if (ImGui::CollapsingHeader("Info")) {
                            ImGui::PopStyleColor(3);                                         
                            ImGui::PopFont();
                            _renderVector2DRow("Linear Velocity", d["LinearVelocity"], "X", "Y", 1.0f, 11.0f);
                            _renderFloatRow("Angular Velocity", "", d, "AngularVelocity", 1.0f, 10.0f);
                            _renderFloatRow("Inertia", "", d, "Inertia", 0.1f, 95.0f);
                            _renderVector2DRow("Center of Mass", d["CenterOfMass"], "X", "Y", 0.1f, 7.0f);
                        }
                        else {
                            ImGui::PopStyleColor(3);
                            ImGui::PopFont();
                        }
                        });
                }
                // CIRCLE COLLIDER 2D
                else if (componentType == "CircleCollider2D") {
                    _renderComponentSection("Circle Collider 2D", data, [this](nlohmann::json& d) {
                        // Is Trigger
                        ImGui::Text("Is Trigger");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(173);

                        bool isTrigger = d.value("IsTrigger", false);
                        if (ImGui::Checkbox("##IsTriggerCircle", &isTrigger)) {
                            d["IsTrigger"] = isTrigger;
                        }
                        _renderVector2DRow("Offset##Circle", d["Offset"], "X", "Y", 1.0f, 83.0f);
                        _renderFloatRow("Radius##Circle", "px", d, "Radius", 1.0f, 71.0f);
                        _renderIntProperty("Layer##Circle", d, "Layer", 110.0f);
                        });
                }
                // BOX COLLIDER 2D
                else if (componentType == "BoxCollider2D") {
                    _renderComponentSection("Box Collider 2D", data, [this](nlohmann::json& d) {
                        // Is Trigger
                        ImGui::Text("Is Trigger");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(173);

                        bool isTrigger = d.value("IsTrigger", false);
                        if (ImGui::Checkbox("##IsTriggerBox", &isTrigger)) {
                            d["IsTrigger"] = isTrigger;
                        }
                        _renderVector2DRow("Offset##Box", d["Offset"], "X", "Y", 1.0f, 83.0f);
                        _renderVector2DRow("Size##Box", d["Size"], "X", "Y", 1.0f, 100.0f);
                        _renderIntProperty("Layer##Box", d, "Layer", 110.0f);
                        });
                }
                // LINE RENDERER
                else if (componentType == "LineRenderer") {
                    _renderComponentSection("Line Renderer", data, [this](nlohmann::json& d) {
                        _renderVector2DRow("Start", d["Start"], "X", "Y", 1.0f, 95.0f);
                        _renderVector2DRow("End", d["End"], "X", "Y", 1.0f, 103.0f);
                        _renderFloatRow("Thickness", "px", d, "Thickness", 0.1f, 42.0f);
                        _renderColorProperty("Color##Line", d["Color"], 111.0f);
                        });
                }
                // SHAPE RENDERER 2D
                else if (componentType == "ShapeRenderer2D") {
                    _renderComponentSection("Shape Renderer 2D", data, [this](nlohmann::json& d) {
                        // Shape Type dropdown
                        ImGui::Text("Shape Type");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 56.0f);
                        ImGui::SetNextItemWidth(100);

                        const char* shapeTypes[] = { "Rectangle", "Circle", "Polygon" };
                        int currentShape = d.value("Type", 0);
                        if (ImGui::Combo("##ShapeType", &currentShape, shapeTypes, 3)) {
                            d["Type"] = currentShape;
                        }

                        // Shape-specific properties
                        if (currentShape == 0) {      // Rectangle
                            _renderVector2DRow("Size##Rectangle", d["Size"], "X", "Y", 1.0f, 100.0f);
                        }
                        else if (currentShape == 1) { // Circle
                            _renderFloatRow("Radius##Circle2", "px", d, "Radius", 1.0f, 71.0f);
                        }
                        else if (currentShape == 2) { // Polygon
                            ImGui::PushFont(m_boldFont);
                            ImGui::Text("Points");
                            ImGui::PopFont();

                            // Get points array
                            auto& points = d["Points"];
                            float maxWidth = ImGui::CalcTextSize("Point 88").x; // A wide number

                            // Display each point
                            for (size_t i = 0; i < points.size(); i++) {
                                ImGui::PushID(static_cast<int>(i));

                                // Fixed width label
                                std::string label = "Point " + std::to_string(i);
                                ImGui::Text("%s", label.c_str());
                                float currentWidth = ImGui::CalcTextSize(label.c_str()).x;
                                if (currentWidth < maxWidth) {
                                    ImGui::SameLine();
                                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (maxWidth - currentWidth));
                                }

                                // Now the vector fields will always start at the same position
                                _renderVector2DRow("##Point", points[i], "X", "Y", 1.0f, 58.0f);

                                // Delete point button
                                ImGui::SameLine();
                                ImGui::PushFont(m_symbolsFont);

                                // Make button transparent but text red
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));                    // Transparent background
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f)); // Subtle hover
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));  // Subtle active
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));          // Red text/icon

                                if (ImGui::SmallButton("\xEE\xA1\xB2")) { // Trash can icon
                                    points.erase(points.begin() + i);
                                    ImGui::PopStyleColor(4);              // Pop all 4 color styles
                                    ImGui::PopFont();
                                    ImGui::PopID();
                                    break;
                                }
                                ImGui::PopStyleColor(4);
                                ImGui::PopFont();
                                ImGui::PopID();
                            }

                            // Add point button
                            if (ImGui::Button("Add Point")) {
                                points.push_back({ {"X", 0.0f}, {"Y", 0.0f} });
                            }

                            // Closed checkbox
                            ImGui::SameLine();
                            bool closed = d.value("Closed", true);
                            if (ImGui::Checkbox("Closed##PolygonClosed", &closed)) {
                                d["Closed"] = closed;
                            }
                        }

                        ImGui::Separator();
                        ImGui::PushFont(m_boldFont);
                        ImGui::Text("Colors");
                        ImGui::PopFont();
                        _renderColorProperty("Fill Color##Shape", d["FillColor"], 84.0f);
                        _renderColorProperty("Outline Color##Shape", d["OutlineColor"], 47.0f);
                        _renderFloatRow("\" Thickness", "px", d, "OutlineThickness", 0.1f, 29.0f);
                        });
                }
            }
        }

        ImGui::Separator();

        // Add Component button
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentMenu");
        }

        // Dropdown menu for component selection
        if (ImGui::BeginPopup("AddComponentMenu")) {
            ImGui::PushFont(m_boldFont);
            ImGui::Text("Components");
            ImGui::PopFont();
            ImGui::Separator();

            // Transform
            bool hasTransform = _prefabHasComponent("Transform");
            if (hasTransform) ImGui::BeginDisabled();
            if (ImGui::Selectable("Transform")) {
                _addComponentToPrefab("Transform");
            }
            if (hasTransform) ImGui::EndDisabled();
            if (hasTransform && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Component already added");
            }

            // Sprite Renderer
            bool hasSpriteRenderer = _prefabHasComponent("SpriteRenderer");
            if (hasSpriteRenderer) ImGui::BeginDisabled();
            if (ImGui::Selectable("Sprite Renderer")) {
                _addComponentToPrefab("SpriteRenderer");
            }
            if (hasSpriteRenderer) ImGui::EndDisabled();
            if (hasSpriteRenderer && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Component already added");
            }

            // Rigidbody 2D
            bool hasRigidbody = _prefabHasComponent("Rigidbody2D");
            if (hasRigidbody) ImGui::BeginDisabled();
            if (ImGui::Selectable("Rigidbody 2D")) {
                _addComponentToPrefab("Rigidbody2D");
            }
            if (hasRigidbody) ImGui::EndDisabled();
            if (hasRigidbody && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Component already added");
            }

            // Circle Collider 2D
            bool hasCircleCollider = _prefabHasComponent("CircleCollider2D");
            if (hasCircleCollider) ImGui::BeginDisabled();
            if (ImGui::Selectable("Circle Collider 2D")) {
                _addComponentToPrefab("CircleCollider2D");
            }
            if (hasCircleCollider) ImGui::EndDisabled();
            if (hasCircleCollider && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Component already added");
            }

            // Box Collider 2D
            bool hasBoxCollider = _prefabHasComponent("BoxCollider2D");
            if (hasBoxCollider) ImGui::BeginDisabled();
            if (ImGui::Selectable("Box Collider 2D")) {
                _addComponentToPrefab("BoxCollider2D");
            }
            if (hasBoxCollider) ImGui::EndDisabled();
            if (hasBoxCollider && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Component already added");
            }

            // Shape Renderer 2D
            bool hasShapeRenderer = _prefabHasComponent("ShapeRenderer2D");
            if (hasShapeRenderer) ImGui::BeginDisabled();
            if (ImGui::Selectable("Shape Renderer 2D")) {
                _addComponentToPrefab("ShapeRenderer2D");
            }
            if (hasShapeRenderer) ImGui::EndDisabled();
            if (hasShapeRenderer && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Component already added");
            }

            // Line Renderer
            bool hasLineRenderer = _prefabHasComponent("LineRenderer");
            if (hasLineRenderer) ImGui::BeginDisabled();
            if (ImGui::Selectable("Line Renderer")) {
                _addComponentToPrefab("LineRenderer");
            }
            if (hasLineRenderer) ImGui::EndDisabled();
            if (hasLineRenderer && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Component already added");
            }

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        // Apply button: saves modified JSON to file and updates all prefab instances in the world
        if (ImGui::Button("Apply to All Instances")) {
            std::ofstream file(m_editingPrefabPath);
            // Save to build/assets
            if (file.is_open()) {
                file << m_prefabData.dump(2);  // Pretty print with 2-space indent
                file.close();

                // Also save to source assets folder (../assets)
                std::string sourceAssetsPath = m_editingPrefabPath;
                if (sourceAssetsPath.find("assets") != std::string::npos) {
                    std::filesystem::path relativePath = std::filesystem::relative(
                        std::filesystem::path(m_editingPrefabPath), "assets");
                    std::filesystem::path destPathSource = std::filesystem::path("..") / "assets" / relativePath;

                    std::ofstream fileSource(destPathSource);
                    if (fileSource.is_open()) {
                        fileSource << m_prefabData.dump(2);
                        fileSource.close();
                    }
                }

                // Synchronize all entities that were instantiated from this prefab
                _updatePrefabInstances();

                LOG_INFO("Saved prefab and updated instances");
                m_statusMessage = "Prefab updated successfully";
                m_statusTimer = 3.0f;
                m_editingPrefab = false;      // Close editor window after successful save
            }
        }

        ImGui::SameLine();

        // Cancel button: discard changes and close window without saving
        if (ImGui::Button("Cancel")) {
            m_editingPrefab = false;
        }
    }
    ImGui::End();
}
