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
11. [Best Practices](#best-practices)
12. [Common Patterns](#common-patterns)
13. [Troubleshooting](#troubleshooting)

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
Computed world-space transform matrix. Updated by `Hierarchy::UpdateTransforms()`.

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
// Usually called automatically by Scene::_update()
ECS::Hierarchy::UpdateTransforms(world);
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

### 3. Add Systems in OnLoad()
```cpp
void MyScene::OnLoad() {
    AddSystem([](Scenes::Scene& s, float dt) {
        // System logic
    }, "MySystem");
}
```

### 4. Minimize Structural Changes
```cpp
// ✅ Good - use flags
struct Enemy {
    bool IsAlive = true;
};

world.Each<Enemy>([](Entity e, Enemy& enemy) {
    if (!enemy.IsAlive) return; // Skip dead enemies
});

// ❌ Bad - frequent add/remove
if (dead) {
    world.Remove<Enemy>(e); // Expensive!
}
```

### 5. Use Layers for Organization
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

### 6. Prefer Const References in Queries
```cpp
// When reading only
world.Each<const Components::LocalTransform>(
    [](Entity e, const Components::LocalTransform& tr) {
        // Read-only access
    }
);
```

### 7. Use Entity Packing for Storage
```cpp
// Store entity references
std::vector<uint64_t> entityIds;

// Pack for storage
entityIds.push_back(EntityUtils::Pack(entity));

// Unpack when needed
Entity entity = EntityUtils::Unpack(entityIds[0]);

// Always validate after unpacking
if (world.IsAlive(entity)) {
    // Use entity
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
3. `Hierarchy::UpdateTransforms()` is being called
4. Child is properly attached: `world.Attach(child, parent)`

---

## Conclusion

This guide covers the essential concepts and usage patterns for our ECS. For more examples, refer to:
- `src/test/ECSTest.cpp` - Comprehensive test demonstrating all systems
- `include/engine/scene/Scene.h` - Scene class implementation
- `include/engine/scene/SceneManager.h` - Scene management
