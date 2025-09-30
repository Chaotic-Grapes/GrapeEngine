#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#define CREATE_WINDOW(title, width, height) WindowManager::CreateWindow(title, width, height)
#define CREATE_CHILD_WINDOW(title, width, height, parent) WindowManager::CreateWindow(title, width, height, parent)
#define DESTROY_WINDOW(window) WindowManager::DestroyWindow(window)
#define DESTROY_ALL_WINDOWS() WindowManager::DestroyAll();

#include <vector>
#include "ecs/ISystem.h"
#include "Window.h"

#ifdef CreateWindow
#undef CreateWindow
#endif

class WindowManager final : public Engine::ISystem {
public:
    ~WindowManager() override;

    // Create a new window, or a child window
    static Window* CreateWindow(const std::string& title, int width, int height, const Window* parent = nullptr);

    // Destroy a specific window
    static void DestroyWindow(const Window* window);

    // Destroy all windows
    static void DestroyAll();

    static const std::vector<Window*>& GetWindows();
    static Window* GetMainWindow();

    // System interface
    void OnCreate() override;
    void OnUpdate() override;
    std::string Name() const override;

private:
    static std::vector<Window*> m_windows;
};

#endif