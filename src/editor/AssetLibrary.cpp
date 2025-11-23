/* Start Header *****************************************************************/
/*!
\file   AssetLibrary.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025

\brief
Handles all asset browser operations.

Provides:
- Folder navigation and breadcrumb display
- File and folder listing with icons and selection
- Import and replace using OS dialogs
- Drag-drop import support
- Syncing files between ../assets and build/assets
- Safe deletion for files and folders
- Hot reload for textures through ResourceManager
*/
/* End Header *******************************************************************/

// Windows-specific includes must come first
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used Windows stuff
#define NOMINMAX             // Prevent min/max macro conflicts
#include <windows.h>
#include <commdlg.h> 
#endif

#include "AssetLibrary.h"
#include "InspectorPanel.h"
#include "core/Logger.h"
#include "core/ProjectPaths.h"
#include <vector>
#include <services/ResourceManager.h>

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------
    
// Initialize the asset library with the fonts used across the editor
// Stores font pointers so entries can mix icon and text styling
void AssetLibrary::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

// -------------------------------------------------------------------------
// Navigation and Display
// -------------------------------------------------------------------------

// Display a clickable breadcrumb trail for folder navigation
// Shows each path segment as a lightweight link-style button
void AssetLibrary::_displayBreadcrumbs(const std::string& currentPath, std::string& selectedAsset, std::string& outNewPath) {
    std::filesystem::path pathObj(currentPath);
    std::vector<std::filesystem::path> pathParts;

    // Build path parts from root to current (filter out empty/special directory entries)
    for (const auto& part : pathObj) {
        std::string partStr = part.string();
        if (!partStr.empty() && partStr != "." && partStr != ".." && partStr != "/" && partStr != "\\") {
            pathParts.push_back(partStr);
        }
    }

    // Display each part as clickable button with separators
    std::string accumulatedPath;
    for (size_t i = 0; i < pathParts.size(); i++) {
        // Build accumulated path up to this part (e.g. "Assets", "Assets\Audio", etc.)
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

            // Style overrides: make SmallButton look like a link
            // Text: blue; Button: fully transparent; Hover/Active: subtle tinted backgrounds
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.4f, 0.8f, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.8f, 0.5f));

            // When clicked, navigate to that folder level
            if (ImGui::SmallButton(pathParts[i].string().c_str())) {
                outNewPath = accumulatedPath;
                // Only clear selection if we actually navigate to a different folder
                if (outNewPath != currentPath) {
                    selectedAsset.clear();
                }
            }

            // Restore the four color overrides (Text, Button, ButtonHovered, ButtonActive)
            ImGui::PopStyleColor(4);
            // Pop the unique ID for this button
            ImGui::PopID();           
        }
    }
}

// Display all files and folders in the active directory
// Supports single-click select and double-click to enter folders
void AssetLibrary::_displayFolder(const std::filesystem::path& folderPath, std::string& selectedAsset, std::string& currentPath) {
    // Switch to main font; everything inside should use this 
    // unless we temporarily override it (like folder icons)
    ImGui::PushFont(m_mainFont);

    // Check if path exists or if path exists but it's a file, not a folder
    if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Folder not found");
        // Pop the font we pushed above so ImGui stays balanced (files and folders later use icons)
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
            bool isFolderSelected = (selectedAsset == entry.path().string());

            // Single click: select the folder (shows info in right panel)
            if (ImGui::Selectable(folderName.c_str(), isFolderSelected)) {
                selectedAsset = entry.path().string();
            }

            // Double click: navigate into the folder
            // IsMouseDoubleClicked(0) -> 0 means left mouse button
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                currentPath = entry.path().string();
                // Clear selection when navigating
                selectedAsset.clear();
            }
        }
        // If item is file
        else {
            _displayFile(entry.path(), selectedAsset);
        }
    }
    // If we push, we must pop
    ImGui::PopFont();
}

// Display a single file as a selectable entry with an icon
// Keeps selection state so the info panel can reflect details
void AssetLibrary::_displayFile(const std::filesystem::path& filePath, std::string& selectedAsset) {
    // Extract just the filename and then check if THIS file is the currently selected one
    std::string filename = filePath.filename().string();
    bool isSelected = (selectedAsset == filePath.string());

    // Render icon with symbols font
    ImGui::PushFont(m_symbolsFont);
    ImGui::Text("\xEE\xA1\xB3");
    ImGui::PopFont();

    ImGui::SameLine();
    // Selectable file entry
    if (ImGui::Selectable(filename.c_str(), isSelected)) {
        // If clicked, remember it as selected
        selectedAsset = filePath.string();
    }

    // Double-click: open prefab in inspector if applicable
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        selectedAsset = filePath.string();
        if (filePath.extension() == ".prefab" && m_inspector) {
            m_inspector->InspectPrefab(selectedAsset);
        }
    }

    // Enable file dragging
    // Enable drag-and-drop of this file entry
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        // Set payload with type tag "ASSET_PATH"; buffer length includes null terminator
        std::string path = filePath.string();
        ImGui::SetDragDropPayload("ASSET_PATH", path.c_str(), path.size() + 1);

        // Drag preview: icon + filename
        ImGui::PushFont(m_symbolsFont);
        ImGui::Text("\xEF\x8E\xB2");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Text("%s", filename.c_str());
        ImGui::EndDragDropSource();
    }
}

