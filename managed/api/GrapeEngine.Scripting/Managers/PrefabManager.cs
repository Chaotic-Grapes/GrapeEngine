/* Start Header *****************************************************************/
/*!
\file   PrefabManager.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Public C# wrapper for PrefabManager interop.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Reflection;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Managers;

/// <summary>
/// Public C# wrapper for PrefabManager functionality.
/// Provides high-level API for prefab management in scripting.
/// </summary>
public class PrefabManager
{
    private readonly unsafe void* _nativePtr;

    /// <summary>
    /// Gets the singleton PrefabManager instance.
    /// </summary>
    public static PrefabManager GetInstance()
    {
        unsafe
        {
            var nativePtr = Unsafe.PrefabAPI.GetPrefabManagerInstance();
            if (nativePtr == null)
                throw new InvalidOperationException("PrefabManager instance is not available.");
            return new PrefabManager(nativePtr);
        }
    }

    /// <summary>
    /// Gets the PrefabManager associated with a specific scene.
    /// </summary>
    public static PrefabManager GetFromScene(Scene scene)
    {
        if (scene == null)
            throw new ArgumentNullException(nameof(scene));
        
        // Get the PrefabManager via the scene's internal pointer
        // Using reflection approach since we can't directly access _scenePtr
        var type = scene.GetType();
        var field = type.GetField("_scenePtr", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
        if (field == null)
            throw new InvalidOperationException("Could not access scene's internal pointer.");
    
        var scenePtrValue = field.GetValue(scene);
        if (scenePtrValue == null)
            throw new InvalidOperationException("Scene's internal pointer is null.");
        
        // Convert the IntPtr value to void*
        unsafe
        {
            var scenePtr = (void*)(IntPtr)scenePtrValue;
            var nativePtr = Unsafe.SceneAPI.GetPrefabManager(scenePtr);
            
            if (nativePtr == null)
                throw new InvalidOperationException("Could not get PrefabManager from scene.");
            
            return new PrefabManager(nativePtr);
        }
    }

    /// <summary>
    /// Initializes a new instance of the PrefabManager wrapper.
    /// </summary>
    internal unsafe PrefabManager(void* nativePtr)
    {
        if (nativePtr == null)
            throw new ArgumentNullException(nameof(nativePtr));
        _nativePtr = nativePtr;
    }

    /// <summary>
    /// Registers a prefab path and returns its deterministic hash.
    /// </summary>
    public uint RegisterPrefab(string path)
    {
        if (string.IsNullOrEmpty(path))
            throw new ArgumentException("Path cannot be null or empty.", nameof(path));

        unsafe
        {
            return Unsafe.PrefabAPI.RegisterPrefab(_nativePtr, path);
        }
    }

    /// <summary>
    /// Gets the prefab path associated with a hash.
    /// </summary>
    public string GetPrefabPath(uint hash)
    {
        unsafe
        {
            var path = Unsafe.PrefabAPI.GetPrefabPath(_nativePtr, hash);
            return path ?? string.Empty;
        }
    }

    /// <summary>
    /// Checks if a prefab hash is registered.
    /// </summary>
    public bool IsRegistered(uint hash)
    {
        unsafe
        {
            return Unsafe.PrefabAPI.IsRegistered(_nativePtr, hash);
        }
    }

    /// <summary>
    /// Instantiates a prefab by path.
    /// </summary>
    /// <returns>The entity ID of the instantiated prefab root.</returns>
    public uint Instantiate(string path)
    {
        if (string.IsNullOrEmpty(path))
            throw new ArgumentException("Path cannot be null or empty.", nameof(path));
        
        unsafe
        {
            return Unsafe.PrefabAPI.Instantiate(_nativePtr, path);
        }
    }

    /// <summary>
    /// Instantiates a prefab as a child of another entity.
    /// </summary>
    /// <returns>The entity ID of the instantiated prefab root.</returns>
    public uint InstantiateAsChild(string path, uint parentEntityId)
    {
        if (string.IsNullOrEmpty(path))
            throw new ArgumentException("Path cannot be null or empty.", nameof(path));
        
        unsafe
        {
            return Unsafe.PrefabAPI.InstantiateAsChild(_nativePtr, path, parentEntityId);
        }
    }

    /// <summary>
    /// Tracks a prefab instance in the manager.
    /// Called automatically during instantiation and scene loading.
    /// </summary>
    public void TrackInstance(uint entityId, uint prefabHash)
    {
        unsafe
        {
            Unsafe.PrefabAPI.TrackInstance(_nativePtr, entityId, prefabHash);
        }
    }

    /// <summary>
    /// Stops tracking a prefab instance in the manager.
    /// Called when an instance is detached or destroyed.
    /// </summary>
    public void UntrackInstance(uint entityId)
    {
        unsafe
        {
            Unsafe.PrefabAPI.UntrackInstance(_nativePtr, entityId);
        }
    }

    /// <summary>
    /// Gets the number of instances of a prefab currently in the world.
    /// </summary>
    public uint GetInstanceCount(uint prefabHash)
    {
        unsafe
        {
            return Unsafe.PrefabAPI.GetInstanceCount(_nativePtr, prefabHash);
        }
    }

    /// <summary>
    /// Synchronizes a prefab instance with its template.
    /// Applies template changes to the instance.
    /// </summary>
    public bool SynchronizeInstance(uint entityId, string prefabPath)
    {
        if (string.IsNullOrEmpty(prefabPath))
            throw new ArgumentException("Path cannot be null or empty.", nameof(prefabPath));

        unsafe
        {            
            return Unsafe.PrefabAPI.SynchronizeInstance(_nativePtr, entityId, prefabPath);
        }
    }

    /// <summary>
    /// Detaches a prefab instance, converting it to a standalone entity.
    /// Removes the PrefabInstanceMetadata component.
    /// </summary>
    public bool DetachInstance(uint entityId)
    {
        unsafe
        {
            return Unsafe.PrefabAPI.DetachInstance(_nativePtr, entityId);
        }
    }

    /// <summary>
    /// Gets the native pointer to the underlying PrefabManager.
    /// </summary>
    internal unsafe void* GetNativePtr() => _nativePtr;
}
