/* Start Header *****************************************************************/
/*!
\file   ConsolePanel.h
\author Foo Rui Qin (100%)
\par    ruiqin.foo@digipen.edu
\date   20th November 2025

\brief
Declares the ConsolePanel class which displays all LOG_* messages in a dedicated
editor window. Provides filtering by log level, search, auto-scroll, and message
clearing. Integrates with Logger to capture all log output in real-time.
*/
/* End Header *******************************************************************/

#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <unordered_set>
#include <array>
#include "core/Logger.h"

struct ConsoleMessage {
    std::string Timestamp;
    LogLevel Level;
    LogSource Source;
    std::string Content;
    uint64_t Id = 0;
};

class ConsolePanel {
public:
    ConsolePanel() = default;
    ~ConsolePanel() = default;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize fonts and register the panel as a Logger sink.
     * @param mainFont Primary UI font.
     * @param boldFont Bold UI font.
     * @param symbolsFont Icon/symbol font.
     */
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);

    /** @brief Unregister from Logger and clear all stored messages. */
    void Shutdown();

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------

    /** @brief Render the full console panel window including toolbar and message list. */
    void Render();

    // -------------------------------------------------------------------------
    // Message Management
    // -------------------------------------------------------------------------

    /**
     * @brief Enqueue a new log message for display on the next frame.
     * @param level    Severity level of the message.
     * @param source   Origin of the message (engine or editor).
     * @param timestamp Formatted timestamp string.
     * @param message  Message body text.
     */
    void AddMessage(LogLevel level, LogSource source, const std::string& timestamp, const std::string& message);

    /** @brief Clear all stored messages and reset filter state. */
    void Clear();

    /**
     * @brief Check whether the panel is set to clear messages on play/build.
     * @return True if clear-on-play-build is enabled.
     */
    bool IsClearOnPlayBuildEnabled() const { return m_clearOnPlayBuild; }

