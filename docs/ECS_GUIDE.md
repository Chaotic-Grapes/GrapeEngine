# GrapeEngine ECS Guide

**Version:** 2.0  
**Last Updated:** October 21, 2025

---

## Table of Contents

1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [Core Concepts](#core-concepts)
   - [Entity](#entity)
   - [Components](#components)
   - [Systems](#systems)
   - [World](#world)
   - [Archetypes](#archetypes)
4. [Getting Started](#getting-started)
   - [Creating Entities](#creating-entities)
   - [Adding Components](#adding-components)
   - [Querying Entities](#querying-entities)
   - [Removing Components](#removing-components)
   - [Destroying Entities](#destroying-entities)
5. [Components Reference](#components-reference)
   - [Core Components](#core-components)
   - [Transform Components](#transform-components)
   - [Physics Components (2D)](#physics-components-2d)
   - [Physics Components (3D)](#physics-components-3d)
   - [Rendering Components](#rendering-components)
   - [Camera Components](#camera-components)
6. [Built-in Systems](#built-in-systems)
   - [TransformSystem](#transformsystem)
   - [PhysicsSystem](#physicssystem)
   - [RendererSystem](#renderersystem)
   - [LifetimeSystem](#lifetimesystem)
7. [Hierarchy & Parenting](#hierarchy--parenting)
8. [Scene Management](#scene-management)
   - [Creating Scenes](#creating-scenes)
   - [Adding Systems to Scenes](#adding-systems-to-scenes)
   - [Scene Lifecycle](#scene-lifecycle)
   - [System Introspection & IDs](#system-introspection--ids)
   - [Saving & Loading Scenes](#saving--loading-scenes)
9. [Layer System](#layer-system)
10. [Advanced Topics](#advanced-topics)
    - [Entity Pooling](#entity-pooling)
    - [Component Iteration Performance](#component-iteration-performance)
    - [Archetype Changes](#archetype-changes)
    - [Cloning Entities](#cloning-entities)
    - [Performance Optimization Guide](#performance-optimization-guide)
11. [Best Practices](#best-practices)
12. [Performance Quick Reference](#performance-quick-reference)
13. [Common Patterns](#common-patterns)
14. [Troubleshooting](#troubleshooting)

---

## Introduction

GrapeEngine features a high-performance **Entity Component System (ECS)** architecture designed for efficient data-oriented game development. The ECS is built around three core principles:

- **Entities** are lightweight identifiers
- **Components** are pure data structures
- **Systems** contain logic that operates on components

This guide will walk you through all aspects of using the ECS in GrapeEngine.

---

## Architecture Overview

The ECS architecture in GrapeEngine follows these key design decisions:

### Archetype-Based Storage
Entities with identical component combinations are stored together in **Archetypes**. This provides:
- Excellent cache locality when iterating
- Efficient batch processing
- Fast component addition/removal

### Chunked Memory Layout
Each archetype stores components in **Chunks** (16KB by default, 256 entities per chunk). Benefits include:
- Predictable memory allocation
- Better cache utilization
- Reduced memory fragmentation

### Generation-Based Handles
Entity handles use index + generation counters to:
- Detect stale entity references
- Enable entity ID reuse
- Prevent use-after-free bugs

---

## Core Concepts

### Entity

An **Entity** is a lightweight identifier consisting of:
- `Index`: The entity's slot in the world
- `Generation`: Version counter to detect stale references

```cpp
struct Entity {
    EntityId Index;
    EntityId Generation;
    
    bool IsNull() const;
};
```

Entities are created and destroyed by the `World`, and can be packed/unpacked for storage:

```cpp
// Packing for storage
uint64_t packed = EntityUtils::Pack(entity);

// Unpacking from storage
Entity entity = EntityUtils::Unpack(packed);
```

### Components

**Components** are plain data structures (POD types) that are trivially copyable. All components must satisfy:
- `std::is_trivially_copyable_v<T>` returns true
- No virtual functions
- No heap allocations (no `std::string`, `std::vector`, etc.)

Example component:
```cpp
struct MyComponent {
    float Value = 0.0f;
    int Count = 0;
    bool Enabled = true;
    uint8_t _Pad0 = 0, _Pad1 = 0, _Pad2 = 0; // Padding for alignment
};
static_assert(std::is_trivially_copyable_v<MyComponent>);
```

### Systems

**Systems** are functions that process entities with specific components. Systems are added to scenes and run every frame:

```cpp
// System function signature
using System = std::function<void(Scene&, float)>;

// Example system
void MySystem(Scenes::Scene& scene, float dt) {
    auto& world = scene.GetWorld();
    world.Each<MyComponent>([](Entity e, MyComponent& comp) {
        comp.Value += 1.0f;
    });
}
```

### World

The **World** is the central ECS registry that manages:
- Entity lifecycle (creation, destruction)
- Component storage and access
- Archetype management
- Hierarchy tracking

```cpp
ECS::World world;

// Create entity
Entity e = world.Create();

// Add component
world.Add<MyComponent>(e, MyComponent{});

// Get component
MyComponent& comp = world.Get<MyComponent>(e);

// Check if has component
if (world.Has<MyComponent>(e)) { /* ... */ }

// Remove component
world.Remove<MyComponent>(e);

// Destroy entity
world.Destroy(e);
```

### Archetypes

**Archetypes** are internal storage structures that group entities by their component signature. When you add or remove components, entities move between archetypes.

You don't directly interact with archetypes, but understanding them helps optimize your code:
- Adding/removing components triggers archetype changes (expensive)
- Querying entities with the same components is very fast (same archetype)

---

## Getting Started

### Creating Entities

There are multiple ways to create entities:

#### 1. Empty Entity
```cpp
Entity e = world.Create();
```

#### 2. Entity with Components
```cpp
Entity e = world.Create(
    Components::LocalTransform{ Vector3D{0,0,0}, Quaternion{0,0,0,1}, Vector3D{1,1,1} },
    Components::Name{"MyEntity"}
);
```

#### 3. Entity in a Scene (with optional parent)
```cpp
// In a Scene class
Entity e = CreateEntity(); // No parent
Entity child = CreateEntity(parent); // With parent
```

#### 4. Entity on a Specific Layer
```cpp
// In a Scene class
Entity e = CreateOnLayer(layerId, 
    Components::LocalTransform{},
    Components::Name{"LayeredEntity"}
);
```

### Adding Components

#### Add Single Component
```cpp
world.Add<Components::Active>(e, Components::Active{true});
```

#### Add Multiple Components (one at a time)
```cpp
world.Add<Components::LocalTransform>(e, Components::LocalTransform{});
world.Add<Components::Name>(e, Components::Name{"Entity"});
```

#### Set Component (Add or Update)
```cpp
// If component exists, updates it; otherwise adds it
world.Set<Components::Active>(e, Components::Active{false});
```

### Querying Entities

#### Iterate Over All Entities with Specific Components
```cpp
world.Each<Components::LocalTransform, Components::Active>(
    [](Entity e, Components::LocalTransform& transform, Components::Active& active) {
        if (active.Enabled) {
            transform.Position.X += 1.0f;
        }
    }
);
```

#### Const Iteration
```cpp
const World& constWorld = world;
constWorld.Each<const Components::LocalTransform>(
    [](Entity e, const Components::LocalTransform& transform) {
        // Read-only access
    }
);
```

### Removing Components

```cpp
world.Remove<Components::Active>(e);
```

**Note:** Removing a component triggers an archetype change, which is relatively expensive.

### Destroying Entities

#### Single Entity
```cpp
world.Destroy(e);
```

#### All Entities (with hooks)
```cpp
world.DestroyAll(); // Calls per-entity destruction logic
```

#### Fast Clear (no hooks)
```cpp
world.Clear(); // Fast bulk clear, no per-entity logic
```

---

## Components Reference

### Core Components

#### `Components::Name`
Fixed-size entity name (64 bytes, null-terminated).

```cpp
struct Name {
    char Value[64] = {0};
};

// Usage
world.Add<Components::Name>(e, Components::Name{"MyEntity"});
```

#### `Components::TagMask`
32-bit bitmask for entity tags.

```cpp
struct TagMask {
    uint32_t Mask = 0;
};

// Usage
world.Add<Components::TagMask>(e, Components::TagMask{0x01}); // Tag 1
world.Add<Components::TagMask>(e, Components::TagMask{0x03}); // Tags 1 and 2
```

#### `Components::Active`
Enable/disable flag for entities.

```cpp
struct Active {
    bool Enabled = true;
    uint8_t _Pad0, _Pad1, _Pad2; // Padding
};

// Usage
world.Add<Components::Active>(e, Components::Active{true});
```

#### `Components::Lifetime`
Time remaining before entity is destroyed (used by `LifetimeSystem`).

```cpp
struct Lifetime {
    float Time = 0.0f; // Seconds
};

// Usage
world.Add<Components::Lifetime>(e, Components::Lifetime{5.0f}); // 5 seconds
```

### Transform Components

#### `Components::LocalTransform`
Transform relative to parent (or world if no parent).

```cpp
struct LocalTransform {
    Vector3D Position{0,0,0};
    Quaternion Rotation{0,0,0,1};
    Vector3D Scale{1,1,1};
};

// Usage
world.Add<Components::LocalTransform>(e, Components::LocalTransform{
    Vector3D{100, 200, 0},
    Quaternion::Identity,
    Vector3D{2, 2, 1}
});
```


#### `Components::WorldTransform`
Computed world-space transform matrix. Updated by the engine's `Transform` system (`ECS::TransformSystem`).

```cpp
struct WorldTransform {
    Matrix4x4 Matrix{};
    bool Dirty = true;
};

// Usage
world.Add<Components::WorldTransform>(e, Components::WorldTransform{});
// System will compute Matrix automatically
```

### Physics Components (2D)

#### `Components::Rigidbody2D`
2D rigid body physics properties.

```cpp
struct Rigidbody2D {
    float Mass = 1.0f;
    float InverseMass = 1.0f;
    float LinearDamping = 0.0f;
    float AngularDamping = 0.0f;
    float GravityScale = 1.0f;
    uint32_t Flags = 0; // bit 0: Kinematic, bit 1: UseGravity, bit 2: FixedRotation
};

// Usage - Static body
world.Add<Components::Rigidbody2D>(e, Components::Rigidbody2D{
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0
});

// Dynamic body with gravity
world.Add<Components::Rigidbody2D>(e, Components::Rigidbody2D{
    1.0f, 1.0f, 0.1f, 0.1f, 1.0f, 0x02 // 0x02 = UseGravity flag
});
```

#### `Components::LinearVelocity2D`
2D linear velocity (pixels/second).

```cpp
struct LinearVelocity2D {
    Vector2D Value{0.0f, 0.0f};
};

// Usage
world.Add<Components::LinearVelocity2D>(e, Components::LinearVelocity2D{
    Vector2D{100.0f, 0.0f}
});
```

#### `Components::AngularVelocity2D`
2D angular velocity (radians/second around Z-axis).

```cpp
struct AngularVelocity2D {
    float Value = 0.0f;
    float _Pad0, _Pad1, _Pad2;
};

// Usage - 90 degrees per second
world.Add<Components::AngularVelocity2D>(e, Components::AngularVelocity2D{90.0f});
```

#### `Components::Acceleration2D`
2D acceleration (pixels/second²).

```cpp
struct Acceleration2D {
    Vector2D Value{0.0f, 0.0f};
};

// Usage
world.Add<Components::Acceleration2D>(e, Components::Acceleration2D{
    Vector2D{0.0f, -980.0f} // Gravity
});
```

#### `Components::CircleCollider2D`
Circle collision shape.

```cpp
struct CircleCollider2D {
    float Radius = 0.5f;
    Vector2D Offset{0.0f, 0.0f};
    uint32_t LayerMask = 0xFFFFFFFF;
    uint32_t Flags = 0; // bit 0: IsTrigger
};

// Usage
world.Add<Components::CircleCollider2D>(e, Components::CircleCollider2D{
    25.0f, Vector2D{0,0}, 0xFFFFFFFF, 0
});
```

#### `Components::BoxCollider2D`
Axis-aligned or oriented rectangle collision shape.

```cpp
struct BoxCollider2D {
    Vector2D HalfExtents{0.5f, 0.5f};
    Vector2D Offset{0.0f, 0.0f};
    float Rotation = 0.0f; // Radians
    uint32_t LayerMask = 0xFFFFFFFF;
    uint32_t Flags = 0;
    uint32_t _Pad = 0;
};

// Usage
world.Add<Components::BoxCollider2D>(e, Components::BoxCollider2D{
    Vector2D{50, 50}, Vector2D{0,0}, 0.0f, 0xFFFFFFFF, 0, 0
});
```

#### `Components::PhysicsMaterial2D`
Physical material properties (friction, bounciness).

```cpp
struct PhysicsMaterial2D {
    float Friction = 0.2f;
    float Restitution = 0.0f; // Bounciness (0-1)
    float PositionCorrectPercent = 0.2f;
    float _Pad0 = 0.0f;
};

// Usage - Bouncy ball
world.Add<Components::PhysicsMaterial2D>(e, Components::PhysicsMaterial2D{
    0.1f, 0.8f, 0.2f, 0.0f
});
```

### Physics Components (3D)

#### `Components::Rigidbody`
3D rigid body (basic implementation).

```cpp
struct Rigidbody {
    float Mass = 1.0f;
    float InverseMass = 1.0f;
    float LinearDrag = 0.0f;
    float AngularDrag = 0.0f;
    uint32_t Flags = 0;
    uint32_t _Pad = 0;
};
```

#### `Components::Velocity`
3D linear velocity.

```cpp
struct Velocity {
    Vector3D Value{0.0f, 0.0f, 0.0f};
};
```

#### `Components::Acceleration`
3D acceleration.

```cpp
struct Acceleration {
    Vector3D Value{0.0f, 0.0f, 0.0f};
};
```

#### `Components::AngularVelocity`
3D angular velocity.

```cpp
struct AngularVelocity {
    Vector3D Value{0.0f, 0.0f, 0.0f};
};
```

#### `Components::BoxCollider`
3D box collider.

```cpp
struct BoxCollider {
    Vector3D HalfExtents{0.5f, 0.5f, 0.5f};
    uint32_t LayerMask = 0xFFFFFFFF;
    uint32_t _Pad = 0;
};
```

#### `Components::SphereCollider`
3D sphere collider.

```cpp
struct SphereCollider {
    float Radius = 0.5f;
    float _Pad0, _Pad1, _Pad2;
    uint32_t LayerMask = 0xFFFFFFFF;
    uint32_t _Pad3 = 0;
};
```

### Rendering Components

#### `Components::SpriteRenderer2D`
2D texture sprite rendering.

```cpp
struct SpriteRenderer2D {
    uint32_t TextureId = 0;
    Color Color{1.0f, 1.0f, 1.0f, 1.0f};
    Vector2D Tiling{1.0f, 1.0f};
    Vector2D Offset{0.0f, 0.0f};
    uint32_t _Pad = 0;
};

// Usage
world.Add<Components::SpriteRenderer2D>(e, Components::SpriteRenderer2D{
    textureId,
    Color{255, 255, 255, 255},
    Vector2D{1, 1},
    Vector2D{0, 0},
    0
});
```

#### `Components::ShapeCircle2D`
Debug circle rendering.

```cpp
struct ShapeCircle2D {
    float Radius = 0.5f;
    Vector2D Offset{0.0f, 0.0f};
    Color Color{1.f,1.f,1.f,1.f};
    float Thickness = 1.0f;
    bool Filled = false;
    uint8_t _Pad0, _Pad1, _Pad2;
};

// Usage - Filled circle
world.Add<Components::ShapeCircle2D>(e, Components::ShapeCircle2D{
    30.0f, Vector2D{0,0}, Color{255, 0, 0, 255}, 0.0f, true
});
```

#### `Components::ShapeBox2D`
Debug box rendering.

```cpp
struct ShapeBox2D {
    Vector2D HalfExtents{0.5f, 0.5f};
    Vector2D Offset{0.0f, 0.0f};
    Color Color{1.f,1.f,1.f,1.f};
    float Thickness = 1.0f;
    bool Filled = false;
    uint8_t _Pad0, _Pad1, _Pad2;
};

// Usage - Outline box
world.Add<Components::ShapeBox2D>(e, Components::ShapeBox2D{
    Vector2D{50, 50}, Vector2D{0,0}, Color{0, 255, 0, 255}, 2.0f, false
});
```

#### `Components::ShapeLine2D`
Debug line rendering.

```cpp
struct ShapeLine2D {
    Vector2D A{0.0f, 0.0f};
    Vector2D B{1.0f, 0.0f};
    Color Color{1.f,1.f,1.f,1.f};
    float Thickness = 1.0f;
    float _Pad0, _Pad1, _Pad2;
};

// Usage
world.Add<Components::ShapeLine2D>(e, Components::ShapeLine2D{
    Vector2D{-50, -50}, Vector2D{50, 50}, Color{255, 255, 0, 255}, 2.0f
});
```

#### `Components::ZIndex2D`
2D sorting/layering hint.

```cpp
struct ZIndex2D {
    int16_t ZOrder = 0; // Lower values drawn first
    int16_t _Pad0 = 0;
    int32_t _Pad1 = 0;
};
```

### Camera Components

#### `Components::Camera`
Camera configuration.

```cpp
struct Camera {
    bool IsOrthographic = false;
    uint8_t _Pad0, _Pad1, _Pad2;
    float FovY = 60.0f;
    float OrthoHeight = 10.0f;
    float Near = 0.1f;
    float Far = 1000.0f;
    float Aspect = 16.0f / 9.0f;
};

// Usage - Orthographic camera
world.Add<Components::Camera>(e, Components::Camera{
    true, 0, 0, 0, 60.0f, 10.0f, 0.1f, 1000.0f, 16.0f/9.0f
});
```

#### `Components::CameraMatrices`
Computed camera matrices (View, Projection, ViewProjection).

```cpp
struct CameraMatrices {
    Matrix4x4 View{};
    Matrix4x4 Projection{};
    Matrix4x4 ViewProjection{};
};
```

---

## Built-in Systems

### TransformSystem

Updates world-space transforms from local transforms and hierarchy relationships.

What it does:
- Propagates parent-to-child transforms
- Writes/refreshes `Components::WorldTransform` based on `Components::LocalTransform`

Usage:
```cpp
// Typically NOT required to add manually: the engine updates transforms after systems each frame.
// If you build a custom loop, you can call it explicitly:
AddSystem([](Scenes::Scene& s, float dt) {
    ECS::TransformSystem::Update(s.GetWorld(), dt);
}, "Transform System");
```

Important:
- In the default engine loop, transforms are updated automatically at the end of each frame. Avoid adding your own transform update unless you are managing a custom loop to prevent redundant work.

### PhysicsSystem

Handles 2D physics simulation including:
- Velocity integration
- Gravity application
- Collision detection and resolution
- Physics material (friction, restitution)
- Angular motion

**Usage:**
```cpp
// In Scene::OnLoad()
AddSystem([](Scenes::Scene& s, float dt) {
    ECS::PhysicsSystem::Update(s.GetWorld(), dt);
}, "Physics System");
```

**Required Components:**
- `Components::LocalTransform` (position updated)
- `Components::Rigidbody2D` (mass, flags)
- `Components::LinearVelocity2D` (velocity)
- Optional: `Components::AngularVelocity2D`, `Components::CircleCollider2D`, `Components::BoxCollider2D`, `Components::PhysicsMaterial2D`

**Example:**
```cpp
// Falling ball with gravity
Entity ball = world.Create(
    Components::LocalTransform{ Vector3D{400, 100, 0}, Quaternion::Identity, Vector3D{1,1,1} },
    Components::Rigidbody2D{ 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0x02 }, // 0x02 = UseGravity
    Components::LinearVelocity2D{ Vector2D{0, 0} },
    Components::CircleCollider2D{ 25.0f, Vector2D{0,0}, 0xFFFFFFFF, 0 }
);
```

### RendererSystem

Renders 2D sprites and debug shapes.

**Usage:**
```cpp
// In Scene::OnLoad()
m_rendererSystem = std::make_shared<ECS::RendererSystem>();
m_rendererSystem->Initialize();

AddSystem([this](Scenes::Scene& s, float dt) {
    m_rendererSystem->Update(s.GetWorld(), dt);
}, "Renderer System");
```

**Supported Components:**
- `Components::SpriteRenderer2D` - Textured sprites
- `Components::ShapeCircle2D` - Debug circles
- `Components::ShapeBox2D` - Debug boxes
- `Components::ShapeLine2D` - Debug lines

**Example:**
```cpp
// Render a sprite
Entity sprite = world.Create(
    Components::LocalTransform{ Vector3D{400, 300, 0}, Quaternion::Identity, Vector3D{256, 256, 1} },
    Components::SpriteRenderer2D{ textureId, Color{255,255,255,255}, Vector2D{1,1}, Vector2D{0,0} }
);
```

### LifetimeSystem

Automatically destroys entities after a specified lifetime.

**Usage:**
```cpp
// In Scene::OnLoad()
AddSystem([](Scenes::Scene& s, float dt) {
    ECS::LifetimeSystem::Update(s.GetWorld(), dt);
}, "Lifetime System");
```

**Required Components:**
- `Components::Lifetime` (time remaining)
- `Components::Active` (optional, only active entities processed)

**Example:**
```cpp
// Entity that expires after 3 seconds
Entity temp = world.Create(
    Components::LocalTransform{ Vector3D{400, 300, 0}, Quaternion::Identity, Vector3D{1,1,1} },
    Components::Lifetime{ 3.0f },
    Components::Active{ true },
    Components::ShapeCircle2D{ 20.0f, Vector2D{0,0}, Color{255, 255, 0, 255}, 0.0f, true }
);
```

---

## Hierarchy & Parenting

GrapeEngine's ECS supports parent-child relationships between entities.

### Creating Hierarchies

```cpp
// Create parent
Entity parent = world.Create(
    Components::LocalTransform{ Vector3D{400, 300, 0}, Quaternion::Identity, Vector3D{1,1,1} },
    Components::WorldTransform{}
);

// Create child
Entity child = world.Create(
    Components::LocalTransform{ Vector3D{50, 0, 0}, Quaternion::Identity, Vector3D{0.5f, 0.5f, 1} },
    Components::WorldTransform{}
);

// Attach child to parent
world.Attach(child, parent);
```

### Detaching

```cpp
world.Detach(child);
```

### Querying Hierarchy

```cpp
// Get parent of entity
Entity parent = world.ParentOf(child);

// Iterate over children
world.ForChildren(parent, [](Entity child) {
    // Process each child
});
```

### Transform Updates

The engine propagates transforms down the hierarchy each frame:

```cpp
// Transforms are normally updated automatically by the engine's Transform system.
// You typically do not need to call this manually.
```

**Important Notes:**
- Both parent and child must have `LocalTransform` and `WorldTransform` components
- Child transforms are relative to parent
- World transforms are computed automatically each frame

Tip: A dedicated `ECS::TransformSystem::Update(world, dt)` exists but is typically unnecessary to add manually, because the default scene update already refreshes transforms.

---

## Scene Management

### Creating Scenes

Create a custom scene by inheriting from `Scenes::Scene`:

```cpp
class MyScene : public Scenes::Scene {
public:
    void OnLoad() override {
        // Called once when scene is added to SceneManager
        // Initialize resources, create entities, add systems
    }
    
    void OnEnter() override {
        // Called when scene becomes active
    }
    
    void OnUpdate() override {
        // Called every frame while active
    }
    
    void OnFixedUpdate() override {
        // Called at a fixed timestep (e.g., physics)
    }

    void OnLateUpdate() override {
        // Called after OnUpdate each frame
    }
    
    void OnExit() override {
        // Called when scene stops being active
    }
    
    void OnUnload() override {
        // Called once when scene is removed from SceneManager
        // Clean up resources
    }
};
```

### Adding Systems to Scenes

```cpp
void MyScene::OnLoad() {
    // Add built-in systems
    AddSystem([](Scenes::Scene& s, float dt) {
        ECS::PhysicsSystem::Update(s.GetWorld(), dt);
    }, "Physics System");
    
    AddSystem([](Scenes::Scene& s, float dt) {
        ECS::LifetimeSystem::Update(s.GetWorld(), dt);
    }, "Lifetime System");
    
    // Add custom system
    AddSystem([](Scenes::Scene& s, float dt) {
        auto& world = s.GetWorld();
        world.Each<Components::LocalTransform, Components::LinearVelocity2D>(
            [dt](Entity e, Components::LocalTransform& tr, const Components::LinearVelocity2D& vel) {
                tr.Position.X += vel.Value.X * dt;
                tr.Position.Y += vel.Value.Y * dt;
            }
        );
    }, "Movement System");
    
    // Note: Transforms are updated automatically after all systems each frame.
}
```

### System Introspection & IDs

Every system added via `AddSystem()` receives a stable, never-reused 64-bit ID and an optional name.

Useful APIs on `Scenes::Scene`:
- `uint64_t AddSystem(System sys, const char* name = nullptr);`
- `size_t GetSystemCount() const;`
- `const Scene::SystemEntry* GetSystem(size_t index) const;`
- `const std::vector<Scene::SystemEntry>& GetSystems() const;`
- `template<typename F> void ForEachSystem(F&& fn) const; // iterate id, index, entry`
- `size_t FindSystemIndexByName(const char* name) const;`
- `uint64_t FindSystemIdByName(const char* name) const;`
- `size_t FindSystemIndexById(uint64_t id) const;`

Example: enumerate systems for diagnostics
```cpp
scene.ForEachSystem([](uint64_t id, size_t index, const Scenes::Scene::SystemEntry& sys){
    std::printf("[%zu] id=%llu name=%s enabled=%d\n", index,
        static_cast<unsigned long long>(id),
        sys.Name ? sys.Name : "<unnamed>",
        sys.Enabled);
});
```

### Scene Lifecycle

```cpp
// Create scene manager
Scenes::SceneManager sceneManager;

// Add scenes
size_t scene1Index = sceneManager.AddScene(new MyScene());
size_t scene2Index = sceneManager.AddScene(new OtherScene());

// Set active scene
sceneManager.SetActive(scene1Index);

// Update (call every frame)
sceneManager.Update();

// Optional: switch immediately (outside of update) if needed
sceneManager.SetActiveImmediate(scene2Index);

// Switch scenes
sceneManager.SetActive(scene2Index);

// Remove scene
sceneManager.RemoveScene(scene1Index);
```

Notes:
- `SetActive(index)` schedules the transition for the next `Update()` boundary. Use `SetActiveImmediate(index)` to perform the transition right away.
- The active scene’s systems are executed with the current frame’s `DeltaTime`.

### Saving & Loading Scenes

```cpp
// Save current scene to JSON
sceneManager.SaveScene(
    sceneIndex,
    "assets/saves/my_scene.json",
    "MyScene",
    "1.0"
);

// Load scene from JSON
sceneManager.LoadScene(
    sceneIndex,
    "assets/saves/my_scene.json"
);
```

**Note:** Loading a scene destroys all existing entities in the scene before loading.

---

## Layer System

Layers provide a way to organize and filter entities.

### Creating Layers

```cpp
// In a Scene
uint16_t groundLayer = GetLayers().CreateOrGetLayer("ground");
uint16_t playerLayer = GetLayers().CreateOrGetLayer("player");
uint16_t enemyLayer = GetLayers().CreateOrGetLayer("enemy");
```

### Creating Entities on Layers

```cpp
Entity player = CreateOnLayer(playerLayer,
    Components::LocalTransform{ Vector3D{400, 300, 0}, Quaternion::Identity, Vector3D{1,1,1} },
    Components::Name{"Player"}
);
```

### Changing Entity Layers

```cpp
SetLayer(entity, enemyLayer);
```

### Removing from Layer

```cpp
RemoveFromLayer(entity);
```

### Layer-Specific System Updates

```cpp
// Update lifetime only for specific layer
ECS::LifetimeSystem::UpdateForLayer(world, dt, enemyLayer);
```

---

## Advanced Topics

### Entity Pooling

The ECS automatically reuses entity slots when entities are destroyed:

```cpp
// Create entity
Entity e1 = world.Create();
uint32_t index1 = e1.Index; // e.g., 0

// Destroy entity
world.Destroy(e1);

// Create new entity - reuses slot but with incremented generation
Entity e2 = world.Create();
uint32_t index2 = e2.Index; // Same as index1 (e.g., 0)
uint32_t gen2 = e2.Generation; // Incremented (e.g., 1)

// Old handle is now invalid
bool alive = world.IsAlive(e1); // false
```

### Component Iteration Performance

Iteration is fastest when entities share the same archetype:

```cpp
// Fast - all entities with Transform + Velocity are in same archetype
world.Each<Components::LocalTransform, Components::LinearVelocity2D>(
    [](Entity e, Components::LocalTransform& tr, Components::LinearVelocity2D& vel) {
        // Process
    }
);
```

**Performance Tips:**
- Avoid adding/removing components during iteration
- Batch structural changes (add/remove components) when possible
- Use `Each()` for queries rather than manual iteration

### Archetype Changes

Adding or removing components causes entities to move between archetypes:

```cpp
// Entity starts in archetype [Transform]
Entity e = world.Create(Components::LocalTransform{});

// Add component -> moves to archetype [Transform, Active]
world.Add<Components::Active>(e, Components::Active{true});

// Remove component -> moves back to archetype [Transform]
world.Remove<Components::Active>(e);
```

**Performance Impact:**
- Archetype changes are relatively expensive (memory copy)
- Minimize component add/remove during gameplay
- Design components to be toggled via flags instead of added/removed

### Cloning Entities

```cpp
// Clone entity with all components
Entity clone = world.Clone(original);

// Clone with options
ECS::CloneOptions opts;
opts.KeepParent = false;  // Don't clone parent relationship
opts.KeepLayer = true;    // Keep layer assignment
opts.KeepName = false;    // Clear name

Entity clone = world.Clone(original, opts);
```

### Performance Optimization Guide

GrapeEngine's ECS has been heavily optimized for large-scale scenes. Here are the key optimizations and how to leverage them:

#### 1. Optimized Component Lookups (O(log n) → O(1))

**What Changed:** The archetype system replaced binary search with `std::unordered_map` for component index lookups.

**Before:**
```cpp
// O(log n) binary search through component array
ComponentIndex GetComponentIndex(TypeId t) const {
    auto it = std::lower_bound(m_componentInfos.begin(), m_componentInfos.end(), t);
    return static_cast<ComponentIndex>(it - m_componentInfos.begin());
}
```

**After:**
```cpp
// O(1) hash table lookup
std::unordered_map<TypeId, ComponentIndex> m_componentIndexCache;

ComponentIndex GetComponentIndex(TypeId t) const {
    auto it = m_componentIndexCache.find(t);
    return it->second;
}
```

**Performance Impact:**
- Eliminates binary search overhead in hot paths
- Faster for archetypes with many components

**When to Use:**
- Always preferred over Has() + Get() pattern
- Critical for systems processing many entities
- Essential in tight loops and hot paths

---

#### 2. TryGet Pattern - Combined Has + Get

**What Changed:** New `TryGet<T>()` method combines existence check and retrieval in one call.

**Before:**
```cpp
// ❌ SLOW: Two separate lookups (archetype lookup + component retrieval)
if (world.Has<Transform>(entity)) {
    Transform& t = world.Get<Transform>(entity); // Second lookup!
    t.Position.X += 1.0f;
}
```

**After:**
```cpp
// ✅ FAST: Single lookup returning pointer
if (Transform* t = world.TryGet<Transform>(entity)) {
    t->Position.X += 1.0f;
}
```

**API Signature:**
```cpp
template<typename T>
T* TryGet(Entity e);

template<typename T>
const T* TryGet(Entity e) const;
```

**Performance Impact:**
- Faster than Has() + Get() pattern
- Eliminates redundant archetype/location lookups
- Better branch prediction
- Recommended for all single-component conditional access

**Example Usage:**
```cpp
// Mutable access
if (Transform* t = world.TryGet<Transform>(entity)) {
    t->Position.X += velocity * dt;
}

// Const access
if (const Health* h = world.TryGet<Health>(entity)) {
    std::cout << "Health: " << h->Current << std::endl;
}

// Optional components pattern
void UpdateEntity(Entity e) {
    auto* transform = world.TryGet<Transform>(e);
    auto* velocity = world.TryGet<Velocity>(e);
    
    if (transform) {
        // Always process transform
        if (velocity) {
            // Also has velocity - moving entity
            transform->Position += velocity->Value * dt;
        } else {
            // Static entity
        }
    }
}
```

---

#### 3. Bulk Component Access with TryGetComponents

**What Changed:** New `TryGetComponents<Ts...>()` retrieves multiple components in one optimized call using fold expressions.

**Before:**
```cpp
// ❌ SLOW: Multiple separate lookups
if (world.Has<Transform>(e) && world.Has<Velocity>(e) && world.Has<Health>(e)) {
    auto& t = world.Get<Transform>(e);  // Lookup 1
    auto& v = world.Get<Velocity>(e);   // Lookup 2
    auto& h = world.Get<Health>(e);     // Lookup 3
    // Use components
}
```

**After:**
```cpp
// ✅ FAST: Single batch retrieval with fold expressions
auto [t, v, h] = world.TryGetComponents<Transform, Velocity, Health>(e);
if (t && v && h) {
    // Use *t, *v, *h
}
```

**API Signature:**
```cpp
template<typename... Ts>
std::tuple<Ts*...> TryGetComponents(Entity e);

template<typename... Ts>
std::tuple<const Ts*...> TryGetComponents(Entity e) const;
```

**Performance Impact:**
- Faster than multiple Has() + Get() calls for 3 components
- Single validation pass using fold expressions
- Better instruction cache utilization
- Scales with component count: more components = bigger speedup

**Example Usage:**
```cpp
// Basic usage with 3 components
auto [transform, velocity, health] = world.TryGetComponents<Transform, Velocity, Health>(e);
if (transform && velocity && health) {
    velocity->Value += acceleration * dt;
    transform->Position += velocity->Value * dt;
    health->Current -= damage;
}

// Const access
auto [ct, cv] = world.TryGetComponents<const Transform, const Velocity>(e);
if (ct && cv) {
    float speed = glm::length(cv->Value);
    std::cout << "Entity at " << ct->Position << " moving at " << speed << std::endl;
}

// Mixed const/mutable
auto [t, cv, h] = world.TryGetComponents<Transform, const Velocity, Health>(e);
if (t && cv && h) {
    t->Position += cv->Value * dt;  // Read velocity, write transform
    h->Current = std::max(0.0f, h->Current - 1.0f);
}

// With structured bindings for clarity
auto components = world.TryGetComponents<Transform, Rigidbody, Collider>(e);
auto& [transform, rigidbody, collider] = components;
if (transform && rigidbody && collider) {
    // All components present
}
```

**Important Notes:**
- All pointers are `nullptr` if **any** component is missing
- Always check all pointers before dereferencing
- Best for entities where you need multiple components together
- Use `TryGet()` if you only need one component

---

#### 4. Query Archetype Caching

**What Changed:** The `Each()` method caches matching archetypes per query signature.

**How It Works:**
```cpp
// First call: Scans all archetypes, caches matches
world.Each<Transform, Velocity>([](Entity e, Transform& t, Velocity& v) {
    t.Position += v.Value * dt;
});

// Subsequent calls: Reuses cached archetype list (near-zero overhead)
world.Each<Transform, Velocity>([](Entity e, Transform& t, Velocity& v) {
    t.Position += v.Value * dt;  // Instant access to matching archetypes
});
```

**Performance Impact:**
- Near-zero overhead for repeated queries
- Cache invalidated only when archetypes are created/destroyed
- Automatically managed - no user action needed

**When It Matters:**
- Systems that run every frame with the same query
- Multiple `Each()` calls with identical component signatures
- Long-running games where archetype structure stabilizes

---

#### 5. Component Index Precomputation in Each()

**What Changed:** Component indices are precomputed once per archetype, not per entity.

**How It Works:**
```cpp
// Internally optimized
const auto& matched = _getMatchingArchetypes(req);  // Cached archetypes
for (auto* arch : matched) {
    // Precompute component indices ONCE per archetype
    std::array<uint32_t, sizeof...(Ts)> compIdxs{};
    for (size_t k = 0; k < compIdxs.size(); ++k) {
        compIdxs[k] = arch->GetComponentIndex(typeIds[k]);  // O(1) hash lookup
    }
    
    // Iterate entities - no repeated lookups!
    for (size_t i = 0; i < arch->Size(); ++i) {
        // Direct component access using precomputed indices
        fn(entities[i], arch->GetComponent<Ts>(i, compIdxs[k])...);
    }
}
```

**Performance Impact:**
- Linear scaling with entity count (O(n) not O(n log n))
- No repeated hash lookups per entity
- Cache-friendly sequential iteration
- Faster for large entity queries

**When It Matters:**
- Systems processing hundreds/thousands of entities
- Complex queries with many components
- Performance-critical game loops

---

#### When To Optimize?

**High Impact Scenarios:**
- **Large scenes** (1000+ entities)
- **Complex archetypes** (entities with 10+ components)
- **Hot path systems** running every frame
- **AI systems** querying many entities for decision making
- **Physics systems** processing hundreds of dynamic bodies
- **Particle systems** updating thousands of particles

**Low Impact Scenarios:**
- Small scenes (<100 entities)
- Rare component access (once per second)
- Simple archetypes (<5 components)
- Initialization code (not performance critical)

**Recommended Optimization Strategy:**
1. **Profile first** - Identify actual bottlenecks
2. **Update hot paths** - Replace Has()+Get() with TryGet()
3. **Batch access** - Use TryGetComponents() for multi-component queries
4. **Keep Each() loops** - Already fully optimized
5. **Measure improvement** - Verify performance gains

---

#### Future Optimization Opportunities

**1. Component Bitsets (Medium Priority)**

Store a bitset per entity for ultra-fast `Has()` checks:

```cpp
// Potential implementation
std::bitset<MAX_COMPONENTS> componentMask;

bool Has(TypeId t) const {
    return componentMask.test(t);
}
```

**Tradeoffs:**
- **Pro:** O(1) existence check without hash lookup
- **Con:** Limits total component types to bitset size (e.g., 256)
- **Con:** Memory overhead per entity

**When Considered:** If profiling shows `Has()` checks (not `Get()`) as bottleneck.

---

**2. Multi-threaded Each() (Low Priority)**

Parallelize entity iteration across archetypes:

```cpp
world.EachParallel<Transform, Velocity>([](Entity e, Transform& t, Velocity& v) {
    t.Position += v.Value * dt;  // Thread-safe per-entity operation
});
```

**Tradeoffs:**
- **Pro:** Potential speedup on multi-core CPUs
- **Con:** Requires careful synchronization for component access
- **Con:** Overhead for thread spawning on small queries
- **Con:** Complex debugging

**When Considered:** If targeting high-end platforms and processing 10,000+ entities per frame.

---

## Best Practices

### 1. Keep Components as POD Types
```cpp
// ✅ Good
struct MyComponent {
    float Value;
    int Count;
};

// ❌ Bad - uses heap allocation
struct MyComponent {
    std::string Name; // Not trivially copyable!
};
```

### 2. Use Fixed-Size Buffers for Strings
```cpp
// ✅ Good
struct Name {
    char Value[64] = {0};
};

// ❌ Bad
struct Name {
    std::string Value;
};
```

### 3. Use TryGet() Instead of Has() + Get() (5x Faster)
```cpp
// ✅ Good - Single lookup, ~5x faster
if (Transform* t = world.TryGet<Transform>(entity)) {
    t->Position.X += 1.0f;
}

// ❌ Bad - Double lookup overhead
if (world.Has<Transform>(entity)) {
    Transform& t = world.Get<Transform>(entity); // Second lookup!
    t.Position.X += 1.0f;
}
```

**Why it matters:** 
- Eliminates redundant archetype/location lookups
- Better branch prediction
- Faster
- Returns `nullptr` if component doesn't exist

**Common Use Cases:**
```cpp
// Safe component access
if (auto* health = world.TryGet<Health>(entity)) {
    health->Current -= damage;
    if (health->Current <= 0.0f) {
        world.Destroy(entity);
    }
}

// Optional components
if (auto* t = world.TryGet<Transform>(entity)) {
    if (auto* v = world.TryGet<Velocity>(entity)) {
        t->Position += v->Value * dt;  // Has velocity
    } else {
        // Static object, no velocity
    }
}

// Const access for read-only operations
if (const auto* t = world.TryGet<Transform>(entity)) {
    std::cout << "Position: " << t->Position << std::endl;
}
```

### 4. Use TryGetComponents() for Multiple Components
```cpp
// ✅ Good - Single batch retrieval, ~8-9x faster
auto [transform, velocity, health] = world.TryGetComponents<Transform, Velocity, Health>(entity);
if (transform && velocity && health) {
    transform->Position += velocity->Value * dt;
    health->Current -= damage;
}

// ❌ Bad - Multiple separate lookups (6 operations!)
if (world.Has<Transform>(entity) && world.Has<Velocity>(entity) && world.Has<Health>(entity)) {
    Transform& t = world.Get<Transform>(entity);
    Velocity& v = world.Get<Velocity>(entity);
    Health& h = world.Get<Health>(entity);
    t.Position += v.Value * dt;
    h.Current -= damage;
}
```

**Why it matters:**
- Single validation pass using fold expressions
- Better instruction cache utilization
- Faster for 3+ components
- All pointers are `nullptr` if any component is missing

**Common Use Cases:**
```cpp
// Multi-component logic
auto [t, v, a] = world.TryGetComponents<Transform, Velocity, Acceleration>(entity);
if (t && v && a) {
    v->Value += a->Value * dt;  // Apply acceleration
    t->Position += v->Value * dt;  // Apply velocity
}

// Mixed const/mutable access
auto [t, cv, h] = world.TryGetComponents<Transform, const Velocity, Health>(entity);
if (t && cv && h) {
    t->Position += cv->Value * dt;  // Read velocity, write transform
    h->Current = std::max(0.0f, h->Current - damage);
}

// Partial validation for optional components
auto [t, v] = world.TryGetComponents<Transform, Velocity>(entity);
if (t) {
    // Transform exists (mandatory)
    if (v) {
        // Also has velocity (optional)
        t->Position += v->Value * dt;
    }
}
```

### 5. Prefer Each() for Bulk Processing
```cpp
// ✅ Best - Fully optimized with archetype caching and index precomputation
world.Each<Transform, Velocity>([dt](Entity e, Transform& t, Velocity& v) {
    t.Position += v.Value * dt;
});

// ❌ Bad - Manual iteration loses all optimizations
for (Entity e : allEntities) {
    if (world.Has<Transform>(e) && world.Has<Velocity>(e)) {
        // Loses cache locality, no archetype optimization, no index caching
    }
}
```

**Why it matters:**
- Archetype query caching (near-zero overhead for repeated queries)
- Component index precomputation (computed once per archetype, not per entity)
- Cache-friendly sequential iteration
- Faster than manual iteration

**Advanced Each() Patterns:**
```cpp
// Const iteration for read-only operations
world.Each<const Transform, const Velocity>([](Entity e, const Transform& t, const Velocity& v) {
    // Read-only access, compiler can optimize better
    float speed = glm::length(v.Value);
});

// Mixed const/mutable
world.Each<Transform, const Velocity>([dt](Entity e, Transform& t, const Velocity& v) {
    t.Position += v.Value * dt;  // Write transform, read velocity
});

// Capture external state
float totalSpeed = 0.0f;
world.Each<const Velocity>([&totalSpeed](Entity e, const Velocity& v) {
    totalSpeed += glm::length(v.Value);
});

// Early exit for specific conditions
world.Each<Health>([](Entity e, Health& h) {
    if (h.Current <= 0.0f) return;  // Skip dead entities
    h.Current += h.Regeneration * dt;
});
```

### 6. Usage Chart

| Pattern | Use Case |
|---------|----------|
| `Each<T,U,V>()` | Process many entities with same components |
| `TryGetComponents<T,U,V>()` | Check few specific entities with multiple components |
| `TryGet<T>()` | Check specific entity with single component |
| `Has<T>() + Get<T>()` | ❌ **Avoid** - Use TryGet() instead |
| Manual iteration | ❌ **Avoid** - Use Each() instead |

### 7. Add Systems in OnLoad()
```cpp
void MyScene::OnLoad() {
    AddSystem([](Scenes::Scene& s, float dt) {
        // System logic
    }, "MySystem");
}
```

### 8. Minimize Structural Changes (Add/Remove Components)
```cpp
// ✅ Good - use flags to toggle behavior
struct Enemy {
    bool IsAlive = true;
    bool IsAggressive = false;
};

world.Each<Enemy>([](Entity e, Enemy& enemy) {
    if (!enemy.IsAlive) return; // Skip dead enemies
    if (enemy.IsAggressive) {
        // Aggressive behavior
    }
});

// ❌ Bad - frequent add/remove triggers archetype changes (expensive!)
if (dead) {
    world.Remove<Enemy>(e); // Expensive archetype change!
}
if (aggressive) {
    world.Add<Aggressive>(e, Aggressive{});  // Another expensive change!
}
```

**Why it matters:**
- Adding/removing components triggers archetype changes (memory copy)
- Archetype changes are relatively expensive
- Minimize during gameplay, prefer initialization-time composition

**When structural changes are acceptable:**
- Entity creation/destruction
- Level loading
- Infrequent state transitions (e.g., picking up/dropping items)

### 9. Use Layers for Organization
```cpp
uint16_t uiLayer = GetLayers().CreateOrGetLayer("ui");
uint16_t gameLayer = GetLayers().CreateOrGetLayer("game");

// Easy to query by layer
world.Each<Components::Layer, Components::SpriteRenderer2D>(
    [uiLayer](Entity e, const Components::Layer& layer, Components::SpriteRenderer2D& sprite) {
        if (layer.Id == uiLayer) {
            // Process UI sprites
        }
    }
);
```

### 10. Prefer Const References When Reading Only
```cpp
// ✅ Good - const for read-only, enables compiler optimizations
world.Each<const Transform, const Velocity>([](Entity e, const Transform& t, const Velocity& v) {
    float speed = glm::length(v.Value);
    // Read-only access
});

// ❌ Less optimal - mutable when not needed
world.Each<Transform, Velocity>([](Entity e, Transform& t, Velocity& v) {
    float speed = glm::length(v.Value);  // Not modifying but not marked const
});
```

### 11. Use Entity Packing for Serialization
```cpp
// Store entity references for save files
std::vector<uint64_t> entityIds;

// Pack for storage (combines index + generation)
entityIds.push_back(EntityUtils::Pack(entity));

// Unpack when loading
Entity entity = EntityUtils::Unpack(entityIds[0]);

// Always validate after unpacking (generation check)
if (world.IsAlive(entity)) {
    // Safe to use
} else {
    // Entity was destroyed or handle is stale
}
```

### 12. Batch Entity Creation for Better Performance
```cpp
// ✅ Good - batch creation minimizes archetype churn
std::vector<Entity> enemies;
enemies.reserve(100);  // Pre-allocate

for (int i = 0; i < 100; ++i) {
    Entity e = world.Create(
        Components::LocalTransform{},
        Components::Health{100.0f},
        Components::Enemy{}
    );
    enemies.push_back(e);
}

// ❌ Less optimal - creating entities with incomplete components
for (int i = 0; i < 100; ++i) {
    Entity e = world.Create();
    world.Add<Transform>(e, Transform{});  // Archetype change
    world.Add<Health>(e, Health{});        // Another archetype change
    world.Add<Enemy>(e, Enemy{});          // Another archetype change
}
```

### 13. Use Component Flags Instead of Optional Components
```cpp
// ✅ Good - flags within component
struct Rigidbody2D {
    float Mass;
    uint32_t Flags;  // bit 0: Kinematic, bit 1: UseGravity, bit 2: FixedRotation
};

world.Each<Rigidbody2D>([](Entity e, Rigidbody2D& rb) {
    bool useGravity = rb.Flags & 0x02;
    if (useGravity) {
        // Apply gravity
    }
});

// ❌ Less optimal - separate components require archetype changes
struct Rigidbody2D { float Mass; };
struct UseGravity {};  // Separate component

// Adding/removing UseGravity triggers archetype changes
world.Add<UseGravity>(e, UseGravity{});
```

### 14. Profile Before Optimizing
```cpp
// Measure actual performance before optimizing
auto start = std::chrono::high_resolution_clock::now();

world.Each<Transform, Velocity>([](Entity e, Transform& t, Velocity& v) {
    // System logic
});

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
std::cout << "System took: " << duration.count() << "μs" << std::endl;
```

**Performance Targets:**
- **Each() system:** <1ms for 1000 entities with 3 components
- **TryGet() access:** <10ns per call
- **TryGetComponents():** <20ns for 3 components
- **Frame budget:** Keep all systems under 16ms (60 FPS) or 8ms (120 FPS)

---

## Performance Quick Reference

Quick reference for high-performance component access patterns. Use this as a cheat sheet when writing systems.

### Component Access Decision Tree

```
Need to access components?
│
├─ Processing many entities with SAME components?
│  └─ ✅ Use Each<T,U,V>() - FASTEST
│     world.Each<Transform, Velocity>([](Entity e, Transform& t, Velocity& v) { ... });
│
├─ Checking few specific entities with MULTIPLE components?
│  └─ ✅ Use TryGetComponents<T,U,V>() - 8-9x faster than Has+Get
│     auto [t,v,h] = world.TryGetComponents<Transform, Velocity, Health>(e);
│     if (t && v && h) { ... }
│
└─ Checking specific entity with SINGLE component?
   └─ ✅ Use TryGet<T>() - 5x faster than Has+Get
      if (auto* t = world.TryGet<Transform>(e)) { ... }
```

### Pattern Comparison

| Pattern | When to Use |
|---------|-------------|
| **Each<T,U,V>()** | Many entities, same components |
| **TryGetComponents<T,U,V>()** | Few entities, multiple components |
| **TryGet<T>()** | Few entities, single component |
| ~~Has() + Get()~~ | **Never use** - replaced by TryGet() |
| ~~Manual iteration~~ | **Never use** - replaced by Each() |

### Code Examples

#### Single Component Access
```cpp
// ✅ FAST
if (Transform* t = world.TryGet<Transform>(entity)) {
    t->Position.X += 1.0f;
}

// ❌ SLOW - Don't use!
if (world.Has<Transform>(entity)) {
    Transform& t = world.Get<Transform>(entity);
    t.Position.X += 1.0f;
}
```

#### Multiple Component Access
```cpp
// ✅ FAST (3 components)
auto [t, v, h] = world.TryGetComponents<Transform, Velocity, Health>(e);
if (t && v && h) {
    t->Position += v->Value * dt;
    h->Current -= damage;
}

// ❌ SLOW - Don't use!
if (world.Has<Transform>(e) && world.Has<Velocity>(e) && world.Has<Health>(e)) {
    auto& t = world.Get<Transform>(e);
    auto& v = world.Get<Velocity>(e);
    auto& h = world.Get<Health>(e);
}
```

#### Bulk Entity Processing
```cpp
// ✅ FASTEST (1000 entities)
world.Each<Transform, Velocity>([dt](Entity e, Transform& t, Velocity& v) {
    t.Position += v.Value * dt;
});

// ❌ SLOW (1000 entities) - Don't use!
for (Entity e : entities) {
    if (world.Has<Transform>(e) && world.Has<Velocity>(e)) {
        // Manual iteration loses optimizations
    }
}
```

### Optional Components Pattern
```cpp
// Process mandatory component, check for optional
if (auto* transform = world.TryGet<Transform>(entity)) {
    // Transform exists (mandatory)
    transform->Position.X += baseSpeed * dt;
    
    if (auto* velocity = world.TryGet<Velocity>(entity)) {
        // Also has velocity (optional boost)
        transform->Position += velocity->Value * dt;
    }
}
```

### Multi-Component with Partial Validation
```cpp
// Get multiple, validate which exist
auto [t, v, h] = world.TryGetComponents<Transform, Velocity, Health>(entity);

if (t) {
    // Transform exists
    if (v && h) {
        // Has all three
        t->Position += v->Value * dt;
        h->Current -= damage;
    } else if (v) {
        // Has transform + velocity only
        t->Position += v->Value * dt;
    } else {
        // Static entity (transform only)
    }
}
```

### System Templates

#### Physics System Template
```cpp
void PhysicsSystem(Scenes::Scene& scene, float dt) {
    auto& world = scene.GetWorld();
    
    // Use Each() for bulk processing - already optimized!
    world.Each<Transform, Velocity, Rigidbody>([dt](
        Entity e,
        Transform& transform,
        Velocity& velocity,
        Rigidbody& rigidbody
    ) {
        // Apply gravity
        velocity.Value.Y += GRAVITY * dt;
        
        // Apply velocity
        transform.Position += velocity.Value * dt;
        
        // Apply damping
        velocity.Value *= (1.0f - rigidbody.Damping * dt);
    });
}
```

#### Collision Check Template (Specific Entities)
```cpp
void CheckCollision(World& world, Entity a, Entity b) {
    // Use TryGetComponents for specific entity checks
    auto [t1, c1] = world.TryGetComponents<Transform, Collider>(a);
    auto [t2, c2] = world.TryGetComponents<Transform, Collider>(b);
    
    if (t1 && c1 && t2 && c2) {
        // Both have required components
        if (Intersects(*t1, *c1, *t2, *c2)) {
            // Handle collision
        }
    }
}
```

#### Damage Application Template
```cpp
void ApplyDamage(World& world, Entity target, float damage) {
    // Use TryGet for single component access
    if (auto* health = world.TryGet<Health>(target)) {
        health->Current -= damage;
        
        if (health->Current <= 0.0f) {
            world.Destroy(target);
        }
    }
}
```
---

## Common Patterns

### Pattern 1: Particle System
```cpp
void SpawnParticle(Scenes::Scene& scene, Vector3D position) {
    Entity particle = scene.CreateEntity();
    scene.GetWorld().Add<Components::LocalTransform>(particle, 
        Components::LocalTransform{ position, Quaternion::Identity, Vector3D{1,1,1} });
    scene.GetWorld().Add<Components::LinearVelocity2D>(particle,
        Components::LinearVelocity2D{ Vector2D{
            MathUtils::Randomize(-100.f, 100.f),
            MathUtils::Randomize(-100.f, 100.f)
        }});
    scene.GetWorld().Add<Components::Lifetime>(particle, Components::Lifetime{2.0f});
    scene.GetWorld().Add<Components::ShapeCircle2D>(particle,
        Components::ShapeCircle2D{ 5.0f, Vector2D{0,0}, Color{255,255,0,255}, 0.0f, true });
}
```

### Pattern 2: Projectile
```cpp
Entity CreateProjectile(Scenes::Scene& scene, Vector3D position, Vector2D velocity) {
    Entity projectile = scene.CreateOnLayer(projectileLayer,
        Components::LocalTransform{ position, Quaternion::Identity, Vector3D{1,1,1} },
        Components::Rigidbody2D{ 0.1f, 10.0f, 0.0f, 0.0f, 0.0f, 0 },
        Components::LinearVelocity2D{ velocity },
        Components::CircleCollider2D{ 5.0f, Vector2D{0,0}, 0xFFFFFFFF, 0 },
        Components::Lifetime{ 5.0f },
        Components::ShapeCircle2D{ 5.0f, Vector2D{0,0}, Color{255,0,0,255}, 0.0f, true }
    );
    return projectile;
}
```

### Pattern 3: Spawner System
```cpp
struct Spawner {
    float Interval = 1.0f;
    float Timer = 0.0f;
    int MaxSpawns = 10;
    int SpawnCount = 0;
};

// In update system
world.Each<Spawner, Components::LocalTransform>(
    [&](Entity e, Spawner& spawner, const Components::LocalTransform& tr) {
        spawner.Timer += dt;
        if (spawner.Timer >= spawner.Interval && spawner.SpawnCount < spawner.MaxSpawns) {
            SpawnEntity(scene, tr.Position);
            spawner.SpawnCount++;
            spawner.Timer = 0.0f;
        }
    }
);
```

### Pattern 4: Health System
```cpp
struct Health {
    float Current = 100.0f;
    float Max = 100.0f;
};

// Damage system
void ApplyDamage(ECS::World& world, Entity target, float damage) {
    if (world.Has<Health>(target)) {
        auto& health = world.Get<Health>(target);
        health.Current -= damage;
        
        if (health.Current <= 0.0f) {
            world.Destroy(target);
        }
    }
}
```

### Pattern 5: State Machine Component
```cpp
enum class EnemyState {
    Idle,
    Patrol,
    Chase,
    Attack
};

struct EnemyAI {
    EnemyState State = EnemyState::Idle;
    float StateTimer = 0.0f;
    Entity Target = NULL_ENTITY;
};

// AI System
world.Each<EnemyAI, Components::LocalTransform>(
    [&](Entity e, EnemyAI& ai, Components::LocalTransform& tr) {
        ai.StateTimer += dt;
        
        switch (ai.State) {
            case EnemyState::Idle:
                if (ai.StateTimer > 2.0f) {
                    ai.State = EnemyState::Patrol;
                    ai.StateTimer = 0.0f;
                }
                break;
            case EnemyState::Patrol:
                // Patrol logic
                break;
            // ... other states
        }
    }
);
```

---

## Troubleshooting

### Entity Handle is Invalid After Destroy
**Problem:** Trying to use an entity after it's been destroyed.

**Solution:** Always check `world.IsAlive(entity)` before using an entity handle.

```cpp
if (world.IsAlive(entity)) {
    // Safe to use
}
```

### Components Not Updating
**Problem:** Modifying components but changes don't appear.

**Solution:** Ensure you're getting a non-const reference:

```cpp
// ❌ Wrong - const reference
world.Each<const Components::LocalTransform>([](Entity e, const Components::LocalTransform& tr) {
    tr.Position.X += 1.0f; // Won't compile
});

// ✅ Correct - mutable reference
world.Each<Components::LocalTransform>([](Entity e, Components::LocalTransform& tr) {
    tr.Position.X += 1.0f; // Works
});
```

### System Not Running
**Problem:** System is added but doesn't execute.

**Solution:** Ensure the scene is active and `SceneManager::Update()` is called:

```cpp
// In game loop
sceneManager.Update();
```

### Entity Not Rendering
**Problem:** Entity has rendering components but doesn't appear.

**Checklist:**
1. Is `RendererSystem` initialized and added to scene?
2. Does entity have `LocalTransform` component?
3. Is entity on a visible layer?
4. Is texture ID valid (for sprites)?
5. Is entity within camera bounds?

### Physics Not Working
**Problem:** Physics components don't affect entity.

**Checklist:**
1. Is `PhysicsSystem` added to scene?
2. Does entity have all required components?
   - `LocalTransform`
   - `Rigidbody2D`
   - `LinearVelocity2D`
   - Collider component
3. Is mass > 0 for dynamic bodies?
4. Are flags set correctly (e.g., `UseGravity`)?

### Memory Leak or Crash on Scene Change
**Problem:** Application crashes when switching scenes.

**Solution:** Ensure proper cleanup in `OnUnload()` and `OnExit()`:

```cpp
void MyScene::OnUnload() {
    // Release shared resources
    m_rendererSystem.reset();
    
    // Clear entity references
    m_entityReferences.clear();
}
```

### Transform Hierarchy Not Working
**Problem:** Child entities don't follow parent.

**Checklist:**
1. Both parent and child have `WorldTransform` component
2. Both have `LocalTransform` component
3. Transforms are being updated by the engine's `Transform` system (ensure `TransformSystem` is registered)
4. Child is properly attached: `world.Attach(child, parent)`

---

## Conclusion

This guide covers the essential concepts and usage patterns for our ECS. For more examples, refer to:
- `src/test/ECSTest.cpp` - Comprehensive test demonstrating all systems
- `include/engine/scene/Scene.h` - Scene class implementation
- `include/engine/scene/SceneManager.h` - Scene management
