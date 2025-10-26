/* Start Header *****************************************************************/
/*!
\file   AssetBrowser.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   26th October 2025
\brief
Implements the AssetBrowser class for browsing and managing game assets.

Features:
- File browser with folder navigation
- File selection
- Display of assets folder structure
*/
/* End Header *******************************************************************/

#include "../editor/AssetBrowser.h"
#include "services/UICommon.h"
#include <imgui.h>

AssetBrowser::AssetBrowser() {}

AssetBrowser::~AssetBrowser() {}

void AssetBrowser::Initialize(ImFont* symbolsFont) {
    m_symbolsFont = symbolsFont;
}

// Render the asset browser UI window
void AssetBrowser::Render() {
    UICommon::ApplyLayout(UICommon::WindowId::EDITOR_ASSET_BROWSER);
    ImGui::SetWindowFontScale(0.8f);
    ImGui::Begin("Asset Browser");

    // Show current path
    ImGui::Text("Path: %s", m_currentPath.c_str());
    ImGui::Separator();

    // "Up" button: goes to parent folder (only if not at root)
    if (m_currentPath != m_assetsRootPath) {
        if (ImGui::Button(".. (Up)")) {
            std::filesystem::path p(m_currentPath);
            m_currentPath = p.parent_path().string();  // Go up one folder
        }
    }

    // Show all files/folders in current path
    _displayFolder(m_currentPath);

    ImGui::End();
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
            // Folder: clickable to navigate
            std::string folderName = "\xEE\x8B\x87 " + entry.path().filename().string();
            if (ImGui::Selectable(folderName.c_str())) {
                // If clicked, then navigate into it by changing current path
                m_currentPath = entry.path().string();
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
    std::string displayName = "\xEE\xA1\xB3 " + filePath.filename().string();
    bool isSelected = (m_selectedAsset == filePath.string());

    // Selectable file entry
    if (ImGui::Selectable(displayName.c_str(), isSelected)) {
        // If clicked, remember it as selected
        m_selectedAsset = filePath.string();
    }
}
