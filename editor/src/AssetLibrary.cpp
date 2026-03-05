/* Start Header *****************************************************************/
/*!
\file   AssetLibrary.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   11th January 2026

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
#include "EditorStyle.h"
#include "EditorIcons.h"
#include <algorithm>
#include <cctype>

// Helper functions
namespace {
	// Normalize a path to a consistent format for comparison
    std::filesystem::path NormalizePath(const std::filesystem::path& input) {
		// weakly_canonical resolves symlinks and normalizes path components
        std::error_code ec;
        std::filesystem::path normalized = std::filesystem::weakly_canonical(input, ec);

        // But can fail if the path doesn't exist
        if (ec) {
            ec.clear();
			// If weakly_canonical fails, we can still try absolute to resolve relative components
            normalized = std::filesystem::absolute(input, ec);
        }
		// If absolute also fails (e.g. path doesn't exist), we fall back to the original input
        if (ec) {
            return input.lexically_normal();
        }
		// Finally, we apply lexically_normal to clean up any remaining redundant components (like "a/../b" -> "b")
        return normalized.lexically_normal();
    }

	// Compare two path components for equality, ignoring case on Windows
    bool PathComponentEquals(const std::filesystem::path& a, const std::filesystem::path& b) {
#ifdef _WIN32
		// On Windows, paths are case-insensitive, so we compare them in lowercase
        std::string lhs = a.string();
        std::string rhs = b.string();

		// Convert both strings to lowercase for case-insensitive comparison
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		// Finally, compare the normalized lowercase strings
        return lhs == rhs;
#else
		// On Unix-like systems, paths are case-sensitive, so we can compare them directly
        return a == b;
#endif
    }

	// Check if a given path is inside the root directory, accounting for normalization and case sensitivity
    bool IsPathInsideRoot(const std::filesystem::path& path, const std::filesystem::path& root) {
		// Normalize both paths to ensure consistent formatting (resolving symlinks, relative components, etc.)
        const std::filesystem::path normalizedPath = NormalizePath(path);
        const std::filesystem::path normalizedRoot = NormalizePath(root);

		// Now we can compare the normalized paths component by component
        auto pathIt = normalizedPath.begin();
        auto rootIt = normalizedRoot.begin();

		// Iterate through each component of the root path and compare with the corresponding component in the input path
        for (; rootIt != normalizedRoot.end(); rootIt++, pathIt++) {
			// If the input path has fewer components than the root, it can't be inside the root
            if (pathIt == normalizedPath.end()) {
                return false;
            }
			// Compare the current components of both paths, using case-insensitive comparison on Windows
            if (!PathComponentEquals(*pathIt, *rootIt)) {
                return false;
            }
        }
		// If we successfully compared all components of the root path, then the input path is inside the root
        return true;
    }
}

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
            ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Accent);
            ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::Transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::Scale(EditorStyle::AccentHover, 0.3f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::Scale(EditorStyle::AccentActive, 0.5f));

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
        ImGui::TextColored(EditorStyle::DangerText, "Folder not found");
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
            ImGui::Text(EditorIcons::Folder);
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
    ImGui::Text(EditorIcons::File);
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
        ImGui::Text(EditorIcons::Drag);
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
        ImGui::TextColored(EditorStyle::DangerText, "Selected file no longer exists");
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
            ImGui::TextColored(EditorStyle::WarningText, "Could not get file size");
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
		// Call shared import logic to copy file and update status
        _importFromSourcePath(sourcePath, currentPath, selectedAsset, statusMessage, statusTimer);
    }
    // User clicked cancel
    else {
        LOG_INFO("Import cancelled by user");
    }

#else  // Not Windows (Linux, Mac, etc.)
    LOG_WARNING("File dialog not implemented for this platform");
#endif
}

// Shared logic for importing a file from a source path (used by both file dialog and drag-drop)
void AssetLibrary::_importFromSourcePath(const std::filesystem::path& sourcePath, const std::string& currentPath,
    std::string& selectedAsset, std::string& statusMessage, float& statusTimer) 
{
	// Validate source file exists, destination folder exists and destination is inside project
    try {
		// Check if source file exists
        if (!std::filesystem::exists(sourcePath)) {
            statusMessage = "Failed to import: source not found";
            statusTimer = 3.0f;
            LOG_ERROR("Import failed, source not found: " << sourcePath.string());
            return;
        }

		// Check if destination folder exists and is a directory
        const std::filesystem::path destinationDir(currentPath);
        if (!std::filesystem::exists(destinationDir) || !std::filesystem::is_directory(destinationDir)) {
            statusMessage = "Failed to import: destination folder not found";
            statusTimer = 3.0f;
            LOG_ERROR("Import failed, invalid destination folder: " << currentPath);
            return;
        }

		// Ensure destination is inside project root to prevent copying files from arbitrary locations on disk
        const std::filesystem::path projectRoot(Engine::ProjectPaths::GetProjectRoot());
        if (projectRoot.empty()) {
            statusMessage = "Failed to import: project root unavailable";
            statusTimer = 3.0f;
            LOG_ERROR("Import failed, project root unavailable");
            return;
        }

		// Check if destination is inside project root (with normalization and case-insensitive comparison)
        if (!IsPathInsideRoot(destinationDir, projectRoot)) {
            statusMessage = "Failed to import: destination outside project";
            statusTimer = 3.0f;
            LOG_ERROR("Import failed, destination is outside project root. Destination = " << destinationDir.string() << ", root = " << projectRoot.string());
            return;
        }

		// Resolve name conflicts: if a file with the same name already exists in the destination, append a numeric suffix to the filename
        std::filesystem::path destinationPath = destinationDir / sourcePath.filename();

		// If the destination file already exists, we need to find a unique name by appending a suffix (e.g. "file.png" -> "file_1.png")
        if (std::filesystem::exists(destinationPath)) {
			// Determine base name and extension for suffixing
            const bool sourceIsDirectory = std::filesystem::is_directory(sourcePath);
            const std::string baseName = sourceIsDirectory ? destinationPath.filename().string() : destinationPath.stem().string();
            const std::string extension = sourceIsDirectory ? std::string() : destinationPath.extension().string();

			// Start with suffix 1 and increment until we find a name that doesn't exist
            int suffix = 1;

			// Loop to find a unique filename by appending "_1", "_2", etc. before the extension until we find a name that doesn't exist
            do {
                destinationPath = destinationDir / (baseName + "_" + std::to_string(suffix) + extension);
                suffix++;
            } while (std::filesystem::exists(destinationPath));
        }

		// Use RM to copy the file into the project (handles both ../assets and build/assets)
        if (RM.AddAsset(sourcePath.string(), destinationPath.string())) {
            selectedAsset = destinationPath.string();
            statusMessage = "File imported: " + destinationPath.filename().string();
            statusTimer = 3.0f;
            LOG_INFO("Imported file to current folder: source = " << sourcePath.string() << ", destination = " << destinationPath.string());
            return;
        }

		// If we reach this point, the import failed for some reason (e.g. copy error)
        statusMessage = "Failed to import file";
        statusTimer = 3.0f;
        LOG_ERROR("Import failed via ResourceManager: source = " << sourcePath.string() << ", destination = " << destinationPath.string());
    }
	// Catch any exceptions that might occur during filesystem operations and log them
    catch (const std::exception& e) {
        statusMessage = "Failed to import file";
        statusTimer = 3.0f;
        LOG_ERROR("Import failed with exception: " << e.what());
    }
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
            // Use ResourceManager to replace the asset (handles unload + reload automatically)
            if (RM.ReplaceAsset(selectedAsset, sourcePath.string())) {
                LOG_INFO("Successfully replaced asset: " << destPathBuild.filename().string());
                statusMessage = "File replaced: " + destPathBuild.filename().string();
                statusTimer = 3.0f;
            }
            else {
                statusMessage = "Failed to replace file";
                statusTimer = 3.0f;
            }
        }
        else {
            LOG_INFO("Replace cancelled by user");
        }
    }

#else
    LOG_WARNING("File dialog not implemented for this platform");
#endif
    }

// Handle a file dropped from the OS into the asset browser
// Mirrors the import flow and selects the newly copied file
void AssetLibrary::_handleFileDrop(const std::string & sourcePathStr, const std::string & currentPath,
    std::string & selectedAsset, std::string & statusMessage, float& statusTimer)
{
    std::filesystem::path sourcePath(sourcePathStr);
	// The logic for handling a file dropped from the OS is basically the same as the import flow triggered by the file dialog, 
    // except we already have the source path and don't need to show a dialog
    _importFromSourcePath(sourcePath, currentPath, selectedAsset, statusMessage, statusTimer);
}

// Delete the currently selected file or folder safely in both locations
// Removes from build/assets and mirrors the deletion in ../assets
void AssetLibrary::_deleteSelectedAsset(std::string & selectedAsset, std::string & statusMessage,
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
    // bool isFolder = std::filesystem::is_directory(selectedPath);

    // Only operate when inside the project's Assets folder
    std::string assetsRoot = Engine::ProjectPaths::GetAssetsPath();
    auto absSel = std::filesystem::absolute(selectedAsset).string();
    auto absAssets = std::filesystem::absolute(assetsRoot).string();
    if (absSel.find(absAssets) != std::string::npos) {
        // Use ResourceManager to delete the asset (handles cache cleanup automatically)
        if (RM.DeleteAsset(selectedAsset)) {
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
