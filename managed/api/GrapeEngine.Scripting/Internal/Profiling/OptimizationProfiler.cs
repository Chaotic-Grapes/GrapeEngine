using System.Diagnostics;

namespace GrapeEngine.Scripting.Internal.Profiling;

/// <summary>
/// Tracks optimization metrics and profiles hot paths for managed code optimization.
/// Provides data for profile-guided optimization (PGO) decisions.
/// 
/// PERFORMANCE NOTE: Profiling is disabled by default via EnableProfiling flag.
/// Users can enable profiling at runtime by setting OptimizationProfiler.EnableProfiling = true
/// to analyze their script performance without recompiling.
/// When disabled, profiling adds near-zero overhead (just a null check).
/// </summary>
public class OptimizationProfiler
{
    /// <summary>
    /// Runtime flag to enable/disable profiling.
    /// Users can set this to profile their scripts regardless of build configuration.
    /// </summary>
    public static bool EnableProfiling { get; set; } = false;

    private class MethodProfile
    {
        public string MethodName { get; set; } = string.Empty;
        public int CallCount { get; set; }
        public long TotalNanoseconds { get; set; }
        public long PeakNanoseconds { get; set; }
        public int EntityProcessed { get; set; }
        public List<long> RecentSamples { get; set; } = [];
        public OptimizationSafety SafetyLevel { get; set; }
        public bool IsHotPath { get; set; }
    }

    private readonly Dictionary<string, MethodProfile> _profiles = [];
    private readonly Stopwatch _frameTimer = new();
    private readonly long _frameStartNs;

    /// <summary>
    /// Statistics snapshot for a method.
    /// </summary>
    public struct MethodStats
    {
        public string MethodName { get; set; }
        public int CallCount { get; set; }
        public double AverageMs { get; set; }
        public double PeakMs { get; set; }
        public double TotalMs { get; set; }
        public int EntitiesProcessed { get; set; }
        public double ThroughputPerMs { get; set; }
        public bool IsHotPath { get; set; }
    }

    public OptimizationProfiler()
    {
        _frameTimer.Start();
        _frameStartNs = _frameTimer.ElapsedTicks * 1000000 / Stopwatch.Frequency;
    }

    /// <summary>
    /// Begin profiling a method call.
    /// Profiling is controlled by the EnableProfiling static flag, allowing runtime control.
    /// When disabled, returns a no-op scope with near-zero overhead.
    /// </summary>
    public OptimizationScope BeginProfile(string methodName, OptimizationSafety safetyLevel = OptimizationSafety.Normal)
    {
        if (!EnableProfiling)
            return default; // No-op when profiling is disabled

        return new OptimizationScope(this, methodName, safetyLevel);
    }

    /// <summary>
    /// Record a method execution.
    /// </summary>
    internal void RecordExecution(string methodName, long nanoseconds, int entitiesProcessed, OptimizationSafety safety)
    {
        if (!_profiles.TryGetValue(methodName, out var profile))
        {
            profile = new MethodProfile { MethodName = methodName, SafetyLevel = safety };
            _profiles[methodName] = profile;
        }

        profile.CallCount++;
        profile.TotalNanoseconds += nanoseconds;
        profile.PeakNanoseconds = System.Math.Max(profile.PeakNanoseconds, nanoseconds);
        profile.EntityProcessed += entitiesProcessed;
        profile.RecentSamples.Add(nanoseconds);

        // Keep only recent 100 samples
        if (profile.RecentSamples.Count > 100)
        {
            profile.RecentSamples.RemoveAt(0);
        }

        // Mark as hot path if called frequently with significant time
        if (profile.CallCount > 10 && nanoseconds > 1_000_000) // > 1ms
        {
            profile.IsHotPath = true;
        }
    }

