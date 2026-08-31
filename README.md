# GrapeEngine

A custom C++17 / OpenGL / CUDA game engine developed at DigiPen Institute of Technology, built by a team of seven for the Software Engineering Project III & IV curriculum.

**Team:** Chaotic Grapes

---

## Overview

GrapeEngine is a full 2D game engine spanning an archetype-based ECS, a custom 2D physics solver, GPU-accelerated CUDA simulation, a modern OpenGL rendering pipeline, C# scripting via CoreCLR, FMOD audio, and a complete scene/serialization system — supporting both an in-editor workflow and standalone game export.

## Architecture

### Entity Component System (ECS)
- Archetype- and chunk-based storage (`Archetype`, `Chunk`, `ComponentRegistry`) for cache-efficient component iteration
- Signature-based system scheduling with an explicit `SystemDependencyGraph` for ordering system execution
- Event dispatch system (`EventDispatcher`) decoupling gameplay systems from one another
- Prefab system (`PrefabManager`) for reusable, configurable entity templates

### 2D Physics
A custom physics pipeline (`physics2d/`) built in-house, including:
- Broadphase and narrowphase collision detection (`Broadphase2D`, `Narrowphase2D`)
- Continuous collision detection (CCD) to prevent fast-moving object tunneling
- Contact management and constraint solving (`ContactManager2D`, `Solver2D`)
- Island building for isolating independent groups of colliding bodies (`IslandBuilder2D`)
- SIMD-accelerated math kernels (SSE intrinsics) with scalar fallback for hot paths
- A deterministic, static-partition parallel-for helper (multithreaded worker pool) for parallelizing physics stages across cores

### GPU-Accelerated Simulation (CUDA)
- **Boid flocking simulation** — GPU spatial-hashing kernels (cell assignment + cell-start lookup construction) followed by a flocking kernel applying separation, alignment, and cohesion steering forces
- **GPU particle system** built on the same spatial-hashing architecture
- **CUDA-OpenGL interop** for direct GPU-resident rendering, eliminating CPU-GPU transfer overhead
- **Result:** simulation throughput scaled 100x (5,000 → 500,000+ agents) at a fixed 60 FPS latency budget

### Rendering
- Forward+ lighting pipeline with a dedicated `LightManager`
- Post-processing passes: HDR bloom, god rays (`GodRayPass`), underwater distortion (`WaterDistortionPass`)
- PBR materials with normal/MRA (metallic-roughness-AO) maps
- Sprite batching, sprite-sheet animation, and tilemap rendering with lighting support
- Multi-viewport rendering via a `RenderGraph` and `FrameBuffer` abstraction
- GPU-accelerated object-selection pipeline using async, double-buffered pixel readback (`PixelBufferObject`) for interactive editor state inspection

### C# Scripting (CoreCLR)
- Live C# scripting layer with a managed API (`GrapeEngine.Scripting`) covering Math, Events, Components, Systems, and Gameplay helpers
- A native-to-managed interop layer (`Interop_*` bridge files) exposing engine subsystems — physics, audio, input, prefabs, save games, state machines, world/scene access — to C# scripts
- Background script compilation with editor hot-reload; standalone exports compile scripts into a `GameScripts.dll`

### Core Services
- Custom memory manager with global allocation override hooks
- Resource manager for asset loading/caching (`TextureCache`, `ResourceManager`)
- Save/load system (`SaveGameManager`) with entity serialization
- Finite state machine service (`StateMachine`)
- Device/input management and a frame-accurate time system

### Audio
FMOD-based audio engine with a cue registry system for organizing and triggering sound events, plus audio diagnostics tooling.

### Serialization
JSON-based entity and configuration serialization (via nlohmann/json), supporting scene save/load and project configuration.

## Tech Stack

**Core:** C++17, CUDA, OpenGL, GLSL
**Scripting:** C# (.NET CoreCLR)
**Audio:** FMOD
**Libraries:** GLM, GLFW, GLAD, Dear ImGui, nlohmann/json, stb_image

```text
└── GrapeEngine/
   ├── assets/         # Shared engine assets (textures, shaders, etc.)
   ├── cmake/          # CMake modules and configuration
   ├── editor/         # Editor source and headers
   ├── engine/
   │   ├── include/    # ecs, physics2d, graphics, cuda, scripting, services, audio, serialization
   │   └── src/        # corresponding implementations
   ├── externals/      # Third-party dependencies
   ├── managed/        # C# scripting API and tools
   ├── runtime/        # Runtime assets/binaries for game export
   ├── TestSuite/      # Sample project and demo scenes
   ├── CMakeLists.txt
   ├── run.bat         # Configure/build editor in Debug + Release
   └── clean.bat
```

## Team Roster & Credits

Full team roster, instructor list, and third-party library licenses are listed in `ChaoticGrapes_ReadMe.txt`.

*DigiPen Institute of Technology academic project (GAM200/250)*
