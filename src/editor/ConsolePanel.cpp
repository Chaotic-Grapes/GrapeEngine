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

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

void ConsolePanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;
    m_boldFont = boldFont;
    m_symbolsFont = symbolsFont;

    // Clear any messages that accumulated before console was initialized
    Clear();
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

    // Filter toggles
    ImGui::Separator();
    ImGui::Text(" Filter:");
    ImGui::SameLine();

    if (ImGui::Checkbox("WRN", &m_showWarning)) {}
    ImGui::SameLine();
    if (ImGui::Checkbox("ERR", &m_showError)) {}
    ImGui::SameLine();
    if (ImGui::Checkbox("CRT", &m_showCritical)) {}

    // Search bar
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search messages...", m_searchBuffer, sizeof(m_searchBuffer));
}

void ConsolePanel::_renderMessages() {
    // Use child region for scrolling
    ImGui::BeginChild("MessageList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Display messages
    int displayedCount = 0;
    for (size_t i = 0; i < m_messages.size(); ++i) {
        const auto& msg = m_messages[i];

        if (_shouldDisplayMessage(msg)) {
            _renderMessage(msg, displayedCount);
            displayedCount++;
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
    // Alternating background colors for readability
    if (index % 2 == 0) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.3f));
    }
    else {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    ImGui::BeginChild(ImGui::GetID((void*)(intptr_t)index), ImVec2(0, ImGui::GetTextLineHeightWithSpacing()), false);

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
}

// -------------------------------------------------------------------------
// Message Management
// -------------------------------------------------------------------------

void ConsolePanel::AddMessage(LogLevel level, const std::string& timestamp, const std::string& message) {
    // Only store warnings, errors, and critical messages
    if (level == LogLevel::TRACE || level == LogLevel::INFO || level == LogLevel::DEBUG) {
        return; // Don't even add these to the buffer
    }

    // Limit message buffer size
    if (m_messages.size() >= MAX_MESSAGES) {
        m_messages.erase(m_messages.begin());
    }

    m_messages.push_back({ timestamp, level, message });
    m_scrollToBottom = true;
}

void ConsolePanel::Clear() {
    m_messages.clear();
}

// -------------------------------------------------------------------------
// Filtering
// -------------------------------------------------------------------------

bool ConsolePanel::_shouldDisplayMessage(const ConsoleMessage& msg) const {
    // Filter by level checkboxes (INFO/DEBUG/TRACE are already excluded at AddMessage)
    switch (msg.Level) {
    case LogLevel::WARNING:  if (!m_showWarning) return false; break;
    case LogLevel::ERROR:    if (!m_showError) return false; break;
    case LogLevel::CRITICAL: if (!m_showCritical) return false; break;
    default: return false; // Shouldn't happen, but just in case
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
    case LogLevel::WARNING:  return ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow
    case LogLevel::ERROR:    return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Red
    case LogLevel::CRITICAL: return ImVec4(1.0f, 0.0f, 1.0f, 1.0f);  // Magenta
    default:                 return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

const char* ConsolePanel::_getLevelIcon(LogLevel level) const {
    ImGui::PushFont(m_symbolsFont);
    switch (level) {
    case LogLevel::WARNING:  return "\xEE\x80\x82";
    case LogLevel::ERROR:    return "\xEE\x80\x80";
    case LogLevel::CRITICAL: return "\xEE\xA2\x9A";
    default:                 return "?";
    }
    ImGui::PopFont();
}

const char* ConsolePanel::_getLevelText(LogLevel level) const {
    switch (level) {
    case LogLevel::WARNING:  return "WRN";
    case LogLevel::ERROR:    return "ERR";
    case LogLevel::CRITICAL: return "CRT";
    default:                 return "???";
    }
}