private:
    struct RenderRow {
        int firstFilteredIndex = 0;
        int lastFilteredIndex = 0;
        int count = 1;
    };

    // -------------------------------------------------------------------------
    // UI Rendering
    // -------------------------------------------------------------------------

    /**
     * @brief Render the toolbar row (filter chips, search bar, clear button).
     * @param snapshot Immutable copy of the message list for this frame.
     */
    void _renderToolbar(const std::vector<ConsoleMessage>& snapshot);

    /**
     * @brief Render the scrollable message list area.
     * @param snapshot Immutable copy of the message list for this frame.
     */
    void _renderMessages(const std::vector<ConsoleMessage>& snapshot);

    /**
     * @brief Render a single (possibly collapsed) message row.
     * @param snapshot         Immutable message list for this frame.
     * @param row              Render row descriptor (index range and count).
     * @param rowIndex         Visual row index used for selection tracking.
     * @param messageWrapWidth Available width for wrapping message text.
     * @param badgeMinWidth    Minimum width for the level badge pill.
     */
    void _renderMessageRow(const std::vector<ConsoleMessage>& snapshot, const RenderRow& row, int rowIndex, float messageWrapWidth, float badgeMinWidth);

    // -------------------------------------------------------------------------
    // Filtering
    // -------------------------------------------------------------------------

    /**
     * @brief Rebuild the filtered index list and render-row cache from the current snapshot.
     * @param snapshot Immutable message list to filter.
     */
    void _rebuildFilterCache(const std::vector<ConsoleMessage>& snapshot);

    /**
     * @brief Check whether a message should appear given active filters and search text.
     * @param msg         Message to test.
     * @param searchLower Lowercased search string (empty means no text filter).
     * @return True if the message passes all active filters.
     */
    bool _shouldDisplayMessage(const ConsoleMessage& msg, const std::string& searchLower) const;

    /**
     * @brief Check whether a message's content or level matches the search string.
     * @param msg         Message to test.
     * @param searchLower Lowercased search string.
     * @return True if the message content contains the search string.
     */
    bool _messageMatchesSearch(const ConsoleMessage& msg, const std::string& searchLower) const;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    /**
     * @brief Get the display colour for a given log level.
     * @param level Log severity level.
     * @return RGBA colour used to tint the level badge and text.
     */
    ImVec4 _getColorForLevel(LogLevel level) const;

    /**
     * @brief Get the short display label for a log level (e.g. "INFO", "WARN").
     * @param level Log severity level.
     * @return Null-terminated string label.
     */
    const char* _getLevelText(LogLevel level) const;

    /**
     * @brief Get the display label for a log source (e.g. "ENGINE", "EDITOR").
     * @param source Log origin.
     * @return Null-terminated string label.
     */
    const char* _getSourceText(LogSource source) const;

    /**
     * @brief Draw a coloured rounded-rectangle pill with centred text.
     * @param text      Text to display inside the pill.
     * @param bgColor   Background fill colour.
     * @param textColor Text colour.
     * @param minWidth  Minimum pill width in pixels (0 = auto).
     */
    void _drawPill(const char* text, const ImVec4& bgColor, const ImVec4& textColor, float minWidth = 0.0f) const;

    /**
     * @brief Render a toggleable level-filter chip showing the message count.
     * @param id     ImGui widget ID.
     * @param label  Display text on the chip.
     * @param color  Chip accent colour.
     * @param active Whether the level filter is currently enabled.
     * @param count  Number of messages of this level currently visible.
     * @return True if the chip was clicked (caller should toggle the filter).
     */
    bool _renderLevelChip(const char* id, const char* label, const ImVec4& color, bool active, int count);

    /** @brief Move all queued pending messages into the main message list. */
    void _flushPendingMessages();

    /**
     * @brief Copy selected messages to the system clipboard.
     * @param snapshot        Current message list snapshot.
     * @param indices         Subset of row indices to copy; nullptr copies all visible rows.
     * @param messageOnly     When true, omit timestamp and level prefix.
     * @param includeTimestamp Prepend each line with its timestamp.
     * @param includeLevel    Prepend each line with its log level label.
     */
    void _copyMessagesToClipboard(const std::vector<ConsoleMessage>& snapshot, const std::vector<int>* indices,
        bool messageOnly, bool includeTimestamp, bool includeLevel) const;

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    std::vector<ConsoleMessage> m_messages;
    std::vector<ConsoleMessage> m_pendingMessages;
    std::mutex m_messagesMutex;
    std::atomic<bool> m_initialized{false};
    std::atomic<uint64_t> m_nextMessageId{1};

    // Filter settings
    bool m_showInfo = true;
    bool m_showDebug = true;
    bool m_showWarning = true;
    bool m_showError = true;
    bool m_showCritical = true;

    // UI state
    char m_searchBuffer[256] = {};
    std::string m_searchLower;
    bool m_autoScroll = true;
    bool m_scrollToBottom = false;
    std::atomic<bool> m_paused{false};
    bool m_clearOnPlayBuild = false;
    bool m_collapseRepeated = true;
    int m_sourceFilter = 0;
    int m_lastSelectedRow = -1;
    std::unordered_set<uint64_t> m_selectedMessageIds;

    // Cached filtered view to avoid rendering every message every frame
    std::vector<int> m_filteredIndices;
    std::vector<RenderRow> m_renderRows;
    bool m_filterDirty = true;
    size_t m_lastMessageCount = 0;
    size_t m_lastPendingCount = 0;
    size_t m_totalEligibleCount = 0;
    bool m_lastShowWarning = true;
    bool m_lastShowError = true;
    bool m_lastShowCritical = true;
    bool m_lastShowInfo = true;
    bool m_lastShowDebug = true;
    int m_lastSourceFilter = 0;
    bool m_lastCollapseRepeated = true;
    uint32_t m_lastSearchHash = 0;
    std::array<int, 5> m_levelCounts{};

    // Message limits
    static constexpr size_t MAX_MESSAGES = 1000;
};
