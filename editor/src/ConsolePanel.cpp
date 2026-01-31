/* Start Header *****************************************************************/
/*!
\file   ConsolePanel.cpp
\author Foo Rui Qin (70%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
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
#include <sstream>
#include <cctype>
#include "EditorStyle.h"

#ifdef max
#undef max
#endif

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
    m_pendingMessages.clear();
    m_selectedMessageIds.clear();
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

void ConsolePanel::Render() {
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Console");

    std::vector<ConsoleMessage> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        snapshot = m_messages;
        m_lastPendingCount = m_pendingMessages.size();
    }

    _rebuildFilterCache(snapshot);
    _renderToolbar(snapshot);
    if (m_filterDirty) {
        _rebuildFilterCache(snapshot);
    }
    ImGui::Separator();
    _renderMessages(snapshot);

    ImGui::End();
    ImGui::PopFont();
}

void ConsolePanel::_renderToolbar(const std::vector<ConsoleMessage>& snapshot) {
    // Clear button
    if (ImGui::Button("Clear")) {
        Clear();
    }

    ImGui::SameLine();

    // Pause logging toggle
    const bool paused = m_paused.load();
    if (ImGui::Button(paused ? "Resume" : "Pause")) {
        m_paused.store(!paused);
        if (paused) {
            _flushPendingMessages();
        }
    }

    ImGui::SameLine();

    // Auto-scroll toggle
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::SameLine();

    // Message count
    ImGui::Text("| Messages: %zu", snapshot.size());

    if (m_paused.load() && m_lastPendingCount > 0) {
        ImGui::SameLine();
        ImGui::Text("| Queued: %zu", m_lastPendingCount);
    }

    ImGui::SameLine();
    ImGui::Text("| Filtered: %zu / %zu", m_filteredIndices.size(), m_totalEligibleCount);

    ImGui::SameLine();
    if (ImGui::Button("Options")) {
        ImGui::OpenPopup("ConsoleOptions");
    }

    if (ImGui::BeginPopup("ConsoleOptions")) {
        if (ImGui::MenuItem("Copy all")) {
            _copyMessagesToClipboard(snapshot, nullptr, false, true, true);
        }
        if (ImGui::MenuItem("Copy filtered")) {
            _copyMessagesToClipboard(snapshot, &m_filteredIndices, false, true, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear")) {
            Clear();
        }
        ImGui::Checkbox("Clear on play/build", &m_clearOnPlayBuild);
        ImGui::EndPopup();
    }

    // Filter toggles
    ImGui::Separator();
    ImGui::Text("Levels:");
    ImGui::SameLine();

    if (_renderLevelChip("##level_inf", "INF", EditorStyle::LogInfo, m_showInfo, m_levelCounts[0])) {
        m_showInfo = !m_showInfo;
        m_filterDirty = true;
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_dbg", "DBG", EditorStyle::LogDebug, m_showDebug, m_levelCounts[1])) {
        m_showDebug = !m_showDebug;
        m_filterDirty = true;
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_wrn", "WRN", EditorStyle::LogWarning, m_showWarning, m_levelCounts[2])) {
        m_showWarning = !m_showWarning;
        m_filterDirty = true;
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_err", "ERR", EditorStyle::DangerText, m_showError, m_levelCounts[3])) {
        m_showError = !m_showError;
        m_filterDirty = true;
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_crt", "CRT", EditorStyle::LogCritical, m_showCritical, m_levelCounts[4])) {
        m_showCritical = !m_showCritical;
        m_filterDirty = true;
    }

    ImGui::SameLine();
    ImGui::Checkbox("Collapse", &m_collapseRepeated);
    if (m_lastCollapseRepeated != m_collapseRepeated) {
        m_filterDirty = true;
        m_lastCollapseRepeated = m_collapseRepeated;
    }

    ImGui::SameLine();
    ImGui::Text("Source:");
    ImGui::SameLine();

    const char* sourceOptions[] = { "All", "Engine", "Script" };
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo("##source_filter", &m_sourceFilter, sourceOptions, IM_ARRAYSIZE(sourceOptions))) {
        m_filterDirty = true;
    }

    // Search bar
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##search", "Search messages...", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_filterDirty = true;
    }
}

void ConsolePanel::_renderMessages(const std::vector<ConsoleMessage>& snapshot) {
    // Use child region for scrolling
    ImGui::BeginChild("MessageList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        m_selectedMessageIds.clear();

        for (const auto& row : m_renderRows) {
            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];

                if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {
                    m_selectedMessageIds.insert(snapshot[static_cast<size_t>(msgIndex)].Id);
                }
            }
        }
        if (!m_renderRows.empty()) {
            m_lastSelectedRow = static_cast<int>(m_renderRows.size() - 1);
        }
    }

    const bool wasAtBottom = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX;
    if (ImGui::BeginTable("ConsoleTable", 3, tableFlags)) {
        const float timeWidth = 90.0f;
        const float badgeMinWidth = ImGui::CalcTextSize("CRT").x + 25.0f;
        const float levelWidth = badgeMinWidth + ImGui::GetStyle().CellPadding.x * 2.0f + 6.0f;

        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, timeWidth);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, levelWidth);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

        float messageWrapWidth = ImGui::GetContentRegionAvail().x - timeWidth - levelWidth - ImGui::GetStyle().CellPadding.x * 4.0f;
        messageWrapWidth = (std::max)(1.0f, messageWrapWidth);

        for (int rowIndex = 0; rowIndex < static_cast<int>(m_renderRows.size()); ++rowIndex) {
            _renderMessageRow(snapshot, m_renderRows[static_cast<size_t>(rowIndex)], rowIndex, messageWrapWidth, badgeMinWidth);
        }

        ImGui::EndTable();
    }

    // Auto-scroll to bottom only if user was already at bottom
    if (m_autoScroll && m_scrollToBottom && wasAtBottom) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBottom = false;
    }

    ImGui::EndChild();
}

void ConsolePanel::_renderMessageRow(const std::vector<ConsoleMessage>& snapshot, const RenderRow& row, int rowIndex, float messageWrapWidth, float badgeMinWidth) {
    if (row.firstFilteredIndex < 0 || row.firstFilteredIndex >= static_cast<int>(m_filteredIndices.size())) {
        return;
    }

    const int displayMessageIndex = m_filteredIndices[static_cast<size_t>(row.lastFilteredIndex)];
    if (displayMessageIndex < 0 || displayMessageIndex >= static_cast<int>(snapshot.size())) {
        return;
    }

    const ConsoleMessage& msg = snapshot[static_cast<size_t>(displayMessageIndex)];

    bool rowSelected = false;
    for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
        const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];
        if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {
            if (m_selectedMessageIds.count(snapshot[static_cast<size_t>(msgIndex)].Id) > 0) {
                rowSelected = true;
                break;
            }
        }
    }

    const ImVec2 messageSize = ImGui::CalcTextSize(msg.Content.c_str(), nullptr, false, messageWrapWidth);
    const float rowHeight = std::max(ImGui::GetTextLineHeightWithSpacing(), messageSize.y) + ImGui::GetStyle().CellPadding.y * 2.0f;

    ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
    ImGui::TableSetColumnIndex(0);

    const ImVec2 rowStart = ImGui::GetCursorPos();
    ImGui::PushID(static_cast<int>(msg.Id));
    ImGui::PushStyleColor(ImGuiCol_Header, EditorStyle::Transparent);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorStyle::Transparent);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorStyle::Transparent);
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, rowHeight));

    ImGui::PopStyleColor(3);
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    if (clicked || rightClicked) {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        const bool shift = ImGui::GetIO().KeyShift;

        if (shift && m_lastSelectedRow >= 0) {
            const int start = std::min(m_lastSelectedRow, rowIndex);
            const int end = std::max(m_lastSelectedRow, rowIndex);

            if (!ctrl) {
                m_selectedMessageIds.clear();
            }

            for (int i = start; i <= end; ++i) {
                const RenderRow& rangeRow = m_renderRows[static_cast<size_t>(i)];

                for (int j = rangeRow.firstFilteredIndex; j <= rangeRow.lastFilteredIndex; ++j) {
                    const int msgIndex = m_filteredIndices[static_cast<size_t>(j)];

                    if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {
                        m_selectedMessageIds.insert(snapshot[static_cast<size_t>(msgIndex)].Id);
                    }
                }
            }
        }
        else if (ctrl) {
            bool anySelected = false;

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];

                if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {

                    if (m_selectedMessageIds.count(snapshot[static_cast<size_t>(msgIndex)].Id) > 0) {
                        anySelected = true;
                        break;
                    }
                }
            }

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];

                if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {
                    const uint64_t id = snapshot[static_cast<size_t>(msgIndex)].Id;

                    if (anySelected) {
                        m_selectedMessageIds.erase(id);
                    }
                    else {
                        m_selectedMessageIds.insert(id);
                    }
                }
            }
        }
        else {
            m_selectedMessageIds.clear();

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];

                if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {
                    m_selectedMessageIds.insert(snapshot[static_cast<size_t>(msgIndex)].Id);
                }
            }
        }

        m_lastSelectedRow = rowIndex;
    }

    if (clicked || rightClicked) {
        rowSelected = false;

        for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
            const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];

            if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {
                if (m_selectedMessageIds.count(snapshot[static_cast<size_t>(msgIndex)].Id) > 0) {
                    rowSelected = true;
                    break;
                }
            }
        }
    }

    ImGui::PushID(static_cast<int>(msg.Id));
    if (ImGui::BeginPopupContextItem("RowContext")) {
        if (ImGui::MenuItem("Copy")) {
            std::vector<int> rowIndices;
            rowIndices.reserve(static_cast<size_t>(row.count));

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                rowIndices.push_back(m_filteredIndices[static_cast<size_t>(i)]);
            }
            _copyMessagesToClipboard(snapshot, &rowIndices, false, true, true);
        }
        if (ImGui::MenuItem("Copy message only")) {
            std::vector<int> rowIndices;
            rowIndices.reserve(static_cast<size_t>(row.count));

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                rowIndices.push_back(m_filteredIndices[static_cast<size_t>(i)]);
            }
            _copyMessagesToClipboard(snapshot, &rowIndices, true, false, false);
        }
        if (ImGui::MenuItem("Copy with timestamp")) {
            std::vector<int> rowIndices;
            rowIndices.reserve(static_cast<size_t>(row.count));

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                rowIndices.push_back(m_filteredIndices[static_cast<size_t>(i)]);
            }
            _copyMessagesToClipboard(snapshot, &rowIndices, false, true, false);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Filter by level")) {
            m_showInfo = m_showDebug = m_showWarning = m_showError = m_showCritical = false;

            switch (msg.Level) {
            case LogLevel::INFO: m_showInfo = true; break;
            case LogLevel::DEBUG: m_showDebug = true; break;
            case LogLevel::WARNING: m_showWarning = true; break;
            case LogLevel::ERROR: m_showError = true; break;
            case LogLevel::CRITICAL: m_showCritical = true; break;
            default: break;
            }

            m_filterDirty = true;
        }
        if (ImGui::MenuItem("Filter by source")) {
            m_sourceFilter = (msg.Source == LogSource::ENGINE) ? 1 : 2;
            m_filterDirty = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImVec4 bgColor = EditorStyle::Transparent;
    if (rowSelected) {
        bgColor = EditorStyle::Scale(EditorStyle::Selection, 1.4f);
    }
    else if (hovered) {
        bgColor = EditorStyle::Scale(EditorStyle::FrameBgHover, 0.5f);
    }
    else if (rowIndex % 2 == 0) {
        bgColor = EditorStyle::Scale(EditorStyle::FrameBg, 0.6f);
    }

    if (!m_searchLower.empty()) {
        bgColor = ImVec4(
            std::min(1.0f, bgColor.x + 0.08f),
            std::min(1.0f, bgColor.y + 0.08f),
            std::min(1.0f, bgColor.z + 0.08f),
            bgColor.w + 0.08f
        );
    }

    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(bgColor));

    ImGui::SetCursorPos(rowStart);
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", msg.Timestamp.c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::PushFont(m_boldFont ? m_boldFont : m_mainFont);
    _drawPill(_getLevelText(msg.Level), _getBadgeColorForLevel(msg.Level), EditorStyle::Text, badgeMinWidth);
    ImGui::PopFont();

    ImGui::TableSetColumnIndex(2);
    ImGui::PushStyleColor(ImGuiCol_Text, _getColorForLevel(msg.Level));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + messageWrapWidth);
    ImGui::TextUnformatted(msg.Content.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    if (row.count > 1) {
        ImGui::SameLine();
        const std::string countText = "x" + std::to_string(row.count);
        _drawPill(countText.c_str(), EditorStyle::Scale(EditorStyle::FrameBg, 1.1f), EditorStyle::Muted);
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

    if (m_paused.load()) {
        if (m_pendingMessages.size() >= MAX_MESSAGES) {
            m_pendingMessages.erase(m_pendingMessages.begin());
        }
        m_pendingMessages.push_back({ timestamp, level, source, message, m_nextMessageId.fetch_add(1) });
        return;
    }

    // Limit message buffer size
    if (m_messages.size() >= MAX_MESSAGES) {
        m_selectedMessageIds.erase(m_messages.front().Id);
        m_messages.erase(m_messages.begin());
    }

    m_messages.push_back({ timestamp, level, source, message, m_nextMessageId.fetch_add(1) });
    m_filterDirty = true;
    m_scrollToBottom = true;
}

void ConsolePanel::Clear() {
    std::lock_guard<std::mutex> lock(m_messagesMutex);
    m_messages.clear();
    m_pendingMessages.clear();
    m_filteredIndices.clear();
    m_filterDirty = true;
    m_lastMessageCount = 0;
    m_lastPendingCount = 0;
    m_totalEligibleCount = 0;
    m_selectedMessageIds.clear();
    m_lastSelectedRow = -1;
}

// -------------------------------------------------------------------------
// Filtering
// -------------------------------------------------------------------------

void ConsolePanel::_rebuildFilterCache(const std::vector<ConsoleMessage>& snapshot) {
    if (m_lastMessageCount != snapshot.size()) {
        m_filterDirty = true;
        m_lastMessageCount = snapshot.size();
    }

    if (m_lastShowInfo != m_showInfo || m_lastShowDebug != m_showDebug ||
        m_lastShowWarning != m_showWarning || m_lastShowError != m_showError ||
        m_lastShowCritical != m_showCritical) {
        m_filterDirty = true;
        m_lastShowInfo = m_showInfo;
        m_lastShowDebug = m_showDebug;
        m_lastShowWarning = m_showWarning;
        m_lastShowError = m_showError;
        m_lastShowCritical = m_showCritical;
    }

    if (m_lastSourceFilter != m_sourceFilter) {
        m_filterDirty = true;
        m_lastSourceFilter = m_sourceFilter;
    }

    if (m_lastCollapseRepeated != m_collapseRepeated) {
        m_filterDirty = true;
        m_lastCollapseRepeated = m_collapseRepeated;
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

        m_searchLower = m_searchBuffer;
        std::transform(m_searchLower.begin(), m_searchLower.end(), m_searchLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    if (!m_filterDirty) {
        return;
    }

    m_filteredIndices.clear();
    m_renderRows.clear();
    m_totalEligibleCount = 0;
    m_levelCounts = { 0, 0, 0, 0, 0 };

    for (size_t i = 0; i < snapshot.size(); ++i) {
        const auto& msg = snapshot[i];

        if (m_sourceFilter == 1 && msg.Source != LogSource::ENGINE) {
            continue;
        }
        if (m_sourceFilter == 2 && msg.Source != LogSource::SCRIPT) {
            continue;
        }

        if (!m_searchLower.empty() && !_messageMatchesSearch(msg, m_searchLower)) {
            continue;
        }

        ++m_totalEligibleCount;

        switch (msg.Level) {
        case LogLevel::INFO:     ++m_levelCounts[0]; break;
        case LogLevel::DEBUG:    ++m_levelCounts[1]; break;
        case LogLevel::WARNING:  ++m_levelCounts[2]; break;
        case LogLevel::ERROR:    ++m_levelCounts[3]; break;
        case LogLevel::CRITICAL: ++m_levelCounts[4]; break;
        default: break;
        }

        if (_shouldDisplayMessage(msg, m_searchLower)) {
            m_filteredIndices.push_back(static_cast<int>(i));
        }
    }

    if (!m_filteredIndices.empty()) {
        RenderRow current;
        current.firstFilteredIndex = 0;
        current.lastFilteredIndex = 0;
        current.count = 1;

        for (int i = 1; i < static_cast<int>(m_filteredIndices.size()); ++i) {
            const ConsoleMessage& prev = snapshot[static_cast<size_t>(m_filteredIndices[static_cast<size_t>(i - 1)])];
            const ConsoleMessage& next = snapshot[static_cast<size_t>(m_filteredIndices[static_cast<size_t>(i)])];

            const bool sameGroup = m_collapseRepeated &&
                prev.Level == next.Level &&
                prev.Source == next.Source &&
                prev.Content == next.Content;

            if (sameGroup) {
                current.lastFilteredIndex = i;
                ++current.count;
            }
            else {
                m_renderRows.push_back(current);
                current.firstFilteredIndex = i;
                current.lastFilteredIndex = i;
                current.count = 1;
            }
        }
        m_renderRows.push_back(current);
    }

    m_filterDirty = false;
}

bool ConsolePanel::_shouldDisplayMessage(const ConsoleMessage& msg, const std::string& /*searchLower*/) const {
    // Filter by level checkboxes
    switch (msg.Level) {
    case LogLevel::INFO:     return m_showInfo;
    case LogLevel::DEBUG:    return m_showDebug;
    case LogLevel::WARNING:  return m_showWarning;
    case LogLevel::ERROR:    return m_showError;
    case LogLevel::CRITICAL: return m_showCritical;
    default: return false; // TRACE or unknown
    }
}

