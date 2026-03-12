/* Start Header *****************************************************************/
/*!
\file   ConsolePanel.cpp
\author Foo Rui Qin (70%)
        Muhammad Nur Fadzly Bin Zulkifli (30%)
\par    ruiqin.foo@digipen.edu
        muhammadnurfadzly.b@digipen.edu
\date   12th March 2026

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
#include "EditorIcons.h"

#ifdef max
#undef max
#endif

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

// Binds fonts, clears stale state, and marks the panel ready to accept incoming messages
void ConsolePanel::Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont) {
    m_mainFont = mainFont;        // Regular text rendering
    m_boldFont = boldFont;        // Level badges (INF, ERR, etc.)
    m_symbolsFont = symbolsFont;  // Icon glyphs (if needed)

    // Clear any messages that accumulated before console was initialized
    Clear();

    // Allow message processing from this point forward
    m_initialized = true;  
}

// Stops message intake, disconnects logger callback, and clears all runtime console state
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

// Draws the full console panel, including toolbar controls and filtered message list
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

// Draws top toolbar controls for clear, pause, filters, source selection, and text search
void ConsolePanel::_renderToolbar(const std::vector<ConsoleMessage>& snapshot) {

    // Clear button uses danger colors because this action discards visible and queued history
    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::DangerButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::DangerButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::DangerButtonActive);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);

    if (ImGui::Button("Clear")) {
        Clear();
    }
    ImGui::PopStyleColor(4);

    ImGui::SameLine();

    // Pause mode redirects incoming logs into pending queue so current view stays stable while reading
    const bool paused = m_paused.load();  // Atomic read for thread safety

    // Pause and resume use neutral styling since this is a mode toggle, not a destructive action
    ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::SecondaryButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::SecondaryButtonHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::SecondaryButtonActive);
    ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::Text);
    const char* pauseText = paused ? "Resume" : "Pause";

    if (ImGui::Button(pauseText)) {
        m_paused.store(!paused);      // Atomic write

        if (paused) {
            _flushPendingMessages();  // When resuming, add queued messages
        }
    }
    ImGui::PopStyleColor(4);
    ImGui::SameLine(0.0f, 12.0f);

    // Auto scroll follows new logs only when user is already at bottom so browsing older rows is not interrupted
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

    // Right aligned options button keeps secondary actions discoverable without crowding primary controls
    const float optionsWidth = ImGui::CalcTextSize("Options").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const float optionsTargetX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - optionsWidth;

    if (optionsTargetX > ImGui::GetCursorPosX()) {
        ImGui::SetCursorPosX(optionsTargetX);
    }
    if (ImGui::Button("Options")) {
        ImGui::OpenPopup("ConsoleOptions");
    }

    if (ImGui::BeginPopup("ConsoleOptions")) {
        // Copy all exports the raw snapshot regardless of active UI filters
        if (ImGui::MenuItem("Copy all")) {
            _copyMessagesToClipboard(snapshot, nullptr, false, true, true);
        }

        // Copy filtered exports only messages that survived current level, source, and search filters
        if (ImGui::MenuItem("Copy filtered")) {
            _copyMessagesToClipboard(snapshot, &m_filteredIndices, false, true, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear")) {
            Clear();
        }

        // Auto clear creates clean sessions between tool time and runtime or build phases
        ImGui::Checkbox("Clear on play/build", &m_clearOnPlayBuild);
        ImGui::EndPopup();
    }

    // Level chips show per level counts and toggle visibility for each severity lane
    ImGui::Separator();
    ImGui::Text("Levels:");
    ImGui::SameLine();

    // Each chip carries label, count, and active state color so filter intent is visible at a glance
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

    // Collapse groups consecutive duplicate entries and replaces repeated rows with one row plus count badge
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

    // Source filter narrows logs by origin channel
    const char* sourceOptions[] = { "All", "Engine", "Script" };
    ImGui::SetNextItemWidth(sourceComboWidth);
    if (ImGui::Combo("##source_filter", &m_sourceFilter, sourceOptions, IM_ARRAYSIZE(sourceOptions))) {
        m_filterDirty = true;  // Rebuild to apply new source filter
    }

    // Search applies case insensitive matching across content, timestamp, level token, and source token
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);  // Full width
    if (ImGui::InputTextWithHint("##search", "Search messages...", m_searchBuffer, sizeof(m_searchBuffer))) {
        m_filterDirty = true;  // Rebuild filter on search text change
    }
}

// Draws the scrollable message table, handles select all shortcut, and applies auto scroll policy
void ConsolePanel::_renderMessages(const std::vector<ConsoleMessage>& snapshot) {

    // Use child region for independent scrolling within the console window
    ImGui::BeginChild("MessageList", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    // Ctrl+A to select all visible messages
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A) && !io.WantTextInput) {
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

    // Three column table keeps timestamp and level fixed while message column consumes remaining width
    const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX;
    if (ImGui::BeginTable("ConsoleTable", 3, tableFlags)) {
        // Fixed columns stabilize alignment across long and short messages
        const float timeWidth = 90.0f;  // Fixed width for timestamps
        const float badgeMinWidth = ImGui::CalcTextSize("CRT").x + 25.0f;  // Widest badge
        const float levelWidth = badgeMinWidth + ImGui::GetStyle().CellPadding.x * 2.0f + 6.0f;

        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, timeWidth);
        ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, levelWidth);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);  // Takes remaining space

        // Wrap width subtracts fixed columns and padding so message text wraps inside visible cell area
        float messageWrapWidth = ImGui::GetContentRegionAvail().x - timeWidth - levelWidth - ImGui::GetStyle().CellPadding.x * 4.0f;
        messageWrapWidth = (std::max)(1.0f, messageWrapWidth);  // Ensure positive width

        // Render rows from prepared render list where each row may represent one or many collapsed messages
        for (int rowIndex = 0; rowIndex < static_cast<int>(m_renderRows.size()); ++rowIndex) {
            _renderMessageRow(snapshot, m_renderRows[static_cast<size_t>(rowIndex)], rowIndex, messageWrapWidth, badgeMinWidth);
        }

        ImGui::EndTable();
    }

    // Auto scroll fires only when user was already at bottom to avoid snapping away from historical review
    if (m_autoScroll && m_scrollToBottom && wasAtBottom) {
        ImGui::SetScrollHereY(1.0f);  // Scroll to bottom
        m_scrollToBottom = false;     // Reset flag until next message arrives
    }

    ImGui::EndChild();
}

// Draws one visual row, resolves selection behavior, and provides row specific context actions
void ConsolePanel::_renderMessageRow(const std::vector<ConsoleMessage>& snapshot, const RenderRow& row, int rowIndex, float messageWrapWidth, float badgeMinWidth) {
    // Guard against stale row metadata that may appear if filters changed between cache rebuild and draw
    if (row.firstFilteredIndex < 0 || row.firstFilteredIndex >= static_cast<int>(m_filteredIndices.size())) {
        return;
    }

    // Use the last message in the collapsed span as the representative visual row payload
    const int displayMessageIndex = m_filteredIndices[static_cast<size_t>(row.lastFilteredIndex)];
    if (displayMessageIndex < 0 || displayMessageIndex >= static_cast<int>(snapshot.size())) {
        return;
    }

    const ConsoleMessage& msg = snapshot[static_cast<size_t>(displayMessageIndex)];

    // A collapsed row is selected if any message id inside its filtered span is selected
    bool rowSelected = false;

    // Scan the grouped span so selection state matches both collapsed and non collapsed views
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

    // Row height follows wrapped message size while preserving one line minimum for short messages
    const float minRowHeight = ImGui::GetTextLineHeight();
    const float rowHeight = std::max(minRowHeight, messageSize.y) + ImGui::GetStyle().CellPadding.y;

    // Reserve row in table first so all subsequent row operations target a valid table item
    ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
    ImGui::TableSetColumnIndex(0);

    // Cache row origin now, then draw an invisible spanning selectable for row click handling
    const ImVec2 rowStart = ImGui::GetCursorPos();
    ImGui::PushID(static_cast<int>(msg.Id));
    ImGui::PushStyleColor(ImGuiCol_Header, EditorStyle::Transparent);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, EditorStyle::Transparent);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, EditorStyle::Transparent);
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.0f, rowHeight));

    // Pull click and hover state from the invisible row item before drawing visible cell content
    ImGui::PopStyleColor(3);
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();

    // Selection model supports plain click, ctrl toggle, and shift range selection over rendered rows
    if (clicked || rightClicked) {
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        const bool shift = ImGui::GetIO().KeyShift;

        // Shift click expands selection to a contiguous row range anchored at the last selected row
        if (shift && m_lastSelectedRow >= 0) {
            const int start = std::min(m_lastSelectedRow, rowIndex);
            const int end = std::max(m_lastSelectedRow, rowIndex);

            // Shift without ctrl replaces existing selection with the computed range
            if (!ctrl) {
                m_selectedMessageIds.clear();
            }

            // Add all message ids from every row inside the selected range
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
            // Detect whether this row is currently selected so ctrl click can toggle full row state
            bool anySelected = false;

            // If any id in this row is selected, ctrl click removes all ids in this row
            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                const int msgIndex = m_filteredIndices[static_cast<size_t>(i)];

                if (msgIndex >= 0 && msgIndex < static_cast<int>(snapshot.size())) {

                    if (m_selectedMessageIds.count(snapshot[static_cast<size_t>(msgIndex)].Id) > 0) {
                        anySelected = true;
                        break;
                    }
                }
            }

            // Apply full row toggle to each id in collapsed span so state remains internally consistent
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
            // Plain click resets selection to this row only
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

    // Recompute row selected state after click logic so visuals reflect latest selection changes immediately
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

    // Context menu actions apply to the whole collapsed row span, not just the visible representative message
    if (ImGui::BeginPopupContextItem("RowContext")) {
        // Build explicit row index list for each copy variant so output matches current collapsed span
        if (ImGui::MenuItem("Copy")) {
            std::vector<int> rowIndices;
            rowIndices.reserve(static_cast<size_t>(row.count));

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                rowIndices.push_back(m_filteredIndices[static_cast<size_t>(i)]);
            }
            _copyMessagesToClipboard(snapshot, &rowIndices, false, true, true);
        }

        // Message only variant strips timestamp and level metadata from copied output
        if (ImGui::MenuItem("Copy message only")) {
            std::vector<int> rowIndices;
            rowIndices.reserve(static_cast<size_t>(row.count));

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                rowIndices.push_back(m_filteredIndices[static_cast<size_t>(i)]);
            }
            _copyMessagesToClipboard(snapshot, &rowIndices, true, false, false);
        }

        // Timestamp variant keeps chronology while omitting level badges from copied text
        if (ImGui::MenuItem("Copy with timestamp")) {
            std::vector<int> rowIndices;
            rowIndices.reserve(static_cast<size_t>(row.count));

            for (int i = row.firstFilteredIndex; i <= row.lastFilteredIndex; ++i) {
                rowIndices.push_back(m_filteredIndices[static_cast<size_t>(i)]);
            }
            _copyMessagesToClipboard(snapshot, &rowIndices, false, true, false);
        }
        ImGui::Separator();

        // Level filter shortcut switches to a single level view based on clicked row message level
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

        // Source filter shortcut jumps directly to engine or script source lane
        if (ImGui::MenuItem("Filter by source")) {
            m_sourceFilter = (msg.Source == LogSource::ENGINE) ? 1 : 2;
            m_filterDirty = true;
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();

    // Background color communicates selection, hover, zebra striping, and search emphasis in priority order
    ImVec4 bgColor = EditorStyle::Transparent;
    if (rowSelected) {
        // Keep selection tint soft enough that level based text colors remain legible
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

    // Slightly brighten rows during active search to reinforce that list is currently filtered by query
    if (!m_searchLower.empty()) {
        bgColor = ImVec4(
            std::min(1.0f, bgColor.x + 0.08f),
            std::min(1.0f, bgColor.y + 0.08f),
            std::min(1.0f, bgColor.z + 0.08f),
            bgColor.w + 0.08f
        );
    }

    // Apply computed row background before placing visible text in table columns
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(bgColor));

    // Restore row origin so visible content aligns with the full row selectable hit region
    ImGui::SetCursorPos(rowStart);

    // Column 0 shows timestamp in subdued text to keep focus on message and severity
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", msg.Timestamp.c_str());

    // Column 1 renders compact severity badge with fixed minimum width for column alignment
    ImGui::TableSetColumnIndex(1);
    ImGui::PushFont(m_boldFont ? m_boldFont : m_mainFont);
    _drawPill(_getLevelText(msg.Level), _getColorForLevel(msg.Level), EditorStyle::Text, badgeMinWidth);
    ImGui::PopFont();

    // Column 2 renders wrapped message body using level color as quick severity cue
    ImGui::TableSetColumnIndex(2);
    ImGui::PushStyleColor(ImGuiCol_Text, _getColorForLevel(msg.Level));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + messageWrapWidth);
    ImGui::TextUnformatted(msg.Content.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    // Collapsed groups append count badge so users can see repetition volume at a glance
    if (row.count > 1) {
        ImGui::SameLine();
        const std::string countText = "x" + std::to_string(row.count);
        _drawPill(countText.c_str(), EditorStyle::Scale(EditorStyle::FrameBg, 1.1f), EditorStyle::Muted);
    }
}

// -------------------------------------------------------------------------
// Message Management
// -------------------------------------------------------------------------

// Adds a new console entry with source and level filtering, pause queue handling, and bounded history storage
void ConsolePanel::AddMessage(LogLevel level, LogSource source, const std::string& timestamp, const std::string& message) {
    // Don't process messages if panel isn't initialized or is shutting down
    // Check this before any other operations to avoid race conditions
    // Without this check, messages could be added to destroyed data structures
    if (!m_initialized) {
        return;
    }

    // Store high severity logs from all sources and keep low severity logs only for script output
    // This reduces console spam from engine internals while keeping user script logs
    if (level == LogLevel::TRACE) {
        return; // Never show TRACE in console (too verbose)
    }

    if ((level == LogLevel::INFO || level == LogLevel::DEBUG) && source != LogSource::SCRIPT) {
        // Keep engine INFO/DEBUG noise out of the console
        return;
    }

    // Guard all message containers and selection ids during insertion
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

// Clears displayed and queued logs and resets all filter and selection cache state
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

// Recomputes filtered message indices, level counters, and collapsed render rows when relevant state changes
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

    // FNV 1a hash gives cheap change detection for the current search buffer content
    // Rehashing chars avoids allocating or lowercasing unless text actually changed
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

    // Reset all derived filter caches before rebuilding with current controls
    m_filteredIndices.clear();   // Indices of messages that pass filters
    m_renderRows.clear();        // Rows to render (with collapse grouping)
    m_totalEligibleCount = 0;    // Count before level filtering
    m_levelCounts = { 0, 0, 0, 0, 0 };  // Reset counts for filter badges

    // First pass applies source and text constraints, then counts per level and keeps level eligible indices
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

        // Level visibility check runs after source and search to populate final visible message list
        if (_shouldDisplayMessage(msg, m_searchLower)) {
            m_filteredIndices.push_back(static_cast<int>(i));
        }
    }

    // Second pass compresses consecutive duplicates into grouped rows when collapse mode is active
    if (!m_filteredIndices.empty()) {
        RenderRow current;
        current.firstFilteredIndex = 0;  // Start of group
        current.lastFilteredIndex = 0;   // End of group (same as first initially)
        current.count = 1;               // Number of messages in group

        // Compare neighbor messages in filtered order so grouping respects current filter view
        for (int i = 1; i < static_cast<int>(m_filteredIndices.size()); ++i) {
            const ConsoleMessage& prev = snapshot[static_cast<size_t>(m_filteredIndices[static_cast<size_t>(i - 1)])];
            const ConsoleMessage& next = snapshot[static_cast<size_t>(m_filteredIndices[static_cast<size_t>(i)])];

            // Grouping requires matching severity, source, and content to avoid merging different events
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

// Returns whether a message level is currently enabled by the toolbar level toggle chips
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

// Performs case insensitive text matching across message content, metadata fields, and labels
bool ConsolePanel::_messageMatchesSearch(const ConsoleMessage& msg, const std::string& searchLower) const {
    if (searchLower.empty()) {
        return true;
    }

    // Lowercase helper normalizes candidate strings so matching remains case insensitive
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

// Maps log level to the muted text color used by row badges and message text
ImVec4 ConsolePanel::_getColorForLevel(LogLevel level) const {
    // Shade multiplier keeps colors readable over themed console row backgrounds
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

// Returns compact three letter level token used in badges and copy output
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

// Returns source token used for search matching and source based filter display
const char* ConsolePanel::_getSourceText(LogSource source) const {
    switch (source) {
    case LogSource::ENGINE: return "ENGINE";
    case LogSource::SCRIPT: return "SCRIPT";
    default:                return "UNKNOWN";
    }
}

// Draws a rounded badge with centered text and optional minimum width for visual consistency
void ConsolePanel::_drawPill(const char* text, const ImVec4& bgColor, const ImVec4& textColor, float minWidth) const {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 padding(6.0f, 2.0f);

    // Enforce minimum width so short tokens align with longer badges in table layout
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

// Draws one clickable level chip that shows active state and visible count for that level
bool ConsolePanel::_renderLevelChip(const char* id, const char* label, const ImVec4& color, bool active, int count) {
    std::string text = std::string(label) + " " + std::to_string(count);
    const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
    const ImVec2 padding(6.0f, 3.0f);
    const ImVec2 size(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);

    // Use an invisible button for input then paint custom chip visuals over its item rect
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

// Moves queued paused messages into visible history while respecting max history size constraints
void ConsolePanel::_flushPendingMessages() {
    std::lock_guard<std::mutex> lock(m_messagesMutex);
    if (m_pendingMessages.empty()) {
        return;
    }

    // Append queued messages in order and trim front if history cap is reached
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

// Copies selected or full messages to clipboard using requested formatting options
void ConsolePanel::_copyMessagesToClipboard(const std::vector<ConsoleMessage>& snapshot, const std::vector<int>* indices,
    bool messageOnly, bool includeTimestamp, bool includeLevel) const {
    std::ostringstream oss;

    // Formatter helper applies one consistent output policy for each copied message
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

    // When indices are provided, copy only that subset in the given order
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
    // Without indices, copy the entire snapshot in chronological order
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
