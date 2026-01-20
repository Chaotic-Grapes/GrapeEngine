/* Start Header *****************************************************************/
/*!
\file   ConsolePanel.cpp
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   20th November 2025

\brief
Implements the ConsolePanel class which displays all LOG_* messages in the editor.
Provides real-time log viewing with filtering, search, and auto-scroll capabilities.
Messages are captured through Logger integration and displayed with timestamps and
color-coded severity levels.
*/
/* End Header *******************************************************************/

#include "ConsolePanel.h"
#include <algorithm>
#include <cstring>
#include "EditorStyle.h"

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

void ConsolePanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;

    // Clear any messages that accumulated before console was initialized
    Clear();

    m_initialized = true;
}

void ConsolePanel::Shutdown() {
    // Mark as uninitialized to prevent any new messages
    m_initialized = false;

    // Clear callback to prevent further invocations during shutdown
    Logger::Get().SetConsoleCallback(nullptr);

    // Clear messages
    std::lock_guard<std::mutex> lock(m_messagesMutex);
    m_messages.clear();
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

void ConsolePanel::Render() {
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Console");

    _renderToolbar();
    ImGui::Separator();
    _renderMessages();

    ImGui::End();
    ImGui::PopFont();
}

void ConsolePanel::_renderToolbar() {
    // Clear button
    if (ImGui::Button("Clear")) {
        Clear();
    }

    ImGui::SameLine();

    // Auto-scroll toggle
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::SameLine();

    // Message count
    ImGui::Text("| Messages: %zu", m_messages.size());

    // Filter toggles - only show WRN/ERR/CRT since INFO/DEBUG are filtered at source
    ImGui::Separator();
    ImGui::Text(" Filter:");
    ImGui::SameLine();

    if (ImGui::Checkbox("WRN", &m_showWarning)) { m_filterDirty = true; }
    ImGui::SameLine();
    if (ImGui::Checkbox("ERR", &m_showError)) { m_filterDirty = true; }
    ImGui::SameLine();
    if (ImGui::Checkbox("CRT", &m_showCritical)) { m_filterDirty = true; }

    // Search bar
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search messages...", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_filterDirty = true;
    }
}

void ConsolePanel::_renderMessages() {
    // Use child region for scrolling
    ImGui::BeginChild("MessageList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Display messages with thread safety
    std::lock_guard<std::mutex> lock(m_messagesMutex);

    // Detect changes that require rebuilding the filtered view
    if (m_lastMessageCount != m_messages.size()) {
        m_filterDirty = true;
        m_lastMessageCount = m_messages.size();
    }

    if (m_lastShowWarning != m_showWarning || m_lastShowError != m_showError || m_lastShowCritical != m_showCritical) {
        m_filterDirty = true;
        m_lastShowWarning = m_showWarning;
        m_lastShowError = m_showError;
        m_lastShowCritical = m_showCritical;
    }

    // Cheap rolling hash for search buffer
    uint32_t searchHash = 2166136261u;
    for (const char* p = m_searchBuffer; *p; ++p) {
        searchHash ^= static_cast<uint8_t>(*p);
        searchHash *= 16777619u;
    }
    if (m_lastSearchHash != searchHash) {
        m_filterDirty = true;
        m_lastSearchHash = searchHash;
    }

    if (m_filterDirty) {
        m_filteredIndices.clear();
        m_filteredIndices.reserve(m_messages.size());
        for (size_t i = 0; i < m_messages.size(); ++i) {
            const auto& msg = m_messages[i];
            if (_shouldDisplayMessage(msg)) {
                m_filteredIndices.push_back(static_cast<int>(i));
            }
        }
        m_filterDirty = false;
    }

    // Render only visible messages
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_filteredIndices.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const int msgIndex = m_filteredIndices[static_cast<size_t>(row)];
            const auto& msg = m_messages[static_cast<size_t>(msgIndex)];
            _renderMessage(msg, msgIndex);
        }
    }
    // Auto-scroll to bottom
    if (m_autoScroll && m_scrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBottom = false;
    }

    ImGui::EndChild();
}

