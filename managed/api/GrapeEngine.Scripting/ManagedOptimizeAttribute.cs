using System;
using System.Diagnostics;

namespace GrapeEngine.Scripting
{
    /// <summary>
    /// Marks a method, class, or delegate for managed code optimization.
    /// Similar to Unity's [BurstCompile], this attribute signals to the runtime
    /// to apply AOT compilation, SIMD vectorization, and profile-guided optimization.
    /// </summary>
    [AttributeUsage(AttributeTargets.Method | AttributeTargets.Class | AttributeTargets.Delegate, AllowMultiple = false)]
    [Conditional("DEBUG")]
    public class ManagedOptimizeAttribute : Attribute
    {
        /// <summary>
        /// Performance target in milliseconds. If execution exceeds this,
        /// the method is candidates for enhanced optimization passes.
        /// </summary>
        public float PerformanceTargetMs { get; set; } = 1.0f;

        /// <summary>
        /// Enable SIMD intrinsics for vectorizable operations.
        /// Default: true for hot paths.
        /// </summary>
        public bool EnableSIMD { get; set; } = true;

        /// <summary>
        /// Enable aggressive inlining and constant folding.
        /// Default: true.
        /// </summary>
        public bool EnableAggressiveOptimization { get; set; } = true;

        /// <summary>
        /// Enable memory pooling and allocation elimination.
        /// Default: true.
        /// </summary>
        public bool EnableMemoryOptimization { get; set; } = true;

        /// <summary>
        /// Profile this method and apply profile-guided optimizations.
        /// Default: true.
        /// </summary>
        public bool EnablePGO { get; set; } = true;

        /// <summary>
        /// Safety level for optimizations (Strict, Normal, Aggressive).
        /// Strict = no unsafe operations
        /// Normal = struct blitting allowed
        /// Aggressive = unsafe pointers and direct memory access
        /// </summary>
        public OptimizationSafety SafetyLevel { get; set; } = OptimizationSafety.Normal;

        /// <summary>
        /// Whether to compile to native code via AOT.
        /// Default: true when available.
        /// </summary>
        public bool EnableAOT { get; set; } = true;

        /// <summary>
        /// Create an optimization attribute with default settings.
        /// </summary>
        public ManagedOptimizeAttribute()
        {
          
        }

        /// <summary>
        /// Create an optimization attribute targeting specific performance goal.
        /// </summary>
        public ManagedOptimizeAttribute(float performanceTargetMs)
        {
            PerformanceTargetMs = performanceTargetMs;
        }
    }

    /// <summary>
    /// Safety level for managed code optimizations.
    /// </summary>
    public enum OptimizationSafety
    {
        /// <summary>
        /// No unsafe operations, full runtime verification.
        /// </summary>
        Strict = 0,

        /// <summary>
        /// Allow struct blitting and limited unsafe code.
        /// </summary>
        Normal = 1,

        /// <summary>
        /// Allow aggressive unsafe pointers and direct memory access.
        /// </summary>
        Aggressive = 2
    }

    /// <summary>
    /// Marks a method as a hot path that would benefit from aggressive optimization.
    /// Used by profilers to prioritize optimization candidates.
    /// </summary>
    [AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
    public class HotPathAttribute : Attribute
    {
        /// <summary>Estimated call frequency per frame (Low, Medium, High, Critical).</summary>
        public HotPathFrequency Frequency { get; set; } = HotPathFrequency.High;

        /// <summary>Estimated entities/objects processed per call.</summary>
        public int EstimatedThroughput { get; set; } = 1000;

        public HotPathAttribute()
        {
        }

        public HotPathAttribute(HotPathFrequency frequency)
        {
            Frequency = frequency;
        }
    }

    /// <summary>
    /// Frequency classification for hot paths.
    /// </summary>
    public enum HotPathFrequency
    {
        Low = 0,      // Called occasionally
        Medium = 1,   // Called regularly
        High = 2,     // Called every frame
        Critical = 3  // Called many times per frame
    }

    /// <summary>
    /// Marks data that should be memory-aligned for SIMD operations.
    /// Applied to structs to hint the runtime about layout optimization.
    /// </summary>
    [AttributeUsage(AttributeTargets.Struct | AttributeTargets.Field, AllowMultiple = false)]
    public class SIMDAlignAttribute : Attribute
    {
        /// <summary>Alignment requirement in bytes (16, 32, 64 typically).</summary>
        public int AlignmentBytes { get; set; } = 16;

        public SIMDAlignAttribute()
        {
        }

        public SIMDAlignAttribute(int alignmentBytes)
        {
            AlignmentBytes = alignmentBytes;
        }
    }

    /// <summary>
    /// Marks a loop or method for vectorization via SIMD.
    /// Provides hints to the optimizer about vectorizable patterns.
    /// </summary>
    [AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
    public class VectorizeAttribute : Attribute
    {
        /// <summary>Preferred SIMD width (128, 256, 512 bits).</summary>
        public int PreferredWidthBits { get; set; } = 256;

        /// <summary>Whether to allow mixed-width operations.</summary>
        public bool AllowMixedWidth { get; set; } = true;

        public VectorizeAttribute()
        {
        }

        public VectorizeAttribute(int preferredWidthBits)
        {
            PreferredWidthBits = preferredWidthBits;
        }
    }

    /// <summary>
    /// Marks a method for inlining by the JIT compiler.
    /// More aggressive than standard [MethodImpl(MethodImplOptions.AggressiveInlining)].
    /// </summary>
    [AttributeUsage(AttributeTargets.Method, AllowMultiple = false)]
    public class ForceInlineAttribute : Attribute
    {
        /// <summary>Maximum depth for recursive inlining.</summary>
        public int MaxInlineDepth { get; set; } = 5;

        public ForceInlineAttribute()
        {
        }

        public ForceInlineAttribute(int maxInlineDepth)
        {
            MaxInlineDepth = maxInlineDepth;
        }
    }
}
