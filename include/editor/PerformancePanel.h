/* Start Header *****************************************************************/
/*!
\file   PerformancePanel.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\date   25th November 2025

\brief
Declares the in-editor performance monitoring panel used by the Level Editor.
*/
/* End Header *******************************************************************/

#ifndef PERFORMANCE_PANEL_H
#define PERFORMANCE_PANEL_H

#include <imgui.h>
#include <string>

class PerformancePanel {
public:
    void Initialize(ImFont* mainFont, ImFont* boldFont);
    void Shutdown();

    // Render accepts whether the editor is currently playing; monitoring is paused when not playing
    void Render(bool isPlaying);

private:
    ImFont* m_mainFont = nullptr;
    ImFont* m_boldFont = nullptr;
    bool m_initialized = false;

#ifdef _WIN32
    // Previous system times for CPU usage calculation
    unsigned long long m_prevIdle = 0;
    unsigned long long m_prevKernel = 0;
    unsigned long long m_prevUser = 0;
#endif
};

#endif