bool ConsolePanel::_messageMatchesSearch(const ConsoleMessage& msg, const std::string& searchLower) const {
    if (searchLower.empty()) {
        return true;
    }

    auto containsLower = [&searchLower](const std::string& value) {
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower.find(searchLower) != std::string::npos;
    };

    if (containsLower(msg.Content)) {
        return true;
    }
    if (containsLower(msg.Timestamp)) {
        return true;
    }
    if (containsLower(_getLevelText(msg.Level))) {
        return true;
    }
    if (containsLower(_getSourceText(msg.Source))) {
        return true;
    }

    return false;
}

// -------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------

ImVec4 ConsolePanel::_getColorForLevel(LogLevel level) const {
    const float shade = 0.55f;
    ImVec4 base;

    switch (level) {
    case LogLevel::INFO:     base = EditorStyle::LogInfo; break;
    case LogLevel::DEBUG:    base = EditorStyle::LogDebug; break;
    case LogLevel::WARNING:  base = EditorStyle::LogWarning; break;
    case LogLevel::ERROR:    base = EditorStyle::DangerText; break;
    case LogLevel::CRITICAL: base = EditorStyle::LogCritical; break;
    default:                 base = EditorStyle::Text; break;
    }

    return ImVec4(base.x * shade, base.y * shade, base.z * shade, 0.95f);
}

