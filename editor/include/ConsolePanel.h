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
    void Initialize(ImFont* mainFont, ImFont* boldFont, ImFont* symbolsFont);
    void Shutdown();

    // -------------------------------------------------------------------------
    // Rendering
    // -------------------------------------------------------------------------
    void Render();

    // -------------------------------------------------------------------------
    // Message Management
    // -------------------------------------------------------------------------
    void AddMessage(LogLevel level, LogSource source, const std::string& timestamp, const std::string& message);
    void Clear();
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
    void _renderToolbar(const std::vector<ConsoleMessage>& snapshot);
    void _renderMessages(const std::vector<ConsoleMessage>& snapshot);
    void _renderMessageRow(const std::vector<ConsoleMessage>& snapshot, const RenderRow& row, int rowIndex, float messageWrapWidth, float badgeMinWidth);

    // -------------------------------------------------------------------------
    // Filtering
    // -------------------------------------------------------------------------
    void _rebuildFilterCache(const std::vector<ConsoleMessage>& snapshot);
    bool _shouldDisplayMessage(const ConsoleMessage& msg, const std::string& searchLower) const;
    bool _messageMatchesSearch(const ConsoleMessage& msg, const std::string& searchLower) const;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    ImVec4 _getColorForLevel(LogLevel level) const;
    ImVec4 _getBadgeColorForLevel(LogLevel level) const;
    // const char* _getLevelIcon(LogLevel level) const;
    const char* _getLevelText(LogLevel level) const;
    const char* _getSourceText(LogSource source) const;
    void _drawPill(const char* text, const ImVec4& bgColor, const ImVec4& textColor, float minWidth = 0.0f) const;
    bool _renderLevelChip(const char* id, const char* label, const ImVec4& color, bool active, int count);
    void _flushPendingMessages();
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
