# GrapeEngine

GrapeEngine is a custom game engine developed for academic and personal projects at DigiPen.

---

## Getting Started

### Prerequisites

- **C++ Compiler** (Visual Studio 2022 or equivalent)
- **CMake:** Version 3.13 or later
- **.NET SDK:** Version 9.0 or later (for C# scripting support)
- **Git:** (for cloning the repository)

### Cloning the Repository

```bash
git clone https://github.com/Chaotic-Grapes/GrapeEngine.git
cd GrapeEngine
```

### Building the Engine

1. **Build:** Run `script_build.bat` (configures for Visual Studio 2022 x64 Debug)
2. **Run:** Execute `script_run.bat` or run `build/Debug/GrapeEngine.exe`

---

## Game Concept

GrapeEngine is a 2D game engine built for educational purposes at DigiPen. Core features include:
- Entity Component System (ECS) architecture
- **C# CoreCLR Scripting** - Write game logic in C#
- 2D Physics with collision detection
- OpenGL rendering pipeline
- FMOD audio integration
- Scene management and serialization

---

## C# Scripting

GrapeEngine supports C# scripting through CoreCLR integration. Game scripts are **separate C# projects** in the `scripts/` folder, for now.

### Quick Start

1. **Create a game script project:**
   ```powershell
   cd scripts
   dotnet new classlib -n MyGame -f net9.0
   cd MyGame
   dotnet add reference ../GrapeEngine.ScriptAPI/GrapeEngine.ScriptAPI.csproj
   ```

2. **Write scripts:**
   ```csharp
   using GrapeEngine.ScriptAPI;
   
   namespace MyGame;
   
   public class PlayerController : ScriptBehaviour
   {
       protected override void OnUpdate()
       {
            // Code here
       }
   }
   ```

---

## Demo Usage

The engine includes four test scenes:

1. **Physics & Collision 2D Test** - Interactive physics simulation
   - Controls: `P` (toggle step mode), `Space` (step physics), Arrow keys (move triangle)

2. **Graphics & Art Pipeline Test** - Comprehensive rendering system showcase with 10 test cases:
   - Basic Graphics, Debug Drawing, Basic Sprites, Background Rendering
   - Sprite Scaling, Sprite Rotation, Sprite Animation, Multi-Animation  
   - Performance/Batch Stress Testing, Font System Testing
   - Controls: `G` (cycle through test cases)

3. **Serialization Check Test** - Save/load system validation

4. **Memory Tracking Test** - Custom memory management system validation
   - Demonstrates allocation tracking, leak detection, and memory usage statistics

### Audio System

The GrapeEngine features a comprehensive FMOD-powered audio system:
- FMOD Integration: Professional audio engine for 3D spatial audio
- Audio Debug Interface: Accessible via DebugUI → Audio Monitor
- Features: Sound loading and caching, real-time playback controls, volume and pitch adjustment, 3D positional audio support, audio resource management

The audio system is integrated across all test scenes and can be monitored through the debug interface.

Run the engine and select a test scene from the menu.

---

## Team Roster

**Chaotic Grapes Development Team**

- **Muhammad Nur Fadzly Bin Zulkifli** – Product Manager  
  *Mechanics & Production Champion* (muhammadnurfadzly.b@digipen.edu)

- **Mohammed Ubaidillah Bin Mohammed Izam** – Art Lead  
  *Art Animation, Environment & Concepts Champion* (m.binmohammedizam@digipen.edu)

- **Hooi Kai Ru** – Design Lead  
  *UI Design & Story Champion* (h.kairu@digipen.edu)

- **Foo Rui Qin** – Technical Lead  
  *Engine & Input Champion* (ruiqin.foo@digipen.edu)

- **Samantha Leong Sher Yen** – Programmer  
  *Animation Editor & Debugging Champion* (s.leong@digipen.edu)

- **Choi Meng Yew** – Graphics Programmer  
  *Graphics & Rendering Champion* (choi.m@digipen.edu)

- **Daniel Kay Neo Zuo Feng** – Systems Programmer  
  *Level Editor, Physics & Collision Champion* (k.danielneozuofeng@digipen.edu)

- **Dalton Koh Shi Hao** – Gameplay Programmer  
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

*DigiPen Institute of Technology academic project*

---

## Credits

**Third-Party Libraries:**

**FMOD Sound System** - Copyright (C) Firelight Technologies Pty Ltd.  
Licensed under FMOD Engine License.

**.NET CoreCLR** - Copyright (C) .NET Foundation and Contributors.  
Licensed under the MIT License.

**GLM (OpenGL Mathematics)** - Copyright (C) 2005 - 2014 G-Truc Creation.  
Licensed under the MIT License.

**GLFW** - Copyright (C) 2002-2006 Marcus Geelnard, 2006-2019 Camilla Löwy.  
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

---

## License

Copyright (C) 2025 DigiPen Institute of Technology.
All rights reserved.
