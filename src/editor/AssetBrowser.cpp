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

void AssetBrowser::Initialize(ImFont* symbolsFont) {
    m_symbolsFont = symbolsFont;
}

// Render the asset browser UI window
void AssetBrowser::Render() {
    UICommon::ApplyLayout(UICommon::WindowId::EDITOR_ASSET_BROWSER);
    ImGui::Begin("Asset Browser");

    // Display clickable breadcrumb navigation
    _displayBreadcrumbs();

    ImGui::SameLine();

    // Import button
    if (ImGui::Button("\xEF\x82\x9B")) {
        _importTexture();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Import PNG texture into current folder");
    }

    // Two-column layout (.x is width): file list on left, info panel on right
    float windowWidth = ImGui::GetContentRegionAvail().x;

    // Left side: File/folder list (70% width)
    // Child window = scrollable region within parent window
    ImGui::BeginChild("FileList", ImVec2(windowWidth * 0.7f, 0), true);
    _displayFolder(m_currentPath);
    ImGui::EndChild();

    ImGui::SameLine();

    // Right side: File info panel (30% width)
    // ImVec2(0, 0) = take up remaining horizontal + vertical space
    ImGui::BeginChild("FileInfo", ImVec2(0, 0), true);
    _displaySelectedFileInfo();
    ImGui::EndChild();

    ImGui::End();
}

// Display clickable breadcrumb trail
void AssetBrowser::_displayBreadcrumbs() {
    std::filesystem::path currentPath(m_currentPath);
    std::vector<std::filesystem::path> pathParts;

    // Build path parts from root to current
    for (const auto& part : currentPath) {
        std::string partStr = part.string();
        // Only add non-empty parts (skip empty strings, ".", etc.)
        if (!partStr.empty() && partStr != "." && partStr != ".." && partStr != "/" && partStr != "\\") {
            pathParts.push_back(partStr);
        }
    }

    // Display each part as clickable button
    std::string accumulatedPath;
    for (size_t i{}; i < pathParts.size(); i++) {
        // Add separator between parts
        if (i > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled(">");
            ImGui::SameLine();
        }

        // Build accumulated path up to this part
        if (i > 0) accumulatedPath += "/";
        accumulatedPath += pathParts[i].string();

        // Clickable breadcrumb button
        if (ImGui::SmallButton(pathParts[i].string().c_str())) {
            m_currentPath = accumulatedPath;
            m_selectedAsset.clear();  // Clear selection when navigating folders
        }
    }
}

// Display all files and folders in the given directory
void AssetBrowser::_displayFolder(const std::filesystem::path& folderPath) {
    // Check if path exists or if path exists but it's a file, not a folder
    if (!std::filesystem::exists(folderPath) || !std::filesystem::is_directory(folderPath)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Folder not found");
        return;
    }

    // Iterate through each item in directory (file or folder)
    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        // If item is folder
        if (entry.is_directory()) {
            // Folder icon + name
            std::string folderName = "\xEE\x8B\x87 " + entry.path().filename().string(); 

            // Clickable folder entry
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
}

// Display a single file as a selectable entry
void AssetBrowser::_displayFile(const std::filesystem::path& filePath) {
    // Extract just the filename and then check if THIS file is the currently selected one
    std::string filename = filePath.filename().string();
    bool isSelected = (m_selectedAsset == filePath.string());

    // File icon + filename
    std::string displayName = "\xEE\xA1\xB3 " + filename;

    // Selectable file entry
    if (ImGui::Selectable(displayName.c_str(), isSelected)) {
        // If clicked, remember it as selected
        m_selectedAsset = filePath.string();
    }
}

// Display information about the currently selected file
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
    }
    
    // Stop indenting text
    ImGui::Unindent();
}

// Import a new texture file into the current folder
void AssetBrowser::_importTexture() {
#ifdef _WIN32  // Only compile this code on Windows
    char filename[512] = ""; 

    // OPENFILENAMEA is a Windows struct that configures the file dialog
    OPENFILENAMEA ofn = {};                                  // Initialize all fields to 0/null
    ofn.lStructSize = sizeof(ofn);                           // Tell Windows how big this struct is
    ofn.hwndOwner = nullptr;                                 // No parent window (dialog is standalone)
    ofn.lpstrFile = filename;                                // Point to buffer where Windows will write the selected path
    ofn.nMaxFile = sizeof(filename);                         // Tell Windows max size of buffer (512 bytes)

    ofn.lpstrFilter = "PNG Files\0*.png\0All Files\0*.*\0";  // File type filters in the dropdown
    // Format: "Display Name\0*.extension\0" (null-separated strings)

    ofn.nFilterIndex = 1;                                    // Start with first filter selected (PNG Files)
    ofn.lpstrTitle = "Select PNG Texture to Import";         // Dialog window title

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
