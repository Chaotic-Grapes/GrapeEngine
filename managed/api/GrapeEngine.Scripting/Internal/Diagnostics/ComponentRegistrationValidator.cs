/* Start Header *****************************************************************/
/*!
\file   ComponentRegistrationValidator.cs
\brief  Validation utilities to test component registration and access
*/
/* End Header *******************************************************************/

using System;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Internal.Diagnostics;

/// <summary>
/// Utilities to validate that component registration is working correctly
/// and that C# can access C++ components without memory corruption.
/// </summary>
internal static class ComponentRegistrationValidator
{
    /// <summary>
    /// Test component registration for all critical components that were failing.
    /// This should be called after initialization to verify everything is working.
    /// </summary>
    public static void ValidateAllCriticalComponents()
    {
        Logging.LogInternal("=== Component Registration Validation ===", LogLevel.Info);
        
        // Test the components that were specifically mentioned as problematic
        var criticalComponents = new[]
        {
            typeof(AnimationState2D),
            typeof(AudioSource),
            typeof(Camera3D),
            typeof(CircleCollider2D),
            typeof(Light2D),
            typeof(PhysicsMaterial2D),
            typeof(LocalTransform),
            typeof(WorldTransform),
            typeof(ZIndex2D),
            typeof(LinearVelocity2D),
            typeof(AngularVelocity2D),
            typeof(Rigidbody2D),
            typeof(BoxCollider2D),
            typeof(LayerMask)
        };
        
        Logging.LogInternal("Testing registration status:", LogLevel.Info);
        foreach (var componentType in criticalComponents)
        {
            TestComponentRegistration(componentType);
        }
        
        Logging.LogInternal("Testing hash consistency between C# and C++ for core components:", LogLevel.Info);
        TestHashConsistency();
    }
    
    private static void TestComponentRegistration(Type componentType)
    {
        try
        {
            // Get the generic method ComponentRegistry.IsRegistered<T>
            var isRegisteredMethod = typeof(ComponentRegistry)
                .GetMethod("IsRegistered", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)
                ?.MakeGenericMethod(componentType);
                
            if (isRegisteredMethod == null)
            {
                Logging.LogInternal($"{componentType.Name}: Cannot access IsRegistered method", LogLevel.Error);
                return;
            }
            
            bool isRegistered = (bool)isRegisteredMethod.Invoke(null, [])!;
            
            // Get component hash
            var getHashMethod = typeof(ComponentTypeHelper)
                .GetMethod("GetTypeHash", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)
                ?.MakeGenericMethod(componentType);
                
            uint hash = getHashMethod != null ? (uint)getHashMethod.Invoke(null, [])! : 0;
            
            if (isRegistered)
            {
                Logging.LogInternal($"{componentType.Name}: Registered (hash: 0x{hash:X8})", LogLevel.Info);
            }
            else
            {
                Logging.LogInternal($"{componentType.Name}: NOT registered (hash: 0x{hash:X8})", LogLevel.Error);
                
                // Try to register it manually
                var registerMethod = typeof(ComponentRegistry)
                    .GetMethod("Register", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)
                    ?.MakeGenericMethod(componentType);
                    
                if (registerMethod != null)
                {
                    bool registered = (bool)registerMethod.Invoke(null, [])!;
                    Logging.LogInternal($"   Manual registration attempt: {(registered ? "Success" : "Failed")}", LogLevel.Info);
                }
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"{componentType.Name}: Exception - {ex.Message}", LogLevel.Error);
        }
    }
    
    private static void TestHashConsistency()
    {
        // These are the expected hashes based on the C++ registration
        // Calculated using FNV-1a with the component names
        var expectedHashes = new Dictionary<string, uint>
        {
            { "LocalTransform", FNV1aHash("LocalTransform") },
            { "WorldTransform", FNV1aHash("WorldTransform") },
            { "LinearVelocity2D", FNV1aHash("LinearVelocity2D") },
            { "AngularVelocity2D", FNV1aHash("AngularVelocity2D") },
            { "Rigidbody2D", FNV1aHash("Rigidbody2D") },
            { "BoxCollider2D", FNV1aHash("BoxCollider2D") },
            { "CircleCollider2D", FNV1aHash("CircleCollider2D") },
            { "AnimationState2D", FNV1aHash("AnimationState2D") },
            { "AudioSource", FNV1aHash("AudioSource") },
            { "Camera3D", FNV1aHash("Camera3D") },
            { "Light2D", FNV1aHash("Light2D") },
            { "ZIndex2D", FNV1aHash("ZIndex2D") },
            { "PhysicsMaterial2D", FNV1aHash("PhysicsMaterial2D") }
        };
        
        var componentTypes = new Dictionary<string, Type>
        {
            { "LocalTransform", typeof(LocalTransform) },
            { "WorldTransform", typeof(WorldTransform) },
            { "LinearVelocity2D", typeof(LinearVelocity2D) },
            { "AngularVelocity2D", typeof(AngularVelocity2D) },
            { "Rigidbody2D", typeof(Rigidbody2D) },
            { "BoxCollider2D", typeof(BoxCollider2D) },
            { "CircleCollider2D", typeof(CircleCollider2D) },
            { "AnimationState2D", typeof(AnimationState2D) },
            { "AudioSource", typeof(AudioSource) },
            { "Camera3D", typeof(Camera3D) },
            { "Light2D", typeof(Light2D) },
            { "ZIndex2D", typeof(ZIndex2D) },
            { "PhysicsMaterial2D", typeof(PhysicsMaterial2D) }
        };
        
        foreach (var (name, expectedHash) in expectedHashes)
        {
            if (componentTypes.TryGetValue(name, out var componentType))
            {
                var getHashMethod = typeof(ComponentTypeHelper)
                    .GetMethod("GetTypeHash", System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Static)
                    ?.MakeGenericMethod(componentType);
                    
                if (getHashMethod != null)
                {
                    uint actualHash = (uint)getHashMethod.Invoke(null, [])!;
                    
                    if (actualHash == expectedHash)
                    {
                        Logging.LogInternal($"{name}: Hash matches (0x{actualHash:X8})", LogLevel.Info);
                    }
                    else
                    {
                        Logging.LogInternal($"{name}: Hash mismatch! Expected 0x{expectedHash:X8}, got 0x{actualHash:X8}", LogLevel.Error);
                    }
                }
            }
        }
    }
    
    /// <summary>
    /// Reference FNV-1a implementation to verify expected hashes
    /// </summary>
    private static uint FNV1aHash(string str)
    {
        const uint fnvPrime = 0x01000193;     // FNV prime  
        const uint fnvOffset = 0x811C9DC5;    // FNV offset basis

        var hash = fnvOffset;
        foreach (var c in str)
        {
            hash ^= c;
            hash *= fnvPrime;
        }

        return hash;
    }
}