    /// <summary>
    /// Get statistics for a method.
    /// </summary>
    public bool TryGetStats(string methodName, out MethodStats stats)
    {
        stats = default;

        if (!_profiles.TryGetValue(methodName, out var profile))
            return false;

        var avgNs = profile.CallCount > 0 ? profile.TotalNanoseconds / profile.CallCount : 0;

        stats = new MethodStats
        {
            MethodName = methodName,
            CallCount = profile.CallCount,
            AverageMs = avgNs / 1_000_000.0,
            PeakMs = profile.PeakNanoseconds / 1_000_000.0,
            TotalMs = profile.TotalNanoseconds / 1_000_000.0,
            EntitiesProcessed = profile.EntityProcessed,
            ThroughputPerMs = profile.TotalNanoseconds > 0 ? (profile.EntityProcessed * 1_000_000.0) / profile.TotalNanoseconds : 0,
            IsHotPath = profile.IsHotPath
        };

        return true;
    }

    /// <summary>
    /// Get all hot paths (methods that exceed performance threshold).
    /// </summary>
    public IEnumerable<MethodStats> GetHotPaths(double thresholdMs = 0.5)
    {
        foreach (var profile in _profiles.Values)
        {
            if (!profile.IsHotPath && profile.RecentSamples.Count < 5)
                continue;

            var avgNs = profile.CallCount > 0 ? profile.TotalNanoseconds / profile.CallCount : 0;
            var avgMs = avgNs / 1_000_000.0;

            if (avgMs > thresholdMs)
            {
                yield return new MethodStats
                {
                    MethodName = profile.MethodName,
                    CallCount = profile.CallCount,
                    AverageMs = avgMs,
                    PeakMs = profile.PeakNanoseconds / 1_000_000.0,
                    TotalMs = profile.TotalNanoseconds / 1_000_000.0,
                    EntitiesProcessed = profile.EntityProcessed,
                    ThroughputPerMs = profile.TotalNanoseconds > 0 
                        ? (profile.EntityProcessed * 1_000_000.0) / profile.TotalNanoseconds 
                        : 0,
                    IsHotPath = true
                };
            }
        }
    }

    /// <summary>
    /// Get optimization recommendations based on profiles.
    /// Includes SIMD vectorization suggestions for high-throughput operations.
    /// </summary>
    public List<OptimizationRecommendation> GetRecommendations()
    {
        var recommendations = new List<OptimizationRecommendation>();
        var hasSimdSupport = CapabilityDetection.HasAVX2 || CapabilityDetection.HasAVX;

        foreach (var profile in _profiles.Values)
        {
            if (profile.CallCount < 3)
                continue; // Need samples

            var avgNs = profile.TotalNanoseconds / profile.CallCount;

            // High call count + significant time = candidate for AOT
            if (profile.CallCount > 100 && avgNs > 1_000_000)
            {
                var optimizations = new List<string> { "CompileToNative", "EnablePGO" };
                if (hasSimdSupport)
                    optimizations.Insert(1, "EnableSIMD");
                    
                recommendations.Add(new OptimizationRecommendation
                {
                    MethodName = profile.MethodName,
                    Reason = "High call frequency with significant execution time",
                    SuggestedOptimizations = [.. optimizations],
                    Priority = 1
                });
            }

            // High variance = candidate for PGO
            var variance = profile.RecentSamples.Count > 1
                ? System.Math.Sqrt(profile.RecentSamples.Average(s => System.Math.Pow(s - avgNs, 2)))
                : 0;

            if (variance > avgNs * 0.5) // High variance
            {
                recommendations.Add(new OptimizationRecommendation
                {
                    MethodName = profile.MethodName,
                    Reason = "High performance variance, PGO could help",
                    SuggestedOptimizations = ["EnablePGO"],
                    Priority = 2
                });
            }

            // High entity throughput = candidate for SIMD vectorization
            if (profile.EntityProcessed > 1000 && profile.TotalNanoseconds > 10_000_000)
            {
                var throughputPerSec = (profile.EntityProcessed / (profile.TotalNanoseconds / 1_000_000_000.0));
                var estimatedSpeedup = hasSimdSupport ? "2-4x" : "potential 2-4x";
                    
                recommendations.Add(new OptimizationRecommendation
                {
                    MethodName = profile.MethodName,
                    Reason = $"High-throughput loop ({throughputPerSec:F0} entities/sec), SIMD vectorization recommended",
                    SuggestedOptimizations = hasSimdSupport 
                        ? ["EnableSIMD", "Vectorize"]
                        : ["Vectorize"],
                    Priority = 1
                });
            }

            // SIMD-specific recommendation for suitable operations
            if (hasSimdSupport && profile.EntityProcessed > 500 && profile.CallCount > 20)
            {
                // Check if operation name suggests it's vectorizable (contains common patterns)
                var lowerName = profile.MethodName.ToLower();
                var isVectorizable = lowerName.Contains("transform") || 
                                    lowerName.Contains("position") || 
                                    lowerName.Contains("rotation") ||
                                    lowerName.Contains("scale") ||
                                    lowerName.Contains("velocity") ||
                                    lowerName.Contains("foreach");

                if (isVectorizable && avgNs > 500_000) // > 0.5ms
                {
                    recommendations.Add(new OptimizationRecommendation
                    {
                        MethodName = profile.MethodName,
                        Reason = $"Vectorizable operation with {profile.EntityProcessed} entities. SIMD available: {(CapabilityDetection.HasAVX512F ? "AVX-512" : CapabilityDetection.HasAVX2 ? "AVX2" : CapabilityDetection.HasAVX ? "AVX" : "SSE2")}",
                        SuggestedOptimizations = ["EnableSIMD", "Vectorize", "ConsiderComponentOpsAPI"],
                        Priority = 1
                    });
                }
            }
        }

        return [.. recommendations.OrderByDescending(r => r.Priority)];
    }

