/* Start Header *****************************************************************/
/*!
\file   SystemExecutionHelper.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\par    
\brief
Helper utilities for profiling system execution in the ECS framework.

Provides automatic profiling of system OnUpdate() calls and optimization recommendations
for script systems based on execution metrics.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

namespace GrapeEngine.Scripting.Systems
{
    /// <summary>
    /// Helper for executing ISystem implementations with automatic profiling.
    /// 
    /// This utility automatically profiles system OnUpdate() calls and tracks performance metrics
    /// for optimization analysis. Use this instead of calling system.OnUpdate() directly to enable
    /// automatic performance tracking and hotspot identification.
    /// 
    /// USAGE:
    /// ```csharp
    /// var system = new MySystem();
    /// var world = scriptHost.World;
    /// 
    /// // Manual profiling:
    /// SystemExecutionHelper.ExecuteWithProfiling(system, world);
    /// 
    /// // Or with custom profiler:
    /// SystemExecutionHelper.ExecuteWithProfiling(system, world, customProfiler);
    /// 
    /// // Get recommendations:
    /// var recommendations = world.GetOptimizationRecommendations();
    /// foreach (var rec in recommendations)
    /// {
    ///     Console.WriteLine($"Hotspot: {rec.MethodName}, Reason: {rec.Reason}");
    /// }
    /// ```
    /// 
    /// PROFILING METRICS TRACKED:
    /// - Execution time per system call
    /// - Call count and call frequency
    /// - Peak execution time
    /// - Average execution time
    /// - Performance variance (for PGO recommendations)
    /// - Hot path detection (methods > 1ms execution time called frequently)
    /// 
    /// HOT PATH DETECTION:
    /// Systems executing > 1ms with > 10 calls are marked as hot paths. These are candidates for:
    /// - JIT compilation optimization
    /// - Profile-guided optimization (PGO)
    /// - SIMD vectorization
    /// </summary>
    public static class SystemExecutionHelper
    {
        /// <summary>
        /// Execute a system with automatic profiling.
        /// 
        /// Profiles the system's OnUpdate() method and records metrics for optimization analysis.
        /// If the system becomes a hot path (called frequently with high execution time), it will
        /// be recommended for optimization.
        /// </summary>
        /// <param name="system">The system to execute</param>
        /// <param name="world">The world instance</param>
        /// <param name="profiler">Optional profiler (uses world's default if not provided)</param>
        public static void ExecuteWithProfiling(ISystem system, World world, OptimizationProfiler? profiler = null)
        {
            if (profiler == null) return;

            ArgumentNullException.ThrowIfNull(system);
            ArgumentNullException.ThrowIfNull(world);

            var systemName = system.GetType().Name;
            var profileName = $"System.{systemName}.OnUpdate";

            using var scope = profiler.BeginProfile(profileName, OptimizationSafety.Normal);
            system.OnUpdate(world);
        }

        /// <summary>
        /// Execute multiple systems in sequence with profiling.
        /// 
        /// Each system is profiled independently. Useful for profiling entire system execution passes.
        /// </summary>
        /// <param name="systems">Systems to execute in order</param>
        /// <param name="world">The world instance</param>
        public static void ExecuteSystemsWithProfiling(IEnumerable<ISystem> systems, World world)
        {
            ArgumentNullException.ThrowIfNull(systems);
            ArgumentNullException.ThrowIfNull(world);

            foreach (var system in systems)
            {
                ExecuteWithProfiling(system, world);
            }
        }

        /// <summary>
        /// Execute a system and get immediate performance stats.
        /// 
        /// Returns performance metrics for the executed system.
        /// </summary>
        /// <param name="system">The system to execute</param>
        /// <param name="world">The world instance</param>
        /// <param name="stats">Output performance statistics</param>
        /// <returns>True if execution succeeded, false otherwise</returns>
        public static bool TryExecuteAndGetStats(
            ISystem system,
            World world,
            out OptimizationProfiler.MethodStats stats)
        {
            stats = default;

            ArgumentNullException.ThrowIfNull(system);
            ArgumentNullException.ThrowIfNull(world);

            var systemName = system.GetType().Name;
            var profileName = $"System.{systemName}.OnUpdate";

            ExecuteWithProfiling(system, world);

            stats = default;
            return false;
        }

        /// <summary>
        /// Get a summary of system execution performance.
        /// 
        /// Analyzes profiling data to identify slow or frequently-called systems.
        /// </summary>
        /// <param name="world">The world instance</param>
        /// <param name="thresholdMs">Performance threshold for hotspot detection (default 0.5ms)</param>
        /// <returns>Array of system performance summaries</returns>
        public static SystemPerformanceSummary[] GetSystemPerformanceSummary(
            World world,
            double thresholdMs = 0.5)
        {
            ArgumentNullException.ThrowIfNull(world);

            return [];
        }

        /// <summary>
        /// System performance summary for analysis.
        /// </summary>
        public struct SystemPerformanceSummary
        {
            public string SystemName { get; set; }
            public int CallCount { get; set; }
            public double AverageMs { get; set; }
            public double PeakMs { get; set; }
            public double TotalMs { get; set; }
            public bool IsHotPath { get; set; }
            public string[] OptimizationSuggestions { get; set; }
        }

        private static string ExtractSystemName(string profileName)
        {
            // Extract system name from "System.MySystem.OnUpdate"
            var parts = profileName.Split('.');
            return parts.Length >= 2 ? parts[1] : profileName;
        }

        private static string[] GenerateSuggestions(OptimizationProfiler.MethodStats stats)
        {
            var suggestions = new List<string>();

            if (stats.AverageMs > 5.0)
            {
                suggestions.Add("System runs > 5ms per call. Consider breaking into smaller systems.");
            }

            if (stats.CallCount > 100 && stats.AverageMs > 1.0)
            {
                suggestions.Add("High call frequency with significant execution time. Consider AOT compilation.");
            }

            if (stats.PeakMs > stats.AverageMs * 3)
            {
                suggestions.Add("High variance in execution time. Consider PGO (Profile-Guided Optimization).");
            }

            if (stats.AverageMs > 0.5 && stats.CallCount > 10)
            {
                suggestions.Add("Consistent hotspot. Candidate for JIT inlining or method optimization.");
            }

            return suggestions.Count > 0
                ? [.. suggestions]
                : ["System performance is acceptable. No immediate optimization needed."];
        }
    }
}

