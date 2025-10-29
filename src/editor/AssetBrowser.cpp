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
#include "services/UICommon.h"
#include "core/Logger.h" 
#include <imgui.h>
#include <vector>
#include <services/ResourceManager.h>
#include "serialization/EntitySerializer.h"
#include "ecs/World.h"
#include "ecs/Entity.h"

void AssetBrowser::Initialize(ImFont* mainFont, ImFont* symbolsFont, World* world) {
    m_mainFont = mainFont;
    m_symbolsFont = symbolsFont;
    m_world = world;
}

// Render the asset browser UI window
void AssetBrowser::Render() {
    UICommon::ApplyLayout(UICommon::WindowId::EDITOR_ASSET_BROWSER);
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Asset Browser");

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
            ImGui::SetTooltip("Replace selected texture with a new file");
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

    // Two-column layout (.x is width): file list on left, info panel on right
    float windowWidth = ImGui::GetContentRegionAvail().x;

    // Left side: File/folder list (65% width)
    // Child window = scrollable region within parent window
    ImGui::BeginChild("FileList", ImVec2(windowWidth * 0.65f, 0), true);
    _displayFolder(m_currentPath);
    ImGui::EndChild();

    ImGui::SameLine();

    // Right side: File info panel (35% width)
    // ImVec2(0, 0) = take up remaining horizontal + vertical space
    ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
    _displaySelectedFileInfo();
    ImGui::EndChild();

    // Status message popup (shows success/error messages for 3 seconds)
    if (m_statusTimer > 0.0f) {
        // Position at bottom of the window
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 30);
        ImGui::SetCursorPosX(20);

        // Green text for success, red for errors
        ImVec4 color = (m_statusMessage.find("Failed") != std::string::npos)
            ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)  // Red
            : ImVec4(0.3f, 1.0f, 0.3f, 1.0f);  // Green

        ImGui::TextColored(color, "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;  // Countdown timer
    }

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

            // Update Transform component if present
            if (typeName == "Transform") {
                auto* transform = entity.GetComponent<Component::Transform>();
                if (transform) {
                    // Deserialize JSON data into the component (updates Position, Rotation, Scale)
                    from_json(componentEntry["Data"], *transform);
                }
            }
            // Update SpriteRenderer component
            else if (typeName == "SpriteRenderer") {
                auto* sprite = entity.GetComponent<Component::SpriteRenderer>();
                if (sprite) {
                    // Deserialize JSON data into sprite component (updates TexturePath, Color, FlipX, etc.)
                    from_json(componentEntry["Data"], *sprite);
                }
            }
            // Update Rigidbody2D component
            else if (typeName == "Rigidbody2D") {
                auto* rb = entity.GetComponent<Component::Rigidbody2D>();
                if (rb) {
                    // Deserialize JSON data into rigidbody (updates Mass, Velocity, BodyType, etc.)
                    from_json(componentEntry["Data"], *rb);
                }
            }
            // Update ShapeRenderer2D component
            else if (typeName == "ShapeRenderer2D") {
                auto* shape = entity.GetComponent<Component::ShapeRenderer2D>();
                if (shape) {
                    // Deserialize JSON data into shape renderer (updates Type, FillColor, Radius, etc.)
                    from_json(componentEntry["Data"], *shape);
                }
            }
            // Update CircleCollider2D component
            else if (typeName == "CircleCollider2D") {
                auto* collider = entity.GetComponent<Component::CircleCollider2D>();
                if (collider) {
                    // Deserialize JSON data into circle collider (updates Radius, Offset, IsTrigger, etc.)
                    from_json(componentEntry["Data"], *collider);
                }
            }
            // Update BoxCollider2D component
            else if (typeName == "BoxCollider2D") {
                auto* collider = entity.GetComponent<Component::BoxCollider2D>();
                if (collider) {
                    // Deserialize JSON data into box collider (updates Size, Offset, IsTrigger, etc.)
                    from_json(componentEntry["Data"], *collider);
                }
            }
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
    ImGui::Text(label.c_str());

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
    if (ImGui::DragFloat(("##" + label + "X").c_str(), &x, dragSpeed)) {
        data[xKey] = x;  // Write back to JSON
    }

    // Y field
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20);
    ImGui::Text("Y");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label + "Y").c_str(), &y, dragSpeed)) {
        data[yKey] = y;  // Write back to JSON
    }
}