ImVec4 ConsolePanel::_getBadgeColorForLevel(LogLevel level) const {
    ImVec4 base = _getColorForLevel(level);
    const float shade = 0.55f;
    return ImVec4(base.x * shade, base.y * shade, base.z * shade, 0.95f);
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

const char* ConsolePanel::_getSourceText(LogSource source) const {
    switch (source) {
    case LogSource::ENGINE: return "ENGINE";
    case LogSource::SCRIPT: return "SCRIPT";
    default:                return "UNKNOWN";
    }
}

void ConsolePanel::_drawPill(const char* text, const ImVec4& bgColor, const ImVec4& textColor, float minWidth) const {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 padding(6.0f, 2.0f);

    float width = textSize.x + padding.x * 2.0f;
    if (minWidth > width) {
        width = minWidth;
    }

    const ImVec2 size(width, textSize.y + padding.y * 2.0f);
    const float rounding = ImGui::GetStyle().FrameRounding;

    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::GetColorU32(bgColor), rounding);
    const float textX = pos.x + (size.x - textSize.x) * 0.5f;
    drawList->AddText(ImVec2(textX, pos.y + padding.y), ImGui::GetColorU32(textColor), text);
    ImGui::Dummy(size);
}

bool ConsolePanel::_renderLevelChip(const char* id, const char* label, const ImVec4& color, bool active, int count) {
    std::string text = std::string(label) + " " + std::to_string(count);
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    const ImVec2 padding(6.0f, 3.0f);
    const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);

    ImGui::InvisibleButton(id, size);
    const bool clicked = ImGui::IsItemClicked();

    ImVec4 bg = active ? ImVec4(color.x, color.y, color.z, 0.85f) : EditorStyle::Scale(EditorStyle::FrameBg, 0.9f);
    ImVec4 fg = active ? EditorStyle::Text : EditorStyle::TextDisabled;

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float rounding = ImGui::GetStyle().FrameRounding;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(bg), rounding);
    drawList->AddText(ImVec2(min.x + padding.x, min.y + padding.y), ImGui::GetColorU32(fg), text.c_str());

    return clicked;
}

