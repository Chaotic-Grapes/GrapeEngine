/* Start Header *****************************************************************/
/*!
\file   Scene.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Managed wrapper for Scene. Provides access to the scene's world and entities from C#.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Represents a game scene containing entities, components, and layers.
/// </summary>
public class Scene
{
    private readonly unsafe void* _scenePtr;
    private World? _cachedWorld;

    /// <summary>
    /// Internal constructor. Scenes are obtained from SceneManager.
    /// </summary>
    internal unsafe Scene(void* scenePtr)
    {
        _scenePtr = scenePtr;
    }

    /// <summary>
    /// Gets the scene name.
    /// </summary>
    public string Name
    {
        get
        {
            unsafe
            {
                const int bufferSize = 256;
                Span<byte> buffer = stackalloc byte[bufferSize];
                fixed (byte* ptr = buffer)
                {
                    SceneAPI.GetName(_scenePtr, ptr, bufferSize);
                    int length = 0;
                    while (length < bufferSize && buffer[length] != 0) length++;
                    return System.Text.Encoding.UTF8.GetString(buffer[..length]);
                }
            }
        }
        set
        {
            unsafe
            {
                SceneAPI.SetName(_scenePtr, value);
            }
        }
    }

    /// <summary>
    /// Gets the scene file path.
    /// </summary>
    public string Path
    {
        get
        {
            unsafe
            {
                const int bufferSize = 512;
                Span<byte> buffer = stackalloc byte[bufferSize];
                fixed (byte* ptr = buffer)
                {
                    SceneAPI.GetPath(_scenePtr, ptr, bufferSize);
                    int length = 0;
                    while (length < bufferSize && buffer[length] != 0) length++;
                    return System.Text.Encoding.UTF8.GetString(buffer[..length]);
                }
            }
        }
        set
        {
            unsafe
            {
                SceneAPI.SetPath(_scenePtr, value);
            }
        }
    }

    /// <summary>
    /// Gets the ECS World for this scene. Entities in this scene are created in this world.
    /// </summary>
    public World GetWorld()
    {
        if (_cachedWorld != null)
            return _cachedWorld;

        unsafe
        {
            void* worldPtr = SceneAPI.GetWorld(_scenePtr);
            _cachedWorld = new World(worldPtr);
            return _cachedWorld;
        }
    }

    /// <summary>
    /// Creates an empty entity in the scene.
    /// </summary>
    /// <returns>The created entity.</returns>
    public Entity CreateEntity()
    {
        unsafe
        {
            ulong entityId = SceneAPI.CreateEntity(_scenePtr);
            return Entity.FromId(GetWorld(), entityId);
        }
    }

    /// <summary>
    /// Creates an entity with a parent in the scene.
    /// </summary>
    /// <param name="parent">The parent entity.</param>
    /// <returns>The created entity.</returns>
    public Entity CreateEntity(Entity parent)
    {
        if (parent == null)
            throw new ArgumentNullException(nameof(parent));

        unsafe
        {
            ulong entityId = SceneAPI.CreateEntityWithParent(_scenePtr, parent.Id);
            return Entity.FromId(GetWorld(), entityId);
        }
    }

    /// <summary>
    /// Destroys an entity in the scene.
    /// </summary>
    /// <param name="entity">The entity to destroy.</param>
    public void DestroyEntity(Entity entity)
    {
        if (entity == null)
            throw new ArgumentNullException(nameof(entity));

        unsafe
        {
            SceneAPI.DestroyEntity(_scenePtr, entity.Id);
        }
    }

    /// <summary>
    /// Creates an entity on a specific layer.
    /// </summary>
    /// <param name="layerId">The layer ID to assign the entity to.</param>
    /// <returns>The created entity.</returns>
    public Entity CreateOnLayer(ushort layerId)
    {
        unsafe
        {
            ulong entityId = SceneAPI.CreateOnLayer(_scenePtr, layerId);
            return Entity.FromId(GetWorld(), entityId);
        }
    }

    /// <summary>
    /// Assigns an entity to a layer.
    /// </summary>
    /// <param name="entity">The entity to move.</param>
    /// <param name="layerId">The target layer ID.</param>
    public void SetLayer(Entity entity, ushort layerId)
    {
        if (entity == null)
            throw new ArgumentNullException(nameof(entity));

        unsafe
        {
            SceneAPI.SetLayer(_scenePtr, entity.Id, layerId);
        }
    }

    /// <summary>
    /// Removes an entity from its layer.
    /// </summary>
    /// <param name="entity">The entity to remove from its layer.</param>
    public void RemoveFromLayer(Entity entity)
    {
        if (entity == null)
            throw new ArgumentNullException(nameof(entity));

        unsafe
        {
            SceneAPI.RemoveFromLayer(_scenePtr, entity.Id);
        }
    }

    /// <summary>
    /// Internal method to access the native scene pointer.
    /// </summary>
    internal unsafe void* NativePtr => _scenePtr;
}
