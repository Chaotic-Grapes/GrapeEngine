/* Start Header *****************************************************************/
/*!
\file   ComponentAccessDiagnostics.cs
\brief  Diagnostic utilities for debugging component access issues
*/
/* End Header *******************************************************************/

using System;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Components;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Internal.Diagnostics;

/// <summary>
/// Diagnostic utilities for debugging component access and memory layout issues
/// </summary>
internal static class ComponentAccessDiagnostics
{
    /// <summary>
    /// Debug component registration and hash generation
    /// </summary>
    public static void DiagnoseComponentRegistration()
    {
        Logging.LogInternal("=== Component Registration Diagnostics ===", LogLevel.Info);
        
        // Test critical components that were failing
        DiagnoseComponent<LocalTransform>("LocalTransform");
        DiagnoseComponent<WorldTransform>("WorldTransform"); 
        DiagnoseComponent<LinearVelocity2D>("LinearVelocity2D");
        DiagnoseComponent<AngularVelocity2D>("AngularVelocity2D");
        DiagnoseComponent<Rigidbody2D>("Rigidbody2D");
        DiagnoseComponent<BoxCollider2D>("BoxCollider2D");
        DiagnoseComponent<CircleCollider2D>("CircleCollider2D");
        DiagnoseComponent<AnimationState2D>("AnimationState2D");
        DiagnoseComponent<AudioSource>("AudioSource");
        DiagnoseComponent<Camera3D>("Camera3D");
        DiagnoseComponent<Light2D>("Light2D");
        DiagnoseComponent<ZIndex2D>("ZIndex2D");
        DiagnoseComponent<PhysicsMaterial2D>("PhysicsMaterial2D");
        DiagnoseComponent<LayerMask>("LayerMask");
    }
    
    private static void DiagnoseComponent<T>(string expectedName) where T : unmanaged
    {
        try
        {
            uint hash = ComponentTypeRegistry.GetTypeHash<T>();
            int size = Marshal.SizeOf<T>();
            bool isRegistered = ComponentRegistry.IsRegistered<T>();
            
            Logging.LogInternal($"{expectedName}:", LogLevel.Info);
            Logging.LogInternal($"  Hash: 0x{hash:X8}", LogLevel.Info);
            Logging.LogInternal($"  Size: {size} bytes", LogLevel.Info);
            Logging.LogInternal($"  Registered: {isRegistered}", LogLevel.Info);
            Logging.LogInternal($"  Type: {typeof(T).FullName}", LogLevel.Info);
            Logging.LogInternal("", LogLevel.Info);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"{expectedName}: ERROR - {ex.Message}", LogLevel.Error);
            Logging.LogInternal("", LogLevel.Info);
        }
    }
    
    /// <summary>
    /// Test FNV-1a hash algorithm implementation
    /// </summary>
    public static void TestHashAlgorithm()
    {
        Logging.LogInternal("=== Hash Algorithm Test ===", LogLevel.Info);
        
        string[] testStrings = {
            "LocalTransform",
            "WorldTransform", 
            "LinearVelocity2D",
            "AngularVelocity2D",
            "Rigidbody2D",
            "BoxCollider2D",
            "CircleCollider2D"
        };
        
        foreach (string testStr in testStrings)
        {
            uint hash = FNV1aHash(testStr);
            Logging.LogInternal($"{testStr}: 0x{hash:X8}", LogLevel.Info);
        }
    }
    
    /// <summary>
    /// Reference FNV-1a implementation for testing
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