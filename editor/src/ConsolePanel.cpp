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
    m_mainFont = mainFont;        // Regular text rendering
    m_boldFont = boldFont;        // Level badges (INF, ERR, etc.)
    m_symbolsFont = symbolsFont;  // Icon glyphs (if needed)

    // Clear any messages that accumulated before console was initialized
    Clear();

    m_initialized = true;  // Allow message processing from this point forward
}

void ConsolePanel::Shutdown() {
    // Mark as uninitialized to prevent any new messages
    // This must happen first to block new AddMessage calls
    m_initialized = false;

    // Clear callback to prevent further invocations during shutdown
    // Prevents logger from calling into destroyed console
    Logger::Get().SetConsoleCallback(nullptr);

    // Clear messages
    std::lock_guard<std::mutex> lock(m_messagesMutex);
    m_messages.clear();         // Remove all stored messages
    m_pendingMessages.clear();  // Remove queued messages (when paused)
    m_selectedMessageIds.clear();  // Clear UI selection state
}

// -------------------------------------------------------------------------
// Rendering
// -------------------------------------------------------------------------

void ConsolePanel::Render() {
    ImGui::PushFont(m_mainFont);
    ImGui::Begin("Console");

    // Create thread-safe snapshot of messages for rendering
    // This allows message queue to be modified on other threads during render
    std::vector<ConsoleMessage> snapshot;
    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);  // Minimal lock scope
        snapshot = m_messages;  // Copy entire message vector
        m_lastPendingCount = m_pendingMessages.size();  // Track paused queue size
    }

    // Initial filter rebuild to count levels for toolbar badges
    _rebuildFilterCache(snapshot);
    _renderToolbar(snapshot);
    
    // Rebuild filter if toolbar interactions changed filter settings
    if (m_filterDirty) {
        _rebuildFilterCache(snapshot);
    }
    
    ImGui::Separator();
    _renderMessages(snapshot);

    ImGui::End();
    ImGui::PopFont();
}

