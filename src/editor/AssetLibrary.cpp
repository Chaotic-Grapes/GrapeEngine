/* Start Header *****************************************************************/
/*!
\file   AssetLibrary.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   3rd November 2025
\brief
Implements file system operations and asset management utilities.

Features:
- File browser navigation and display
- Asset import/replacement with dual location sync
- File information retrieval and formatting
- OS file drop support
- Safe asset deletion

References:
- Windows file dialog using Win32 API (commdlg.h)
- ImGui styling and layout functions (imgui.h)
*/
/* End Header *******************************************************************/

// Windows-specific includes must come first
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used Windows stuff
#define NOMINMAX             // Prevent min/max macro conflicts
#include <windows.h>
#include <commdlg.h> 
#endif

#include "../editor/AssetLibrary.h"
#include "../editor/InspectorPanel.h"
#include "core/Logger.h"
#include <vector>
#include <services/ResourceManager.h>

// Initialize the asset library with the fonts used across the editor.
// Stores font pointers so entries can mix icon and text styling.
void AssetLibrary::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;
}

// Display a clickable breadcrumb trail for folder navigation.
// Shows each path segment as a lightweight link-style button.
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
        // Build accumulated path up to this part (e.g. "assets", "assets\Audio", etc.)
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
            ImGui::PopID();           // Pop the unique ID for this button
        }
    }
}

// Display all files and folders in the active directory.
// Supports single-click select and double-click to enter folders.
void AssetLibrary::_displayFolder(const std::filesystem::path& folderPath, std::string& selectedAsset, std::string& currentPath) {
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
            bool isFolderSelected = (selectedAsset == entry.path().string());

            // Single click: select the folder (shows info in right panel)
            if (ImGui::Selectable(folderName.c_str(), isFolderSelected)) {
                selectedAsset = entry.path().string();
            }

            // Double click: navigate into the folder
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
    ImGui::PopFont();
}

// Display a single file as a selectable entry with an icon.
// Keeps selection state so the info panel can reflect details.
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

        // Optional drag preview: icon + filename
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

