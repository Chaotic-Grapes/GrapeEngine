# GrapeEngine

GrapeEngine is a custom game engine developed for academic projects at DigiPen.

## Getting Started

### Prerequisites

- **Visual Studio 2022** with "Desktop development with C++" (C++23 toolset)
- **CMake:** Version 3.13 or later
- **.NET SDK:** Version 9.0 or later (for C# scripting support)
- **Git:** (for cloning the repository)
- **Doxygen:** (optional, for documentation generation)

### Cloning the Repository

```bash
git clone https://github.com/Chaotic-Grapes/GrapeEngine.git
cd GrapeEngine
```

### Building the Engine

You can build using the provided per-target scripts or the interactive menu.

Interactive Menu (recommended):
1. Run `script_build_menu.bat` and choose Editor, Game, Build All, or Clean.
2. The menu supports Debug and Release builds and will pause so you can
   inspect output.

Per-target scripts:
- Editor: `script_build_editor.bat` -> outputs to `build\<Config>` (e.g. `build\Debug\GrapeEngine.exe`)
- Game: `script_build_game.bat` -> outputs to `build_game\export\<ProjectName>\<Config>` (e.g. `build_game\export\EchoesBelow\Release`)

Notes:
- The CMake configuration requests **C++23**.
- `script_build_game.bat` will prompt you to pick a project if more than one is listed in
  `Documents\Grape Engine\EditorSettings.json`. You can also pass a project name as the
  second argument.
- You can export projects outside the repo by passing `-DEXPORT_PROJECT_DIR="C:\Path\To\Project"`
  to CMake (the batch script passes this automatically when a path is selected).
- The standalone game executable name may be auto-detected from the first game project
  folder found (a folder containing `ProjectSettings.json`). You can override this with
  the CMake cache variable `GAME_OUTPUT_NAME`.

## Project Structure

```
└── GrapeEngine/
   ├── assets/         # Shared engine assets (textures, shaders, etc.)
   ├── build/          # Editor build output (generated)
   ├── build_game/     # Standalone game export output (generated)
   ├── cmake/          # CMake modules and configuration
   ├── editor/         # Editor source and headers
   ├── engine/         # Engine source and headers
   ├── externals/      # Third-party dependencies
   ├── managed/        # C# scripting and tools
   ├── runtime/        # Runtime assets/binaries for game export
   ├── TestSuite/      # Sample project (To add in editor for testing)
   ├── CMakeLists.txt  # Main CMake configuration
   └── script_*.bat    # Build, run and clean scripts
```

## Game Concept

GrapeEngine is a 2D game engine built for educational purposes at DigiPen. Core features include:
- Entity Component System (ECS) architecture
- **C# CoreCLR Scripting** - Write game logic in C#
- 2D Physics with collision detection
- OpenGL rendering pipeline
- FMOD audio integration
- Scene management and serialization

## C# Scripting

GrapeEngine supports C# scripting through CoreCLR integration. Scripts are plain
`.cs` files placed inside your project folder (the same folder that contains
`ProjectSettings.json`).

The editor compiles scripts in the background and hot-reloads systems on change.
Standalone exports compile scripts into `GameScripts.dll` during the export step.

### Quick Start

1. **Create a script file** in your project folder (or use the editor's script templates).
2. **Write a system:**
   ```csharp
   using GrapeEngine.Scripting.Systems;
   using GrapeEngine.Scripting.Systems.Attributes;

   namespace MyGame;

   [System(SystemGroup.Update, SystemRunMode.PlayOnly)]
   public class PlayerController : SystemBase
   {
       protected override void OnUpdate()
       {
           // Your gameplay logic here.
       }
   }
   ```
3. **Save the file** and let the editor recompile and hot-reload.

## Demo Usage (TestSuite)

The `TestSuite` project contains the current demo scenes:

1. **AudioCrossfadeDemo** - Audio crossfade showcase
2. **AudioCrossfadeTarget** - Audio crossfade target setup
3. **GUITest** - UI/GUI test scene
4. **LayerSystemSample** - Layer system sample scene
5. **OwnerTracking_\*** - Owner tracking sample for resource cache testing between scenes
7. **SpriteAnimationSample** - Sprite animation sample scene

Run the engine/editor and open a scene from `TestSuite/Scenes`.

## Team Roster

**Chaotic Grapes Development Team**

- **Muhammad Nur Fadzly Bin Zulkifli** - Product Manager  
  *Mechanics & Production Champion* (muhammadnurfadzly.b@digipen.edu)

- **Mohammed Ubaidillah Bin Mohammed Izam** - Art Lead  
  *Art Animation, Environment & Concepts Champion* (m.binmohammedizam@digipen.edu)

- **Hooi Kai Ru** - Design Lead  
  *UI Design & Story Champion* (h.kairu@digipen.edu)

- **Foo Rui Qin** - Technical Lead  
  *Engine & Input Champion* (ruiqin.foo@digipen.edu)

- **Samantha Leong Sher Yen** - Programmer  
  *Animation Editor & Debugging Champion* (s.leong@digipen.edu)

- **Choi Meng Yew** - Graphics Programmer  
  *Graphics & Rendering Champion* (choi.m@digipen.edu)

- **Dalton Koh Shi Hao** - Gameplay Programmer  
  *Gameplay & AI Champion* (d.koh@digipen.edu)

**Instructors:**
- Elie Hosry
- Parminder Singh
- Cheng Ding Xiang
- Goh Dian Yanng
- Holger Liebnitz
- Raymond Teo
- Choon Wee Keh
- Vuk Krakovic
- Gavin Parker
- Rudy Castan

*DigiPen Institute of Technology academic project*

## Credits

**Third-Party Libraries:**

**FMOD Sound System** - Copyright (C) Firelight Technologies Pty Ltd.  
Licensed under FMOD Engine License.

**.NET CoreCLR** - Copyright (C) .NET Foundation and Contributors.  
Licensed under the MIT License.

**GLM (OpenGL Mathematics)** - Copyright (C) 2005 - 2014 G-Truc Creation.  
Licensed under the MIT License.

**GLFW** - Copyright (C) 2002-2006 Marcus Geelnard, 2006-2019 Camilla Lowy.  
Licensed under the zlib/libpng License.

**GLAD** - Copyright (C) 2013-2020 David Herberth.  
Licensed under the MIT License.

**stb_image** - Copyright (C) 2017 Sean Barrett.  
Licensed under the MIT License or Public Domain.

**Dear ImGui** - Copyright (C) 2014-2024 Omar Cornut.  
Licensed under the MIT License.

**nlohmann/json** - Copyright (C) 2013-2022 Niels Lohmann.  
Licensed under the MIT License.

Image "johnPork.png" by Dariomartinezpinto, CC BY 4.0, via Wikimedia Commons. (Modified for project use.)

## License

Copyright (C) 2026 DigiPen Institute of Technology.
All rights reserved.