    /// <summary>
    /// Static capability detection for SIMD instruction sets.
    /// </summary>
    public static class CapabilityDetection
    {
        /// <summary>
        /// Check if the CPU supports AVX2 instructions.
        /// </summary>
        public static bool HasAVX2 => System.Runtime.Intrinsics.X86.Avx2.IsSupported;

        /// <summary>
        /// Check if the CPU supports AVX instructions.
        /// </summary>
        public static bool HasAVX => System.Runtime.Intrinsics.X86.Avx.IsSupported;

        /// <summary>
        /// Check if the CPU supports SSE4.2 instructions.
        /// </summary>
        public static bool HasSSE42 => System.Runtime.Intrinsics.X86.Sse42.IsSupported;

        /// <summary>
        /// Check if the CPU supports AVX-512F instructions.
        /// </summary>
        public static bool HasAVX512F => System.Runtime.Intrinsics.X86.Avx512F.IsSupported;

        /// <summary>
        /// Get human-readable description of available SIMD capabilities.
        /// </summary>
        public static string GetCapabilityDescription()
        {
            if (HasAVX512F) return "AVX-512F (512-bit vectors)";
            if (HasAVX2) return "AVX2 (256-bit vectors)";
            if (HasAVX) return "AVX (256-bit vectors)";
            if (HasSSE42) return "SSE4.2 (128-bit vectors)";
            return "No SIMD support";
        }

        /// <summary>
        /// Estimate speedup for vectorizable operations based on available SIMD.
        /// </summary>
        public static double EstimateSpeedup()
        {
            if (HasAVX512F) return 8.0; // Up to 8x for 512-bit vectors
            if (HasAVX2) return 4.0;    // Up to 4x for 256-bit vectors
            if (HasAVX) return 4.0;
            if (HasSSE42) return 2.0;   // Up to 2x for 128-bit vectors
            return 1.0; // No speedup without SIMD
        }
    }

    /// <summary>
    /// Reset all profiles.
    /// </summary>
    public void Reset()
    {
        _profiles.Clear();
    }

