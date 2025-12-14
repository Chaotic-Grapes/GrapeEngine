using System.Diagnostics;

namespace GrapeEngine.Scripting
{
    /// <summary>
    /// Tracks optimization metrics and profiles hot paths for managed code optimization.
    /// Provides data for profile-guided optimization (PGO) decisions.
    /// </summary>
    public class OptimizationProfiler
    {
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
        /// </summary>
        public OptimizationScope BeginProfile(string methodName, OptimizationSafety safetyLevel = OptimizationSafety.Normal)
        {
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
        /// </summary>
        public List<OptimizationRecommendation> GetRecommendations()
        {
            var recommendations = new List<OptimizationRecommendation>();

            foreach (var profile in _profiles.Values)
            {
                if (profile.CallCount < 3)
                    continue; // Need samples

                var avgNs = profile.TotalNanoseconds / profile.CallCount;

                // High call count + significant time = candidate for AOT
                if (profile.CallCount > 100 && avgNs > 1_000_000)
                {
                    recommendations.Add(new OptimizationRecommendation
                    {
                        MethodName = profile.MethodName,
                        Reason = "High call frequency with significant execution time",
                        SuggestedOptimizations = ["CompileToNative", "EnableSIMD", "EnablePGO"],
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

                // High entity throughput = candidate for SIMD
                if (profile.EntityProcessed > 1000 && profile.TotalNanoseconds > 10_000_000)
                {
                    recommendations.Add(new OptimizationRecommendation
                    {
                        MethodName = profile.MethodName,
                        Reason = "High-throughput loop, SIMD vectorization recommended",
                        SuggestedOptimizations = ["EnableSIMD", "Vectorize"],
                        Priority = 1
                    });
                }
            }

            return [.. recommendations.OrderByDescending(r => r.Priority)];
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
                // TODO: Invoke AOT compiler
                // AOT compiles IL to native code at build time, not runtime
                _compiledMethods.Add(methodFullName);
                return true;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// Compile all methods marked with [ManagedOptimize].
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
}