void ConsolePanel::_flushPendingMessages() {
    std::lock_guard<std::mutex> lock(m_messagesMutex);
    if (m_pendingMessages.empty()) {
        return;
    }

    for (const auto& msg : m_pendingMessages) {
        if (m_messages.size() >= MAX_MESSAGES) {
            m_selectedMessageIds.erase(m_messages.front().Id);
            m_messages.erase(m_messages.begin());
        }
        m_messages.push_back(msg);
    }

    m_pendingMessages.clear();
    m_filterDirty = true;
    m_scrollToBottom = true;
}

void ConsolePanel::_copyMessagesToClipboard(const std::vector<ConsoleMessage>& snapshot, const std::vector<int>* indices,
    bool messageOnly, bool includeTimestamp, bool includeLevel) const {
    std::ostringstream oss;

    auto appendMessage = [&](const ConsoleMessage& msg) {
        if (messageOnly) {
            oss << msg.Content;
        }
        else if (includeTimestamp && includeLevel) {
            oss << msg.Timestamp << " [" << _getLevelText(msg.Level) << "] " << msg.Content;
        }
        else if (includeTimestamp) {
            oss << msg.Timestamp << " " << msg.Content;
        }
        else if (includeLevel) {
            oss << "[" << _getLevelText(msg.Level) << "] " << msg.Content;
        }
        else {
            oss << msg.Content;
        }
    };

    if (indices) {
        for (size_t i = 0; i < indices->size(); ++i) {
            const int msgIndex = (*indices)[i];
            if (msgIndex < 0 || msgIndex >= static_cast<int>(snapshot.size())) {
                continue;
            }
            appendMessage(snapshot[static_cast<size_t>(msgIndex)]);
            if (i + 1 < indices->size()) {
                oss << "\n";
            }
        }
    }
    else {
        for (size_t i = 0; i < snapshot.size(); ++i) {
            appendMessage(snapshot[i]);
            if (i + 1 < snapshot.size()) {
                oss << "\n";
            }
        }
    }

    ImGui::SetClipboardText(oss.str().c_str());
}