// Display information about the currently selected file in the right panel
void AssetLibrary::_displaySelectedFileInfo(const std::string& selectedAsset) {
    if (selectedAsset.empty()) {
        ImGui::TextDisabled("No file selected");
        return;
    }

    std::filesystem::path selectedPath(selectedAsset);

    // Check if file/folder still exists
    if (!std::filesystem::exists(selectedPath)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Selected file no longer exists");
        return;
    }

    bool isFolder = std::filesystem::is_directory(selectedPath);
    ImGui::Text(isFolder ? "Selected Folder:" : "Selected File:");
    ImGui::Indent();

    // File/folder name
    ImGui::Text("Name: %s", selectedPath.filename().string().c_str());

    // File/folder path
    ImGui::Text("Path: %s", selectedAsset.c_str());

    // Type
    if (isFolder) {
        ImGui::Text("Type: Folder");
    }
    else {
        // File extension
        std::string extension = selectedPath.extension().string();
        if (!extension.empty()) {
            ImGui::Text("Type: %s", extension.c_str());
        }
    }

    // File size (display in bytes, KB or MB depending on size); only for files, not folders
    if (!isFolder) {
        try {
            // Get file size in bytes
            auto fileSize = std::filesystem::file_size(selectedPath);

            // If less than 1KB, show in bytes
            if (fileSize < 1024ull) {
                ImGui::Text("Size: %llu bytes", static_cast<unsigned long long>(fileSize));
            }
            // If less than 1MB, show in KB (divide by 1024)
            else if (fileSize < 1024ull * 1024ull) {
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
    }
    // Stop indenting text
    ImGui::Unindent();
}

// -------------------------------------------------------------------------
// Import Replace Delete
// -------------------------------------------------------------------------
    
// Import a new asset into the current folder via a file dialog
// Copies to both build/assets and ../assets for runtime and source sync
void AssetLibrary::_importAsset(const std::string& currentPath, std::string& selectedAsset, 
    std::string& statusMessage, float& statusTimer) 
{
#ifdef _WIN32  // Only compile this code on Windows
    char filename[512] = "";

    // OPENFILENAMEA is a Windows struct that configures the file dialog
    OPENFILENAMEA ofn = {};                     // Initialize all fields to 0/null
    ofn.lStructSize = sizeof(ofn);              // Tell Windows how big this struct is
    ofn.hwndOwner = nullptr;                    // No parent window (dialog is standalone)
    ofn.lpstrFile = filename;                   // Point to buffer where Windows will write the selected path
    ofn.nMaxFile = sizeof(filename);            // Tell Windows max size of buffer (512 bytes)

    // Support multiple file types
    ofn.lpstrFilter = "All Files\0*.*\0PNG Files\0*.png\0WAV Files\0*.wav\0JSON Files\0*.json\0Prefab Files\0*.prefab\0";
    ofn.nFilterIndex = 1;                       // Default to "All Files"
    ofn.lpstrTitle = "Select Asset to Import";  // Dialog window title

    // OFN_PATHMUSTEXIST: Folder must exist
    // OFN_FILEMUSTEXIST: File must exist (can't type fake name)
    // OFN_NOCHANGEDIR: Don't change working directory after dialog closes
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Show file dialog: returns true if user selected a file, false if cancelled
    if (GetOpenFileNameA(&ofn)) {
        // User selected a file; the path is now in 'filename' buffer
        std::filesystem::path sourcePath(filename);

        // Where to copy it: current folder + just the filename (e.g. "assets/player.png")
        std::filesystem::path destPathBuild = std::filesystem::path(currentPath) / sourcePath.filename();

        // Only operate when inside the project's Assets folder
        std::string assetsRoot = Engine::ProjectPaths::GetAssetsPath();
        auto absCur = std::filesystem::absolute(currentPath).string();
        auto absAssets = std::filesystem::absolute(assetsRoot).string();
        if (absCur.find(absAssets) != std::string::npos) {
            // Use helper function to copy to both locations
            if (_copyFileToBothLocations(sourcePath, destPathBuild, true)) {
                LOG_INFO("Successfully imported to both locations: " << sourcePath.filename().string());
                statusMessage = "File imported: " + sourcePath.filename().string();
                statusTimer = 3.0f;

                // Auto-select the newly imported file
                selectedAsset = destPathBuild.string();
            }
            else {
                statusMessage = "Failed to import file";
                statusTimer = 3.0f;
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

// Replace the currently selected file with a new one of the same type
// Performs hot-reload by refreshing the ResourceManager cache
void AssetLibrary::_replaceTexture(const std::string& selectedAsset, std::string& statusMessage, float& statusTimer) {
    // Checks
    if (selectedAsset.empty()) {
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

    // Build a file dialog filter that matches the extension of the selected asset
    // This ensures the Replace dialog only shows compatible files (e.g. only .png)
    std::filesystem::path destPathBuild(selectedAsset);

    // Becomes .PNG instead of .png basically
    std::string ext = destPathBuild.extension().string();
    std::string upperExt = ext;
    for (auto& c : upperExt) c = (char)std::toupper((unsigned char)c);

    // Build the filter description shown in the file dialog
    // If no extension exists, fall back to "All Files"
    // E.g. ext = ".png" -> "PNG files"
    std::string filterDesc = upperExt.empty() ? std::string("All Files") : (upperExt.substr(1) + " Files");
    
    // Build the actual wildcard pattern the dialog will filter by
    // E.g. ext = ".png" -> "*.png" and ext = "" -> "*.*"
    std::string filterPattern = ext.empty() ? std::string("*.*") : ("*" + ext);

    // Combine description + pattern into the Windows dialog filter format
    // "\0" separators are required by Win32 API formatting
    // E.g. "PNG Files\0*.png\0All Files\0*.*\0" matching import textures basically
    std::string filter = filterDesc + "\0" + filterPattern + "\0All Files\0*.*\0";

    // Everything from this point onwards is just the same stuff
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    std::string title = ext.empty() ? std::string("Select file to replace with") : ("Select " + upperExt + " to replace with");
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Show file dialog
    if (GetOpenFileNameA(&ofn)) {
        std::filesystem::path sourcePath(filename);
        // Extension must match
        if (sourcePath.extension() != destPathBuild.extension()) {
            LOG_WARNING("Extension mismatch: selected " << destPathBuild.extension().string() << ", replacement " << sourcePath.extension().string());
            statusMessage = "Replace failed: extension mismatch";
            statusTimer = 3.0f;
            return;
        }

    // Only operate when inside the project's Assets folder
    std::string assetsRoot = Engine::ProjectPaths::GetAssetsPath();
    auto absSel = std::filesystem::absolute(selectedAsset).string();
    auto absAssets = std::filesystem::absolute(assetsRoot).string();
    if (absSel.find(absAssets) != std::string::npos) {
        // Use helper function to copy to both locations
        if (_copyFileToBothLocations(sourcePath, destPathBuild, false)) {
            // Hot reload: force ResourceManager to reload the texture
            // Basically update assets while the program is running without restarting it
            RM.UnloadAsset(selectedAsset);   // Remove old cached version
            RM.Get<Texture>(selectedAsset);  // Load new version into cache

                LOG_INFO("Successfully replaced asset in both locations: " << destPathBuild.filename().string());
                statusMessage = "File replaced: " + destPathBuild.filename().string();
                statusTimer = 3.0f;
            }
            else {
                statusMessage = "Failed to replace file";
                statusTimer = 3.0f;
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

// Handle a file dropped from the OS into the asset browser
// Mirrors the import flow and selects the newly copied file
void AssetLibrary::_handleFileDrop(const std::string& sourcePathStr, const std::string& currentPath, 
    std::string& selectedAsset, std::string& statusMessage, float& statusTimer) 
{
    std::filesystem::path sourcePath(sourcePathStr);

    // Where to copy it: current folder + just the filename
    std::filesystem::path destPathBuild = std::filesystem::path(currentPath) / sourcePath.filename();

    // Only operate when inside the project's Assets folder
    std::string assetsRoot = Engine::ProjectPaths::GetAssetsPath();
    auto absCur = std::filesystem::absolute(currentPath).string();
    auto absAssets = std::filesystem::absolute(assetsRoot).string();
    if (absCur.find(absAssets) != std::string::npos) {
        // Use helper function to copy to both locations
        if (_copyFileToBothLocations(sourcePath, destPathBuild, true)) {
            LOG_INFO("Successfully imported dropped file to: " << currentPath);
            statusMessage = "File imported: " + sourcePath.filename().string();
            statusTimer = 3.0f;

            // Auto-select the newly imported file
            selectedAsset = destPathBuild.string();
        }
        else {
            statusMessage = "Failed to import file";
            statusTimer = 3.0f;
        }
    }
}

// Delete the currently selected file or folder safely in both locations
// Removes from build/assets and mirrors the deletion in ../assets
void AssetLibrary::_deleteSelectedAsset(std::string& selectedAsset, std::string& statusMessage, 
    float& statusTimer) 
{
    if (selectedAsset.empty()) {
        LOG_WARNING("No file or folder selected to delete");
        return;
    }
    std::filesystem::path selectedPath(selectedAsset);

    // Check if file/folder exists
    if (!std::filesystem::exists(selectedPath)) {
        LOG_WARNING("Selected item no longer exists");
        statusMessage = "Item not found";
        statusTimer = 3.0f;
        // Clear selection if they don't exist
        selectedAsset.clear();
        return;
    }
    bool isFolder = std::filesystem::is_directory(selectedPath);

    // Only operate when inside the project's Assets folder
    std::string assetsRoot = Engine::ProjectPaths::GetAssetsPath();
    auto absSel = std::filesystem::absolute(selectedAsset).string();
    auto absAssets = std::filesystem::absolute(assetsRoot).string();
    if (absSel.find(absAssets) != std::string::npos) {
        // Use helper function to delete from both locations
        if (_deleteFromBothLocations(selectedPath, isFolder)) {
            LOG_INFO("Deleted: " << selectedPath.filename().string());
            statusMessage = "Deleted: " + selectedPath.filename().string();
            statusTimer = 3.0f;

            // Clear selection after deletion
            selectedAsset.clear();
        }
        else {
            statusMessage = "Failed to delete item";
            statusTimer = 3.0f;
        }
    }
}

// -------------------------------------------------------------------------
// Helper Methods
// -------------------------------------------------------------------------
    
// Copy a file to both build and source asset locations
// Handles directory creation if needed and returns success status
bool AssetLibrary::_copyFileToBothLocations(const std::filesystem::path& sourcePath,
    const std::filesystem::path& destBuildPath, bool createDirs) 
{
    try {
        // If requested, ensure the destination folder for the build asset exists
        if (createDirs) {
            std::filesystem::create_directories(destBuildPath.parent_path());
        }

        // First copy: write file into the build/assets (runtime)
        std::filesystem::copy_file(sourcePath, destBuildPath,
            std::filesystem::copy_options::overwrite_existing);

        // Construct project assets path using ProjectPaths
        std::filesystem::path assetsRoot = std::filesystem::absolute(Engine::ProjectPaths::GetAssetsPath());
        std::filesystem::path absoluteDest = std::filesystem::absolute(destBuildPath);
        std::filesystem::path relativePath = std::filesystem::relative(absoluteDest, assetsRoot);
        std::filesystem::path destSourcePath = assetsRoot / relativePath;

        // Ensure parent directories for the source assets exist if needed
        if (createDirs) {
            std::filesystem::create_directories(destSourcePath.parent_path());
        }

        // Second copy: write the file into ../assets (source): only if parent exists or we're creating dirs
        if (!std::filesystem::equivalent(destSourcePath, destBuildPath) &&
            (createDirs || std::filesystem::exists(destSourcePath.parent_path()))) {
            // Allow replacing a file with the same name
            std::filesystem::copy_file(sourcePath, destSourcePath,
            std::filesystem::copy_options::overwrite_existing);
        }

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to copy file to both locations: " << e.what());
        return false;
    }
}

// Delete a file or folder from both build and source asset locations
// Handles both single files and recursive folder deletion
bool AssetLibrary::_deleteFromBothLocations(const std::filesystem::path& pathToDelete, bool isFolder) {
    try {
        // Delete from build/assets
        if (isFolder) {
            // remove_all so both folder + files
            std::filesystem::remove_all(pathToDelete);
        }
        else {
            // Else, just the file itself
            std::filesystem::remove(pathToDelete);
        }

        // Construct project assets path using ProjectPaths
        std::filesystem::path assetsRoot = std::filesystem::absolute(Engine::ProjectPaths::GetAssetsPath());
        std::filesystem::path absolutePath = std::filesystem::absolute(pathToDelete);
        std::filesystem::path relativePath = std::filesystem::relative(absolutePath, assetsRoot);
        std::filesystem::path sourcePathToDelete = assetsRoot / relativePath;

        // Delete from ../assets (source) if it exists
        if (!std::filesystem::equivalent(sourcePathToDelete, pathToDelete) && std::filesystem::exists(sourcePathToDelete)) {
            // Same logic
            if (isFolder) {
                std::filesystem::remove_all(sourcePathToDelete);
            }
            else {
                std::filesystem::remove(sourcePathToDelete);
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to delete from both locations: " << e.what());
        return false;
    }
}