void ConsolePanel::_renderToolbar(const std::vector<ConsoleMessage>& snapshot) {
    // Clear button (destructive) uses danger styling.
    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
    if (ImGui::Button("Clear")) {
        Clear();
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();

    // Pause/Resume toggle: queues messages instead of adding them
    // Useful when console spam makes it hard to read specific messages
    const bool paused = m_paused.load();  // Atomic read for thread safety
    // Pause/Resume uses a neutral button palette to avoid destructive emphasis.
    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
    if (ImGui::Button(paused ? "Resume" : "Pause")) {
        m_paused.store(!paused);  // Atomic write
        if (paused) {
            _flushPendingMessages();  // When resuming, add queued messages
        }
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, 12.0f);

    // Auto-scroll toggle: keeps newest messages visible
    // Only scrolls if user was already at bottom (doesn't interrupt browsing)
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::SameLine();

    // Display total message count in snapshot
    ImGui::Text("| Messages: %zu", snapshot.size());

    // Show queued message count when paused
    // Helps user see how many messages are waiting to be displayed
    if (m_paused.load() && m_lastPendingCount > 0) {
        ImGui::SameLine();
        ImGui::Text("| Queued: %zu", m_lastPendingCount);
    }

    ImGui::SameLine();
    // Show filtered count vs eligible count (before level filtering)
    ImGui::Text("| Filtered: %zu / %zu", m_filteredIndices.size(), m_totalEligibleCount);

    ImGui::SameLine();

    // Options menu for bulk operations and settings
    const float optionsWidth = ImGui::CalcTextSize("Options").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float optionsTargetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - optionsWidth;
    if (optionsTargetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(optionsTargetX);
    }
    if (ImGui::Button("Options")) {
        ImGui::OpenPopup("ConsoleOptions");
    }

    if (ImGui::BeginPopup("ConsoleOptions")) {
        // Copy all messages to clipboard (ignores filters)
        if (ImGui::MenuItem("Copy all")) {
            _copyMessagesToClipboard(snapshot, nullptr, false, true, true);
        }
        // Copy only filtered/visible messages
        if (ImGui::MenuItem("Copy filtered")) {
            _copyMessagesToClipboard(snapshot, &m_filteredIndices, false, true, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear")) {
            Clear();
        }
        // Auto-clear when entering play mode or building
        // Useful to separate editor logs from runtime logs
        ImGui::Checkbox("Clear on play/build", &m_clearOnPlayBuild);
        ImGui::EndPopup();
    }

    // Level filter toggle chips: each shows count and can be clicked to toggle
    ImGui::Separator();
    ImGui::Text("Levels:");
    ImGui::SameLine();

    // Each chip shows: level name, count, and visual state (active/inactive)
    // Clicking toggles visibility of that level in the message list
    if (_renderLevelChip("##level_inf", "INF", _getColorForLevel(LogLevel::INFO), m_showInfo, m_levelCounts[0])) {
        m_showInfo = !m_showInfo;
        m_filterDirty = true;  // Mark for filter rebuild
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_dbg", "DBG", _getColorForLevel(LogLevel::DEBUG), m_showDebug, m_levelCounts[1])) {
        m_showDebug = !m_showDebug;
        m_filterDirty = true;
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_wrn", "WRN", _getColorForLevel(LogLevel::WARNING), m_showWarning, m_levelCounts[2])) {
        m_showWarning = !m_showWarning;
        m_filterDirty = true;
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_err", "ERR", _getColorForLevel(LogLevel::ERROR), m_showError, m_levelCounts[3])) {
        m_showError = !m_showError;
        m_filterDirty = true;
    }
    ImGui::SameLine();
    if (_renderLevelChip("##level_crt", "CRT", _getColorForLevel(LogLevel::CRITICAL), m_showCritical, m_levelCounts[4])) {
        m_showCritical = !m_showCritical;
        m_filterDirty = true;
    }

    ImGui::SameLine(0.0f, 12.0f);

    // Collapse repeated messages - groups identical consecutive messages
    // Shows count badge instead of duplicating rows
    ImGui::Checkbox("Collapse", &m_collapseRepeated);
    if (m_lastCollapseRepeated != m_collapseRepeated) {
        m_filterDirty = true;  // Rebuild to apply/unapply grouping
        m_lastCollapseRepeated = m_collapseRepeated;
    }
 
    ImGui::SameLine();
    const char* sourceLabel = "Source:";
    const float sourceLabelWidth = ImGui::CalcTextSize(sourceLabel).x;
    const float sourceComboWidth = 100.0f;
    const float sourceSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float sourceTotalWidth = sourceLabelWidth + sourceSpacing + sourceComboWidth;
    const float sourceTargetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - sourceTotalWidth;
    if (sourceTargetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(sourceTargetX);
    }
    ImGui::Text("%s", sourceLabel);
    ImGui::SameLine();

    // Source filter: All/Engine/Script
    // Filters messages based on where they originated
    const char* sourceOptions[] = { "All", "Engine", "Script" };
    ImGui::SetNextItemWidth(sourceComboWidth);
    if (ImGui::Combo("##source_filter", &m_sourceFilter, sourceOptions, IM_ARRAYSIZE(sourceOptions))) {
        m_filterDirty = true;  // Rebuild to apply new source filter
    }

    // Search bar: filters messages by text content
    // Searches in: message content, timestamp, level name, and source name
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);  // Full width
    if (ImGui::InputTextWithHint("##search", "Search messages...", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_filterDirty = true;  // Rebuild filter on search text change
    }
}

void ConsolePanel::_renderMessages(const std::vector<ConsoleMessage>& snapshot) {
    // Use child region for independent scrolling within the console window
    ImGui::BeginChild("MessageList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Ctrl+A to select all visible messages
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        m_selectedMessageIds.clear();

        // Select all messages in all render rows (collapsed or not)
        for (const auto& row : m_renderRows) {
            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];

                if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {
                    m_selectedMessageIds.insert(snapshot[static_cast<size_t>(msgIndex)].Id);
                }
            }
        }
        // Track last selected row for shift-click range selection
        if (!m_renderRows.empty()) {
            m_lastSelectedRow = static_cast<int>(m_renderRows.size() - 1);
        }
    }

    // Check if user was scrolled to bottom BEFORE rendering
    // We only auto-scroll if they were already at the bottom (don't interrupt browsing)
    const bool wasAtBottom = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);

    // Table with 3 columns: Time, Level, Message
    const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX;
    if (ImGui::BeginTable("ConsoleTable", 3, tableFlags)) {
        // Calculate column widths
        const float timeWidth = 90.0f;  // Fixed width for timestamps
        const float badgeMinWidth = ImGui::CalcTextSize("CRT").x + 25.0f;  // Widest badge
        const float levelWidth = badgeMinWidth + ImGui::GetStyle().CellPadding.x * 2.0f + 6.0f;

        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, timeWidth);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, levelWidth);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);  // Takes remaining space

        // Calculate available width for message text wrapping
        // Subtract column widths and padding to prevent text overflow
        float messageWrapWidth = ImGui::GetContentRegionAvail().x - timeWidth - levelWidth - ImGui::GetStyle().CellPadding.x * 4.0f;
        messageWrapWidth = (std::max)(1.0f, messageWrapWidth);  // Ensure positive width

        // Render each row (may contain multiple collapsed messages)
        for (int rowIndex = 0; rowIndex < static_cast<int>(m_renderRows.size()); ++rowIndex) {
            _renderMessageRow(snapshot, m_renderRows[static_cast<size_t>(rowIndex)], rowIndex, messageWrapWidth, badgeMinWidth);
        }

        ImGui::EndTable();
    }

    // Auto-scroll to bottom only if user was already at bottom
    // This prevents interrupting the user when they're scrolling through old messages
    if (m_autoScroll && m_scrollToBottom && wasAtBottom) {
        ImGui::SetScrollHereY(1.0f);  // Scroll to bottom
        m_scrollToBottom = false;     // Reset flag until next message arrives
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
    // Size rows to content while keeping a compact minimum height.
    const float minRowHeight = ImGui::GetTextLineHeight();
    const float rowHeight = std::max(minRowHeight, messageSize.y) + ImGui::GetStyle().CellPadding.y;

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
        // Soften selection background to keep text readable.
        ImVec4 sel = EditorStyle::Selection;
        sel.w = 0.18f;
        bgColor = sel;
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
    _drawPill(_getLevelText(msg.Level), _getColorForLevel(msg.Level), EditorStyle::Text, badgeMinWidth);
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
    // Without this check, messages could be added to destroyed data structures
    if (!m_initialized) {
        return;
    }

    // Filter: Only store warnings/errors/critical from any source,
    // OR info/debug from SCRIPT source only
    // This reduces console spam from engine internals while keeping user script logs
    if (level == LogLevel::TRACE) {
        return; // Never show TRACE in console (too verbose)
    }

    if ((level == LogLevel::INFO || level == LogLevel::DEBUG) && source != LogSource::SCRIPT) {
        return; // Only show INFO/DEBUG from scripts, not engine (reduces clutter)
    }

    // Thread-safe message insertion
    std::lock_guard<std::mutex> lock(m_messagesMutex);

    // If paused, add to pending queue instead of main message list
    // This lets users freeze the console to read without losing new messages
    if (m_paused.load()) {
        if (m_pendingMessages.size() >= MAX_MESSAGES) {
            m_pendingMessages.erase(m_pendingMessages.begin());  // Remove oldest when full
        }
        m_pendingMessages.push_back({ timestamp, level, source, message, m_nextMessageId.fetch_add(1) });
        return;  // Don't add to main list yet
    }

    // Limit message buffer size to prevent unbounded memory growth
    // Removes oldest messages when limit reached (FIFO queue)
    if (m_messages.size() >= MAX_MESSAGES) {
        m_selectedMessageIds.erase(m_messages.front().Id);  // Clear selection if removing selected message
        m_messages.erase(m_messages.begin());  // Remove oldest
    }

    m_messages.push_back({ timestamp, level, source, message, m_nextMessageId.fetch_add(1) });
    m_filterDirty = true;      // Mark filter for rebuild to include new message
    m_scrollToBottom = true;   // Request scroll to show new message
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
    // Detect if message count changed (new messages added)
    if (m_lastMessageCount != snapshot.size()) {
        m_filterDirty = true;  // Need to reprocess filters
        m_lastMessageCount = snapshot.size();
    }

    // Detect if any level filter checkboxes changed
    // Compare current state with last frame's state
    if (m_lastShowInfo != m_showInfo || m_lastShowDebug != m_showDebug ||
        m_lastShowWarning != m_showWarning || m_lastShowError != m_showError ||
        m_lastShowCritical != m_showCritical) {
        m_filterDirty = true;  // User toggled a level filter
        m_lastShowInfo = m_showInfo;
        m_lastShowDebug = m_showDebug;
        m_lastShowWarning = m_showWarning;
        m_lastShowError = m_showError;
        m_lastShowCritical = m_showCritical;
    }

    // Detect if source filter changed (All/Engine/Script)
    if (m_lastSourceFilter != m_sourceFilter) {
        m_filterDirty = true;
        m_lastSourceFilter = m_sourceFilter;
    }

    // Detect if collapse mode changed
    if (m_lastCollapseRepeated != m_collapseRepeated) {
        m_filterDirty = true;
        m_lastCollapseRepeated = m_collapseRepeated;
    }

    // Cheap rolling hash for search buffer to detect changes
    // Using FNV-1a hash - faster than string comparison for change detection
    uint32_t searchHash = 2166136261u;  // FNV offset basis
    for (const char* p = m_searchBuffer; *p; ++p) {
        searchHash ^= static_cast<uint8_t>(*p);
        searchHash *= 16777619u;  // FNV prime
    }
    if (m_lastSearchHash != searchHash) {
        m_filterDirty = true;  // Search text changed
        m_lastSearchHash = searchHash;

        // Precompute lowercase version for case-insensitive search
        m_searchLower = m_searchBuffer;
        std::transform(m_searchLower.begin(), m_searchLower.end(), m_searchLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    }

    // Skip rebuild if nothing changed
    if (!m_filterDirty) {
        return;
    }

    // Clear previous filter results
    m_filteredIndices.clear();   // Indices of messages that pass filters
    m_renderRows.clear();        // Rows to render (with collapse grouping)
    m_totalEligibleCount = 0;    // Count before level filtering
    m_levelCounts = { 0, 0, 0, 0, 0 };  // Reset counts for filter badges

    // First pass: apply source and search filters, count levels
    for (size_t i = 0; i < snapshot.size(); ++i) {
        const auto& msg = snapshot[i];

        // Source filter: skip if doesn't match selected source
        if (m_sourceFilter == 1 && msg.Source != LogSource::ENGINE) {
            continue;  // User wants Engine only, this is Script
        }
        if (m_sourceFilter == 2 && msg.Source != LogSource::SCRIPT) {
            continue;  // User wants Script only, this is Engine
        }

        // Search filter: skip if doesn't match search text
        if (!m_searchLower.empty() && !_messageMatchesSearch(msg, m_searchLower)) {
            continue;  // Doesn't match search query
        }

        ++m_totalEligibleCount;  // Passed source and search filters

        // Count messages by level for filter badge display
        switch (msg.Level) {
        case LogLevel::INFO:     ++m_levelCounts[0]; break;
        case LogLevel::DEBUG:    ++m_levelCounts[1]; break;
        case LogLevel::WARNING:  ++m_levelCounts[2]; break;
        case LogLevel::ERROR:    ++m_levelCounts[3]; break;
        case LogLevel::CRITICAL: ++m_levelCounts[4]; break;
        default: break;
        }

        // Level filter: add to filtered indices if level is enabled
        if (_shouldDisplayMessage(msg, m_searchLower)) {
            m_filteredIndices.push_back(static_cast<int>(i));
        }
    }

    // Second pass: group consecutive identical messages if collapse enabled
    if (!m_filteredIndices.empty()) {
        RenderRow current;
        current.firstFilteredIndex = 0;  // Start of group
        current.lastFilteredIndex = 0;   // End of group (same as first initially)
        current.count = 1;               // Number of messages in group

        // Compare each message with previous to find groups
        for (int i = 1; i < static_cast<int>(m_filteredIndices.size()); ++i) {
            const ConsoleMessage& prev = snapshot[static_cast<size_t>(m_filteredIndices[static_cast<size_t>(i - 1)])];
            const ConsoleMessage& next = snapshot[static_cast<size_t>(m_filteredIndices[static_cast<size_t>(i)])];

            // Messages are in same group if collapse is on AND all properties match
            const bool sameGroup = m_collapseRepeated &&
                prev.Level == next.Level &&      // Same severity
                prev.Source == next.Source &&    // Same origin
                prev.Content == next.Content;    // Same text

            if (sameGroup) {
                // Extend current group
                current.lastFilteredIndex = i;
                ++current.count;
            }
            else {
                // Save current group and start new one
                m_renderRows.push_back(current);
                current.firstFilteredIndex = i;
                current.lastFilteredIndex = i;
                current.count = 1;
            }
        }
        m_renderRows.push_back(current);  // Don't forget last group
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

    return ImVec4(base.x * shade, base.y * shade, base.z * shade, 1.0f);
}

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
