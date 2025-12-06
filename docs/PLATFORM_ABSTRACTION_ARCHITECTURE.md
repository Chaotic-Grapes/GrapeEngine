# Platform Abstraction Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        Editor Application                        │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │              EditorApplication / EditorService             │ │
│  │                (ImGui-based level editor)                  │ │
│  └─────────────────────┬──────────────────────────────────────┘ │
│                        │ Uses only interfaces                    │
│                        ▼                                          │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │         Platform::IPlatformContext (interface)             │ │
│  │   ┌──────────────┬──────────────┬──────────────────────┐  │ │
│  │   │   IWindow    │ IRenderDevice│   IInputSystem       │  │ │
│  │   │ (interface)  │ (interface)  │   (interface)        │  │ │
│  │   └──────────────┴──────────────┴──────────────────────┘  │ │
│  └────────────────────────────────────────────────────────────┘ │
└──────────────────────┬──────────────────────────────────────────┘
                       │ Links against
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                       Engine Library (DLL)                       │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                    Engine::Application                      │ │
│  │             (Exposes IPlatformContext*)                     │ │
│  └─────────────────────┬──────────────────────────────────────┘ │
│                        │ Creates and owns                        │
│                        ▼                                          │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │       Platform::GLFWPlatformContext (concrete)             │ │
│  │   ┌──────────────┬──────────────┬──────────────────────┐  │ │
│  │   │ GLFWWindow   │ OpenGLRender │  GLFWInputSystem     │  │ │
│  │   │ (concrete)   │ Device       │  (concrete)          │  │ │
│  │   │              │ (concrete)   │                      │  │ │
│  │   └──────┬───────┴──────┬───────┴──────────┬───────────┘  │ │
│  │          │              │                  │               │ │
│  │          └──────────────┼──────────────────┘               │ │
│  │                         │                                   │ │
│  └─────────────────────────┼───────────────────────────────────┘ │
│                            │ Uses directly                       │
│                            ▼                                     │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │               GLFW + OpenGL (Platform Layer)               │ │
│  │  - glfwCreateWindow()  - glViewport()  - glfwPollEvents()  │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

Key Benefits:
═══════════════

1. Editor depends only on abstract interfaces
   → No GLFW headers in editor code (except ImGui backend init)

2. Engine owns all platform code
   → GLFW initialization, window management, OpenGL context

3. Easy to add new platforms
   → Implement IWindow, IRenderDevice, IInputSystem for new platform
   → No editor changes needed

4. Clean architecture
   → Separation of concerns
   → Testable (can mock interfaces)
   → Follows industry patterns (Godot, Banshee, Unity)

Example: Creating a Window in Editor
════════════════════════════════════

BEFORE (Direct GLFW coupling):
  GLFWwindow* window = glfwCreateWindow(...);
  glfwMakeContextCurrent(window);

AFTER (Platform abstraction):
  auto* platform = engine->GetPlatformContext();
  Platform::WindowCreateInfo info{...};
  auto* window = platform->CreateWindow(info);
  // window is IWindow*, no GLFW exposure

Flow: Editor → IPlatformContext → GLFWPlatformContext → GLFW
```

## Comparison with Other Engines

### Godot 4 (DisplayServer)
```
Editor → DisplayServer (interface) → DisplayServerWindows/X11/etc.
```

### Banshee Engine
```
Tools → CoreApplication → Platform (interface) → Win32Platform/LinuxPlatform
```

### GrapeEngine (This Implementation)
```
Editor → IPlatformContext → GLFWPlatformContext → GLFW
```

All follow the same pattern: **abstraction layer between editor and platform**.