// Import a new asset into the current folder via a file dialog.
// Copies to both build/assets and ../assets for runtime and source sync.
void AssetLibrary::_importAsset(const std::string& currentPath, std::string& selectedAsset,
    std::string& statusMessage, float& statusTimer) {
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
        std::filesystem::path destPathBuild = std::filesystem::path(currentPath) / sourcePath.filename();

        // Also copy to source assets folder (relative to build/)
        std::string sourceAssetsPath = currentPath;

        // Check if path contains "assets"
        if (sourceAssetsPath.find("assets") != std::string::npos) {
            // Replace build/assets with ../assets
            std::filesystem::path destPathSource = std::filesystem::path("..") / currentPath / sourcePath.filename();

            try {
                // Copy to build/assets (runtime)
                // If file exists, overwrite it
                std::filesystem::copy_file(sourcePath, destPathBuild, std::filesystem::copy_options::overwrite_existing);

                // Copy to ../assets (source)
                std::filesystem::create_directories(destPathSource.parent_path());
                std::filesystem::copy_file(sourcePath, destPathSource, std::filesystem::copy_options::overwrite_existing);

                LOG_INFO("Successfully imported to both locations");

                // Auto-select the newly imported file
                selectedAsset = destPathBuild.string();
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

// Replace the currently selected file with a new one of the same type.
// Performs hot-reload by refreshing the ResourceManager cache.
void AssetLibrary::_replaceTexture(const std::string& selectedAsset, std::string& statusMessage, float& statusTimer) {
    if (selectedAsset.empty()) {
        LOG_WARNING("No file selected to replace");
        return;
    }

    // Disallow replacing folders
    {
        std::error_code ec;
        if (std::filesystem::is_directory(selectedAsset, ec) && !ec) {
            LOG_WARNING("Cannot replace a folder: " << selectedAsset);
            statusMessage = "Replace failed: selected item is a folder";
            statusTimer = 3.0f;
            return;
        }
    }

#ifdef _WIN32
    // Same as above (import textures)
    char filename[512] = "";

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof(filename);
    // Match selected asset extension in file dialog filter
    std::filesystem::path destPathBuild(selectedAsset);
    std::string ext = destPathBuild.extension().string();
    std::string upperExt = ext;
    for (auto& c : upperExt) c = (char)std::toupper((unsigned char)c);

    std::string filterDesc = upperExt.empty() ? std::string("All Files") : (upperExt.substr(1) + " Files");
    std::string filterPattern = ext.empty() ? std::string("*.*") : ("*" + ext);
    std::string filter = filterDesc + "\0" + filterPattern + "\0All Files\0*.*\0";
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

        // Also replace in source assets folder
        std::string sourceAssetsPath = selectedAsset;

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
                RM.UnloadAsset(selectedAsset);   // Remove old cached version
                RM.Get<Texture>(selectedAsset);  // Load new version into cache

                LOG_INFO("Successfully replaced asset in both locations: " << destPathBuild.filename().string());
                statusMessage = "File replaced successfully";
                statusTimer = 3.0f; // Show for 3 seconds
            }
            catch (const std::exception& e) {
                LOG_ERROR("Failed to replace texture: " << e.what());
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

// Handle a file dropped from the OS into the asset browser.
// Mirrors the import flow and selects the newly copied file.
void AssetLibrary::_handleFileDrop(const std::string& sourcePathStr, const std::string& currentPath,
    std::string& selectedAsset, std::string& statusMessage, float& statusTimer) {
    std::filesystem::path sourcePath(sourcePathStr);

    // Where to copy it: current folder + just the filename
    std::filesystem::path destPathBuild = std::filesystem::path(currentPath) / sourcePath.filename();

    // Check if path contains "assets"
    std::string sourceAssetsPath = currentPath;
    if (sourceAssetsPath.find("assets") != std::string::npos) {
        // Also copy to source assets folder
        std::filesystem::path destPathSource = std::filesystem::path("..") / currentPath / sourcePath.filename();

        try {
            // Copy to build/assets (runtime)
            std::filesystem::copy_file(sourcePath, destPathBuild, std::filesystem::copy_options::overwrite_existing);

            // Copy to ../assets (source)
            std::filesystem::create_directories(destPathSource.parent_path());
            std::filesystem::copy_file(sourcePath, destPathSource, std::filesystem::copy_options::overwrite_existing);

            LOG_INFO("Successfully imported dropped file to: " << currentPath);
            statusMessage = "File imported: " + sourcePath.filename().string();
            statusTimer = 3.0f;

            // Auto-select the newly imported file
            selectedAsset = destPathBuild.string();
        }
        catch (const std::exception& e) {
            LOG_ERROR("Failed to import dropped file: " << e.what());
            statusMessage = "Failed to import file";
            statusTimer = 3.0f;
        }
    }
}

// Delete the currently selected file or folder safely in both locations.
// Removes from build/assets and mirrors the deletion in ../assets.
void AssetLibrary::_deleteSelectedAsset(std::string& selectedAsset, std::string& statusMessage, float& statusTimer) {
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
        selectedAsset.clear();
        return;
    }
    bool isFolder = std::filesystem::is_directory(selectedPath);

    try {
        // Delete from build/assets
        if (isFolder) {
            std::filesystem::remove_all(selectedPath);  // Remove folder and contents
        }
        else {
            std::filesystem::remove(selectedPath);      // Remove file
        }
        // Also delete from source assets folder (../assets)
        std::string sourceAssetsPath = selectedAsset;
        if (sourceAssetsPath.find("assets") != std::string::npos) {
            std::filesystem::path relativePath = std::filesystem::relative(selectedPath, "assets");
            std::filesystem::path sourcePathToDelete = std::filesystem::path("..") / "assets" / relativePath;

            // Only proceed if path exists
            if (std::filesystem::exists(sourcePathToDelete)) {
                if (isFolder) {  // Remove entire folder recursively
                    std::filesystem::remove_all(sourcePathToDelete);
                }
                else {           // Remove single file
                    std::filesystem::remove(sourcePathToDelete);
                }
            }
        }
        LOG_INFO("Deleted: " << selectedPath.filename().string());
        statusMessage = "Deleted: " + selectedPath.filename().string();
        statusTimer = 3.0f;

        // Clear selection after deletion
        selectedAsset.clear();
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to delete: " << e.what());
        statusMessage = "Failed to delete item";
        statusTimer = 3.0f;
    }
}