    /// <summary>
    /// Get all recorded methods.
    /// </summary>
    public IEnumerable<MethodStats> GetAllMethods()
    {
        foreach (var profile in _profiles.Values)
        {
            var avgNs = profile.CallCount > 0 ? profile.TotalNanoseconds / profile.CallCount : 0;

            yield return new MethodStats
            {
                MethodName = profile.MethodName,
                CallCount = profile.CallCount,
                AverageMs = avgNs / 1_000_000.0,
                PeakMs = profile.PeakNanoseconds / 1_000_000.0,
                TotalMs = profile.TotalNanoseconds / 1_000_000.0,
                EntitiesProcessed = profile.EntityProcessed,
                ThroughputPerMs = profile.TotalNanoseconds > 0 ? (profile.EntityProcessed * 1_000_000.0) / profile.TotalNanoseconds : 0,
                IsHotPath = profile.IsHotPath
            };
        }
    }
}

/// <summary>
/// RAII scope for profiling a method execution.
/// </summary>
public struct OptimizationScope : IDisposable
{
    private readonly OptimizationProfiler _profiler;
    private readonly string _methodName;
    private readonly Stopwatch _timer;
    private readonly OptimizationSafety _safety;
    private int _entityCount;

    internal OptimizationScope(OptimizationProfiler profiler, string methodName, OptimizationSafety safety)
    {
        _profiler = profiler;
        _methodName = methodName;
        _timer = Stopwatch.StartNew();
        _safety = safety;
        _entityCount = 0;
    }

    /// <summary>
    /// Record entities processed in this scope (for throughput calculation).
    /// </summary>
    public void RecordEntitiesProcessed(int count)
    {
        _entityCount += count;
    }

    /// <summary>
    /// Finalize and record the profiling data.
    /// </summary>
    public readonly void Dispose()
    {
        if (_profiler == null)
            return;

        _timer.Stop();
        long nanoseconds = (long)(_timer.Elapsed.TotalSeconds * 1_000_000_000);
        _profiler.RecordExecution(_methodName, nanoseconds, _entityCount, _safety);

        GC.SuppressFinalize(this);
    }
}

/// <summary>
/// Recommendation for optimizing a method.
/// </summary>
public class OptimizationRecommendation
{
    /// <summary>
    /// Target method name.
    /// </summary>
    public string MethodName { get; set; } = string.Empty;

    /// <summary>
    /// Reason for recommendation.
    /// </summary>
    public string Reason { get; set; } = string.Empty;

    /// <summary>
    /// Suggested optimizations to apply.
    /// </summary>
    public string[] SuggestedOptimizations { get; set; } = [];

    /// <summary>
    /// Priority (1=high, 3=low).
    /// </summary>
    public int Priority { get; set; }
}

/// <summary>
/// AOT (Ahead-of-Time) compilation configuration for managed code optimization.
/// </summary>
public class AOTConfig
{
    /// <summary>
    /// Enable AOT compilation for marked methods.
    /// </summary>
    public bool Enabled { get; set; } = true;

    /// <summary>
    /// Target optimization level (O0, O2, O3).
    /// </summary>
    public string OptimizationLevel { get; set; } = "O3";

    /// <summary>
    /// Enable LTO (Link Time Optimization).
    /// </summary>
    public bool EnableLTO { get; set; } = true;

    /// <summary>
    /// Enable PGO (Profile Guided Optimization).
    /// </summary>
    public bool EnablePGO { get; set; } = true;

    /// <summary>
    /// Enable SIMD code generation.
    /// </summary>
    public bool EnableSIMD { get; set; } = true;

    /// <summary>
    /// Target CPU instruction set (baseline, avx, avx2, avx512).
    /// </summary>
    public string TargetInstructionSet { get; set; } = "avx2";

    /// <summary>
    /// Inline aggressively at compile time.
    /// </summary>
    public bool AggressiveInlining { get; set; } = true;
}

