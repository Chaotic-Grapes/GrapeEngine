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
#include "core/Logger.h"

struct ConsoleMessage {
    std::string Timestamp;
    LogLevel Level;
    std::string Content;
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

private:
    // -------------------------------------------------------------------------
    // UI Rendering
    // -------------------------------------------------------------------------
    void _renderToolbar();
    void _renderMessages();
    void _renderMessage(const ConsoleMessage& msg, int index);

    // -------------------------------------------------------------------------
    // Filtering
    // -------------------------------------------------------------------------
    bool _shouldDisplayMessage(const ConsoleMessage& msg) const;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    ImVec4 _getColorForLevel(LogLevel level) const;
    const char* _getLevelIcon(LogLevel level) const;
    const char* _getLevelText(LogLevel level) const;

    // -------------------------------------------------------------------------
    // Members
    // -------------------------------------------------------------------------
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    ImFont* m_symbolsFont = nullptr;

    std::vector<ConsoleMessage> m_messages;
    std::mutex m_messagesMutex;
    std::atomic<bool> m_initialized{false};

    // Filter settings
    bool m_showInfo = true;
    bool m_showDebug = true;
    bool m_showWarning = true;
    bool m_showError = true;
    bool m_showCritical = true;

    // UI state
    char m_searchBuffer[256] = {};
    bool m_autoScroll = true;
    bool m_scrollToBottom = false;
    int m_selectedMessageIndex = -1;
    int m_hoveredMessageIndex = -1;

    // Message limits
    static constexpr size_t MAX_MESSAGES = 1000;
};