// Render a single float property with custom field label (works for Mass, Rotation, Volume, etc.)
void AssetBrowser::_renderFloatRow(const std::string& label, const std::string& fieldLabel, nlohmann::json& data,
    const std::string& key, float dragSpeed, float labelOffset)
{
    ImGui::Text(label.c_str());

    float value = data[key];

    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);  // Align with other fields
    ImGui::Text(fieldLabel.c_str());  // Field label like "kg", "°", "m", etc.
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    if (ImGui::DragFloat(("##" + label).c_str(), &value, dragSpeed)) {
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

    ImGui::Text(label.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + labelOffset);
    ImGui::SetNextItemWidth(100);

    // Integer drag field
    if (ImGui::DragInt(("##" + key).c_str(), &value)) {
        data[key] = value;  // Write back to JSON
    }
}

// Render a color picker property (works for any RGBA color in JSON)
void AssetBrowser::_renderColorProperty(const std::string& label, nlohmann::json& colorData, float labelOffset) {
    ImGui::Text(label.c_str());
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
    // AlphaBar = Show alpha (like Unity)
    if (ImGui::ColorEdit4(("##" + label).c_str(), &color.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoDragDrop | 
        ImGuiColorEditFlags_AlphaBar)) 
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
template<typename T>
void AssetBrowser::_renderComponentSection(const std::string& headerName, nlohmann::json& data, T renderContent) {
    // Collapsing header (click triangle to expand/collapse)
    if (ImGui::CollapsingHeader(headerName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        renderContent(data);  // Call the lambda to render component-specific properties
    }
}

// Prefab editor window: displays editable properties for the selected prefab
// (This will eventually move to Inspector when it's implemented)
void AssetBrowser::_showPrefabEditor() {
    UICommon::ApplyLayout(UICommon::WindowId::EDITOR_PREFAB_EDITOR);

    // Window with close button (X) that sets m_editingPrefab to false
    if (ImGui::Begin("Prefab Editor", &m_editingPrefab)) {
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
                        _renderFloatRow("Local Rotation", "R", d, "Rotation", 1.0f);
                        _renderVector2DRow("Local Position", d["Position"], "X", "Y", 1.0f);
                        _renderVector2DRow("Local Scale", d["Scale"], "X", "Y", 0.01f, 52.0f);
                        });
                }
                // SPRITE RENDERER
                // Texture, color tint, flip options, sorting
                else if (componentType == "SpriteRenderer") {
                    _renderComponentSection("Sprite Renderer", data, [this](nlohmann::json& d) {
                        // Sprite texture path (read-only for now)
                        std::string texPath = d.value("TexturePath", "");
                        _renderReadOnlyText("Sprite", texPath.empty() ? "None"
                            : std::filesystem::path(texPath).filename().string());

                        // Color tint picker
                        _renderColorProperty("Color", d["Color"]);

                        // Flip X/Y checkboxes
                        _renderCheckboxRow("Flip", d, "FlipX", "X", "FlipY", "Y");

                        ImGui::Separator();
                        ImGui::Text("Additional Settings");

                        // Sorting layer name (for render order grouping)
                        _renderTextProperty("Sorting Layer", d, "SortingLayerName", 48.0f);

                        // Order in layer (fine-grained sorting within a layer)
                        _renderIntProperty("Order in Layer", d, "SortingOrder", 38.0f);
                        });
                }
                // I'll do more in the future
            }
        }

        ImGui::Separator();

        // Apply button: saves modified JSON to file and updates all prefab instances in the world
        if (ImGui::Button("Apply to All Instances")) {
            std::ofstream file(m_editingPrefabPath);
            if (file.is_open()) {
                file << m_prefabData.dump(2);  // Pretty print with 2-space indent
                file.close();

                // Synchronize all entities that were instantiated from this prefab
                _updatePrefabInstances();

                LOG_INFO("Saved prefab and updated instances");
                m_statusMessage = "Prefab updated successfully";
                m_statusTimer = 3.0f;
                m_editingPrefab = false;  // Close editor window after successful save
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
