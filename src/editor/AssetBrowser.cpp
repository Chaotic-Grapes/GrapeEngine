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

#include "../editor/AssetBrowser.h"
#include "services/UICommon.h"
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
    ImGui::Separator();

    // Show all files/folders in current path
    _displayFolder(m_currentPath);
    ImGui::Separator();

    // Show selected file info
    _displaySelectedFileInfo();

    ImGui::End();
}

// Display clickable breadcrumb trail
void AssetBrowser::_displayBreadcrumbs() {
    std::filesystem::path currentPath(m_currentPath);
    std::vector<std::filesystem::path> pathParts;

    // Build path parts from root to current
    for (const auto& part : currentPath) {
        pathParts.push_back(part);
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
            std::string folderName = "\xEE\x8B\x87 " + entry.path().filename().string();  // folder_open icon

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