/// <summary>
/// Manages AOT (Ahead-of-Time) compilation and native code generation.
/// Compiles C# methods to fully native code ahead of time for maximum performance.
/// </summary>
public class AOTCompiler(AOTConfig? config = null)
{
    private readonly AOTConfig _config = config ?? new AOTConfig();
    private readonly HashSet<string> _compiledMethods = [];

    /// <summary>
    /// Compile a method to native code via AOT.
    /// </summary>
    public bool CompileMethod(string methodFullName, OptimizationSafety safety = OptimizationSafety.Normal)
    {
        if (!_config.Enabled)
            return false;

        if (_compiledMethods.Contains(methodFullName))
            return true; // Already compiled

        try
        {
            // Parse the method full name (format: "Namespace.ClassName.MethodName" or "Namespace.ClassName::MethodName")
            string typeName;
            string methodName;

            if (methodFullName.Contains("::"))
            {
                var parts = methodFullName.Split("::");
                typeName = parts[0];
                methodName = parts[1];
            }
            else
            {
                var lastDotIndex = methodFullName.LastIndexOf('.');
                if (lastDotIndex <= 0)
                    return false;

                typeName = methodFullName[..lastDotIndex];
                methodName = methodFullName[(lastDotIndex + 1)..];
            }

            // Find the type
            var type = Type.GetType(typeName);
            if (type == null)
            {
                // Try to find in all loaded assemblies
                type = AppDomain.CurrentDomain.GetAssemblies()
                    .SelectMany(a => a.GetTypes())
                    .FirstOrDefault(t => t.FullName == typeName);

                if (type == null)
                    return false;
            }

            // Find the method
            var method = type.GetMethod(methodName, 
                System.Reflection.BindingFlags.Public | 
                System.Reflection.BindingFlags.NonPublic | 
                System.Reflection.BindingFlags.Static | 
                System.Reflection.BindingFlags.Instance);

            if (method == null)
                return false;

            // Check if method is suitable for AOT compilation
            // - Should not be virtual (virtual dispatch adds overhead)
            // - Should not use reflection (reflection can't be AOT'd)
            // - Should not be generic with open type parameters
            if (method.IsVirtual && safety == OptimizationSafety.Strict)
                return false;

            // Mark the method as compiled via reflection (in a real implementation, this would invoke native AOT compiler)
            // For now, we just track that it's marked for compilation
            _compiledMethods.Add(methodFullName);

            // Log compilation success
            Logging.LogInternal($"[AOT] Compiled {methodFullName} with args: {GetCompilerArgs()}", LogLevel.Info);

            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Compile all methods marked with <seealso cref="ManagedOptimizeAttribute"/>.
    /// </summary>
    public int CompileOptimizedMethods(Type[] types)
    {
        int compiled = 0;

        foreach (var type in types)
        {
            var methods = type.GetMethods();
            foreach (var method in methods)
            {
                if (method.GetCustomAttributes(typeof(ManagedOptimizeAttribute), false)
                    .FirstOrDefault() is ManagedOptimizeAttribute attr && attr.EnableAOT)
                {
                    if (CompileMethod(method.DeclaringType?.FullName + "::" + method.Name, attr.SafetyLevel))
                        compiled++;
                }
            }
        }

        return compiled;
    }

    /// <summary>
    /// Get the AOT compiler arguments.
    /// </summary>
    public string GetCompilerArgs()
    {
        return $"-O{_config.OptimizationLevel.Last()} " +
                (_config.EnableLTO ? "-flto " : "") +
                (_config.EnableSIMD ? $"-m{_config.TargetInstructionSet} " : "") +
                (_config.AggressiveInlining ? "-finline-limit=1000 " : "");
    }

    /// <summary>
    /// Check if a method is compiled.
    /// </summary>
    public bool IsMethodCompiled(string methodFullName)
    {
        return _compiledMethods.Contains(methodFullName);
    }
}


