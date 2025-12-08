/*
 * CompilePanel.h
 * Small UI panel that displays compile progress and diagnostics for managed scripts.
 */
#pragma once

#include <string>

class CompilePanel {
public:
    static void Initialize();
    static void Shutdown();
    static void Render();
};
