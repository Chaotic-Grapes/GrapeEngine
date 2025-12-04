# GrapeEngine Project Structure (Refactored)

## Architecture Overview

GrapeEngine now follows a **three-project architecture** with clean separation:

```
GrapeEngine/
├── engine/              # Core Engine Library (NO Editor dependencies)
│   ├── CMakeLists.txt   # Engine library build configuration
│   ├── include/         # Engine public headers
│   └── src/             # Engine implementation
│
├── editor/              # Editor Executable (Links GrapeEngineLib + ImGui)
│   ├── CMakeLists.txt   # Editor build configuration
│   ├── include/         # Editor headers
│   └── src/             # Editor implementation (panels, tools, ImGui)
│
├── runtime/             # Runtime/Game Executable (Links GrapeEngineLib only)
│   └── CMakeLists.txt   # Runtime build configuration
│
├── src/game/            # Game entry point
│   └── GameMain.cpp
│
└── CMakeLists.txt       # Root configuration (orchestrates build)
```

## Build Targets

### 1. **GrapeEngineLib** (Engine Library)
- **Type**: Static or Shared library (configurable)
- **Dependencies**: glfw, glad, glm, freetype, fmod, nethost
- **NO ImGui**: Pure engine code, no editor dependencies
- **Can be**: 
  - Linked by Editor
  - Linked by Runtime/Game
  - Wrapped by EngineInterop for C# scripting
  - Distributed as reusable SDK

### 2. **GrapeEditor** (Editor Executable)
- **Depends on**: GrapeEngineLib + ImGui + ImGuizmo
- **Includes**: Level editor, viewport, inspector, hierarchy, asset browser
- **Output**: `GrapeEngine.exe` (editor)
- **USE_IMGUI**: Defined (enables editor UI code paths)

### 3. **GrapeRuntime** (Game Executable)
- **Depends on**: GrapeEngineLib only
- **NO ImGui**: Pure game runtime for distribution
- **Output**: `<ProjectName>Game.exe` (e.g., `EchoesBelowGame.exe`)
- **Auto-named**: Based on project folder with ProjectSettings.json

## Build Configuration

### CMake Options

```bash
# Build editor only (default)
cmake -DBUILD_EDITOR=ON -DBUILD_GAME=OFF ..

# Build game only
cmake -DBUILD_EDITOR=OFF -DBUILD_GAME=ON ..

# Build both
cmake -DBUILD_EDITOR=ON -DBUILD_GAME=ON ..

# Engine as shared library (DLL) - default
cmake -DBUILD_ENGINE_SHARED=ON ..

# Engine as static library
cmake -DBUILD_ENGINE_SHARED=OFF ..
```

### Using Existing Build Scripts

The project includes convenience batch scripts:

- `script_build_editor.bat` - Builds editor only
- `script_build_game.bat` - Builds game/runtime only
- `script_build_all.bat` - Builds both
- `script_run_editor.bat` - Runs the editor
- `script_run_game.bat` - Runs the game

## Dependency Flow

```
┌─────────────────────┐
│   GrapeEditor.exe   │  (Editor with ImGui)
│                     │
│  - ImGui Panels     │
│  - Level Tools      │
│  - Asset Browser    │
└──────────┬──────────┘
           │ links
           ▼
┌─────────────────────┐
│  GrapeEngineLib     │  (Core Engine - NO ImGui)
│   (lib or dll)      │
│                     │
│  - ECS              │
│  - Rendering        │
│  - Physics          │
│  - Systems          │
│  - Scripting        │
└──────────┬──────────┘
           ▲ links
           │
┌──────────┴──────────┐
│  GrapeRuntime.exe   │  (Standalone Game)
│                     │
│  - Game Entry Point │
│  - NO Editor Code   │
└─────────────────────┘
```

## Benefits of This Architecture

### ✅ Clean Separation
- Engine is pure C++, no editor contamination
- Editor is completely separate from runtime
- Runtime has zero editor overhead

### ✅ Reusability
- Engine can be distributed as SDK
- Multiple projects can link the same engine library
- Runtime can be built without editor installed

### ✅ C# Scripting Ready
- Clean engine library can be wrapped by EngineInterop
- No editor dependencies polluting managed API
- Follows industry patterns (Unity, Godot)

### ✅ Build Flexibility
- Build editor for development
- Build runtime for distribution
- Build both simultaneously
- Choose static or shared engine library

## Migration from Old Structure

### What Changed

**Before** (Monolithic):
```
CMakeLists.txt          # Everything in one file
  ├── Collect all sources
  ├── Create GrapeEngine target (editor)
  └── Create GrapeGame target (runtime)
```

**After** (Modular):
```
CMakeLists.txt          # Orchestrates subprojects
  ├── add_subdirectory(engine)   → GrapeEngineLib
  ├── add_subdirectory(editor)   → GrapeEditor
  └── add_subdirectory(runtime)  → GrapeRuntime
```

### Target Name Changes

| Old Name     | New Name       | Type       |
|--------------|----------------|------------|
| GrapeEngine  | GrapeEditor    | Executable |
| GrapeGame    | GrapeRuntime   | Executable |
| (none)       | GrapeEngineLib | Library    |

## C# Scripting Integration

### Script API Exports

**When engine is SHARED library**:
- Exports come from `GrapeEngineNative.dll`
- C# scripts reference the DLL

**When engine is STATIC library**:
- Exports come from executables
- Editor exports for editor builds
- Runtime exports for game builds

### Build Process

1. **Engine Library** builds first
2. **Editor/Runtime** link against engine
3. **C# Script API** (`GrapeEngine.Scripting.dll`) builds
4. **Game Scripts** compile referencing Script API
5. **Everything** outputs to same directory

## Next Steps for Full Refactor

This completes **Step 4: Restructure Project Layout** from the TODO.

### Remaining Refactor Steps:
1. ✅ Remove ImGui from RendererSystem (Step 1)
2. ✅ Move EditorCamera to editor (Step 2)
3. ✅ Create callback system for Engine→Editor (Step 3)
4. ✅ **Restructure project layout (Step 4) ← COMPLETED**
5. ⬜ Update build system testing
6. ⬜ Validate clean dependency graph
7. ⬜ Create EngineInterop project (C++/CLI bridge)
8. ⬜ Create EngineAPI project (Pure C# API)

## Building the Project

### Visual Studio
1. Open `CMakeLists.txt` (root) in Visual Studio
2. Configure CMake options in settings
3. Build → Build All
4. Editor: Run `GrapeEditor` target
5. Game: Run `GrapeRuntime` target

### Command Line
```bash
# Configure
cmake -B build -DBUILD_EDITOR=ON -DBUILD_GAME=ON

# Build
cmake --build build --config Debug

# Outputs
build/Debug/GrapeEngine.exe          # Editor
build/Debug/<ProjectName>Game.exe    # Runtime
build/Debug/GrapeEngineNative.dll    # Engine (if shared)
```

## Troubleshooting

### "Cannot find GrapeEngineLib"
- Ensure engine/CMakeLists.txt exists
- Check root CMakeLists.txt has `add_subdirectory(engine)`

### "ImGui not found" in Editor
- Verify editor/CMakeLists.txt links `imgui` target
- Check `USE_IMGUI` is defined for editor

### "Undefined reference to ScriptAPI_*"
- For shared library: Check exports from engine DLL
- For static library: Check editor/runtime has /EXPORT flags

### Game Executable Name Wrong
- Set `-DGAME_OUTPUT_NAME=<name>` in CMake
- Or ensure ProjectSettings.json exists in project folder

## License
Copyright (C) 2025 DigiPen Institute of Technology.