void ConsolePanel::_renderMessage(const ConsoleMessage& msg, int index) {
    // Determine background color (selected > hovered > alternating)
    bool isSelected = (index == m_selectedMessageIndex);
    bool isHovered = (index == m_hoveredMessageIndex);
    
    ImVec4 bgColor;
    if (isSelected) {
        // stronger selection tint
        bgColor = EditorStyle::Scale(EditorStyle::Selection, 1.4f);
    }
    else if (isHovered) {
        // subtle hover
        bgColor = EditorStyle::Scale(EditorStyle::FrameBgHover, 0.5f);
    }
    else if (index % 2 == 0) {
        // alternating row
        bgColor = EditorStyle::Scale(EditorStyle::FrameBg, 0.6f);
    }
    else {
        bgColor = EditorStyle::Transparent;
    }
    
    // Calculate text height for wrapping
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float timestampWidth = ImGui::CalcTextSize(msg.Timestamp.c_str()).x;
    float levelBadgeWidth = ImGui::CalcTextSize("[XXX]").x;
    float messageWidth = availableWidth - timestampWidth - levelBadgeWidth - ImGui::GetStyle().ItemSpacing.x * 2;
    
    ImVec2 messageSize = ImGui::CalcTextSize(msg.Content.c_str(), nullptr, false, messageWidth);
    float messageHeight = messageSize.y + ImGui::GetStyle().ItemSpacing.y * 2;
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
    ImGui::BeginChild(ImGui::GetID((void*)(intptr_t)index), ImVec2(0, messageHeight), false);

    // Timestamp
    ImGui::TextDisabled("%s", msg.Timestamp.c_str());
    ImGui::SameLine();

    // Level badge with color
    ImVec4 levelColor = _getColorForLevel(msg.Level);
    ImGui::PushStyleColor(ImGuiCol_Text, levelColor);
    ImGui::Text("[%s]", _getLevelText(msg.Level));
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // Message content
    ImGui::TextWrapped("%s", msg.Content.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    
    // Handle hover detection
    if (ImGui::IsItemHovered()) {
        m_hoveredMessageIndex = index;
        
        // Handle left click for selection
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_selectedMessageIndex = index;
        }
    }
    else if (m_hoveredMessageIndex == index) {
        m_hoveredMessageIndex = -1;
    }
    
    // Handle right-click context menu
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_selectedMessageIndex = index;
        ImGui::OpenPopup("MessageContextMenu");
    }
    
    // Render context menu (only if this is the selected message)
    if (isSelected && ImGui::BeginPopup("MessageContextMenu")) {
        if (ImGui::MenuItem("Copy")) {
            // Format the full message text for clipboard
            std::string fullMessage = msg.Timestamp + " [" + _getLevelText(msg.Level) + "] " + msg.Content;
            ImGui::SetClipboardText(fullMessage.c_str());
        }
        ImGui::EndPopup();
    }
}

// -------------------------------------------------------------------------
// Message Management
// -------------------------------------------------------------------------

void ConsolePanel::AddMessage(LogLevel level, LogSource source, const std::string& timestamp, const std::string& message) {
    // Don't process messages if panel isn't initialized or is shutting down
    // Check this before any other operations to avoid race conditions
    if (!m_initialized) {
        return;
    }

    // Filter: Only store warnings/errors/critical from any source,
    // OR info/debug from SCRIPT source only
    if (level == LogLevel::TRACE) {
        return; // Never show TRACE in console
    }

    if ((level == LogLevel::INFO || level == LogLevel::DEBUG) && source != LogSource::SCRIPT) {
        return; // Only show INFO/DEBUG from scripts, not engine
    }

    // Thread-safe message insertion
    std::lock_guard<std::mutex> lock(m_messagesMutex);

    // Limit message buffer size
    if (m_messages.size() >= MAX_MESSAGES) {
        m_messages.erase(m_messages.begin());
    }

    m_messages.push_back({ timestamp, level, message });
    m_filterDirty = true;
    m_scrollToBottom = true;
}

void ConsolePanel::Clear() {
    std::lock_guard<std::mutex> lock(m_messagesMutex);
    m_messages.clear();
    m_filteredIndices.clear();
    m_filterDirty = true;
    m_lastMessageCount = 0;
    m_selectedMessageIndex = -1;
    m_hoveredMessageIndex = -1;
}

// -------------------------------------------------------------------------
// Filtering
// -------------------------------------------------------------------------

bool ConsolePanel::_shouldDisplayMessage(const ConsoleMessage& msg) const {
    // Filter by level checkboxes
    switch (msg.Level) {
    case LogLevel::INFO:     if (!m_showInfo) return false; break;
    case LogLevel::DEBUG:    if (!m_showDebug) return false; break;
    case LogLevel::WARNING:  if (!m_showWarning) return false; break;
    case LogLevel::ERROR:    if (!m_showError) return false; break;
    case LogLevel::CRITICAL: if (!m_showCritical) return false; break;
    default: return false; // TRACE or unknown
    }

    // Check search filter
    if (m_searchBuffer[0] != '\0') {
        std::string search = m_searchBuffer;
        std::string content = msg.Content;

        // Case-insensitive search
        std::transform(search.begin(), search.end(), search.begin(), ::tolower);
        std::transform(content.begin(), content.end(), content.begin(), ::tolower);

        if (content.find(search) == std::string::npos) {
            return false;
        }
    }

    return true;
}

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

ImVec4 ConsolePanel::_getColorForLevel(LogLevel level) const {
    switch (level) {
    case LogLevel::INFO:     return EditorStyle::LogInfo;
    case LogLevel::DEBUG:    return EditorStyle::LogDebug;
    case LogLevel::WARNING:  return EditorStyle::LogWarning;
    case LogLevel::ERROR:    return EditorStyle::DangerText;
    case LogLevel::CRITICAL: return EditorStyle::LogCritical;
    default:                 return EditorStyle::Text;
    }
}

// const char* ConsolePanel::_getLevelIcon(LogLevel level) const {
//     switch (level) {
//     case LogLevel::WARNING:  return "\xEE\x80\x82";
//     case LogLevel::ERROR:    return "\xEE\x80\x80";
//     case LogLevel::CRITICAL: return "\xEE\xA2\x9A";
//     default:                 return "?";
//     }
// }

const char* ConsolePanel::_getLevelText(LogLevel level) const {
    switch (level) {
    case LogLevel::INFO:     return "INF";
    case LogLevel::DEBUG:    return "DBG";
    case LogLevel::WARNING:  return "WRN";
    case LogLevel::ERROR:    return "ERR";
    case LogLevel::CRITICAL: return "CRT";
    default:                 return "???";
    }
}
