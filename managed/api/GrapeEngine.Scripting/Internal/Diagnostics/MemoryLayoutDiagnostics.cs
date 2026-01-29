/* Start Header *****************************************************************/
/*!
\file   MemoryLayoutDiagnostics.cs  
\brief  Diagnostic utilities for comparing C# and C++ component memory layouts
*/
/* End Header *******************************************************************/

using System;
using System.Runtime.InteropServices;
using GrapeEngine.Scripting.Components;

namespace GrapeEngine.Scripting.Internal.Diagnostics;

/// <summary>
/// Diagnostic utilities for validating component memory layouts match between C# and C++
/// </summary>
internal static class MemoryLayoutDiagnostics
{
    /// <summary>
    /// Print size and layout information for all components
    /// </summary>
    public static void DiagnoseComponentSizes()
    {
        Logging.LogInternal("=== Component Memory Layout Diagnostics ===", LogLevel.Info);
        Logging.LogInternal("C# Component Sizes (should match C++ counterparts):", LogLevel.Info);
        Logging.LogInternal("", LogLevel.Info);
        
        // Core components
        PrintComponentSize<Name>("Name");
        PrintComponentSize<TagMask>("TagMask"); 
        PrintComponentSize<Active>("Active");
        PrintComponentSize<PrefabInstanceMetadata>("PrefabInstanceMetadata");
        
        // Transform components  
        PrintComponentSize<LocalTransform>("LocalTransform");
        PrintComponentSize<WorldTransform>("WorldTransform");
        
        // Physics 2D components
        PrintComponentSize<LinearVelocity2D>("LinearVelocity2D");
        PrintComponentSize<Acceleration2D>("Acceleration2D");  
        PrintComponentSize<AngularVelocity2D>("AngularVelocity2D");
        PrintComponentSize<Rigidbody2D>("Rigidbody2D");
        PrintComponentSize<PhysicsMaterial2D>("PhysicsMaterial2D");
        PrintComponentSize<BoxCollider2D>("BoxCollider2D");
        PrintComponentSize<CircleCollider2D>("CircleCollider2D");
        
        // Rendering components
        PrintComponentSize<SpriteRenderer2D>("SpriteRenderer2D");
        PrintComponentSize<AnimationState2D>("AnimationState2D");
        PrintComponentSize<ZIndex2D>("ZIndex2D");
        PrintComponentSize<Light2D>("Light2D");
        
        // Camera components
        PrintComponentSize<Camera3D>("Camera3D");
        PrintComponentSize<CameraMatrices>("CameraMatrices");
        
        // Audio components
        PrintComponentSize<AudioSource>("AudioSource");
        
        // Other
        PrintComponentSize<LayerMask>("LayerMask");
        
        Logging.LogInternal("", LogLevel.Info);
        Logging.LogInternal("Expected C++ sizes based on Components.h:", LogLevel.Info);
        Logging.LogInternal("- Name: 64 bytes (char[64])", LogLevel.Info);
        Logging.LogInternal("- TagMask: 4 bytes (uint32_t)", LogLevel.Info);
        Logging.LogInternal("- Active: 1 byte (bool)", LogLevel.Info);
        Logging.LogInternal("- LocalTransform: ~40 bytes (Vector3D + Quaternion + Vector3D)", LogLevel.Info);
        Logging.LogInternal("- WorldTransform: ~68 bytes (Matrix4x4 + bool + padding)", LogLevel.Info);
        Logging.LogInternal("- LinearVelocity2D: 8 bytes (Vector2D)", LogLevel.Info);
        Logging.LogInternal("- AngularVelocity2D: 4 bytes (float)", LogLevel.Info);
        Logging.LogInternal("- Rigidbody2D: ~24 bytes (5 floats + uint32_t)", LogLevel.Info); 
        Logging.LogInternal("- BoxCollider2D: ~24 bytes (2 Vector2D + float + 2 uint32_t)", LogLevel.Info);
        Logging.LogInternal("- CircleCollider2D: ~16 bytes (float + Vector2D + 2 uint32_t)", LogLevel.Info);
        Logging.LogInternal("- Camera3D: ~28 bytes (2 bool + 5 float)", LogLevel.Info);
        Logging.LogInternal("- AudioSource: ~24 bytes (uint32_t + 3 float + 3 bool)", LogLevel.Info);
        Logging.LogInternal("- Light2D: ~40+ bytes (enum + 2 Vector3D + Color + 2 float + bool)", LogLevel.Info);
    }
    
    private static void PrintComponentSize<T>(string name) where T : unmanaged
    {
        int size = Marshal.SizeOf<T>();
        Logging.LogInternal($"{name,-20}: {size,3} bytes", LogLevel.Info);
    }
    
    /// <summary>
    /// Check for struct layout and alignment issues
    /// </summary>
    public static void DiagnoseStructLayouts()
    {
        Logging.LogInternal("=== Struct Layout Analysis ===", LogLevel.Info);
        Logging.LogInternal("", LogLevel.Info);
        
        // Check critical components for proper sequential layout
        AnalyzeStructLayout<LinearVelocity2D>("LinearVelocity2D");
        AnalyzeStructLayout<AngularVelocity2D>("AngularVelocity2D");
        AnalyzeStructLayout<Rigidbody2D>("Rigidbody2D");
        AnalyzeStructLayout<BoxCollider2D>("BoxCollider2D");
        AnalyzeStructLayout<CircleCollider2D>("CircleCollider2D");
    }
    
    private static void AnalyzeStructLayout<T>(string name) where T : unmanaged
    {
        Type type = typeof(T);
        var structLayoutAttr = type.GetCustomAttribute<StructLayoutAttribute>();
        
        Logging.LogInternal($"{name}:", LogLevel.Info);
        Logging.LogInternal($"  Size: {Marshal.SizeOf<T>()} bytes", LogLevel.Info);
        
        if (structLayoutAttr != null)
        {
            Logging.LogInternal($"  Layout: {structLayoutAttr.Value}", LogLevel.Info);
            Logging.LogInternal($"  Pack: {structLayoutAttr.Pack}", LogLevel.Info);  
            Logging.LogInternal($"  Size (attribute): {structLayoutAttr.Size}", LogLevel.Info);
        }
        else
        {
            Logging.LogInternal($"  Layout: No StructLayout attribute!", LogLevel.Info);
        }
        
        var fields = type.GetFields();
        int offset = 0;
        foreach (var field in fields)
        {
            var fieldOffset = Marshal.OffsetOf<T>(field.Name);
            Logging.LogInternal($"  {field.Name}: offset {fieldOffset}, type {field.FieldType.Name}", LogLevel.Info);
        }
        
        Logging.LogInternal("", LogLevel.Info);
    }
}