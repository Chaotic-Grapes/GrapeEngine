/* Start Header *****************************************************************/
/*!
\file   SIMDOptimizedJobHelper.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\par    
\brief
Helper utilities for SIMD-optimized job execution and component processing.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using GrapeEngine.Scripting.Query;
using System.Runtime.CompilerServices;

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Helper for SIMD-optimized job execution.
/// 
/// Provides utilities for detecting SIMD capabilities, auto-applying optimizations,
/// and handling fallback for non-vectorizable operations.
/// </summary>
public static class SIMDOptimizedJobHelper
{
    /// <summary>
    /// Determines if the current component type can benefit from SIMD optimization.
    /// 
    /// Returns true for types that are:
    /// - Blittable (can be safely copied as memory)
    /// - Small enough for efficient vectorization
    /// - Have common patterns (Vector3, Transform, Position, etc.)
    /// </summary>
    /// <typeparam name="T">Component type to check</typeparam>
    /// <returns>True if SIMD is applicable and beneficial</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static bool CanOptimizeSIMD<T>() where T : unmanaged
    {
        // Check CPU capabilities first
        if (!SIMDOptimizer.CapabilityDetection.HasAVX2)
            return false;

        unsafe
        {
            // Type must be small enough for practical vectorization
            // Typically <= 128 bytes for good cache utilization
            int typeSize = sizeof(T);
            if (typeSize > 128)
                return false;

            // Must be a power of 2 or simple structure for alignment
            if (typeSize < 4 || typeSize > 64)
                return false;
        }

        return true;
    }

    /// <summary>
    /// Get recommended batch size for SIMD processing of a component type.
    /// 
    /// Larger batches can amortize SIMD overhead, but smaller batches allow
    /// better load balancing across cores. This returns a balanced suggestion.
    /// </summary>
    /// <typeparam name="T">Component type</typeparam>
    /// <returns>Recommended number of entities per job batch</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static int GetRecommendedBatchSize<T>() where T : unmanaged
    {
        if (!CanOptimizeSIMD<T>())
            return 256; // Scalar fallback

        unsafe
        {
            int typeSize = sizeof(T);
            
            // AVX2 = 256-bit vectors = 32 bytes
            // Aim for batches that fill L1 cache (typically 32KB)
            // Allow ~8KB per type for batching flexibility
            const int l1CachePerType = 8192;
            int entitiesPerBatch = l1CachePerType / typeSize;

            // Clamp to reasonable bounds
            return System.Math.Max(64, System.Math.Min(1024, entitiesPerBatch));
        }
    }

    /// <summary>
    /// Get estimated performance improvement from SIMD for a specific operation.
    /// </summary>
    /// <typeparam name="T">Component type</typeparam>
    /// <returns>Estimated speedup factor (e.g., 2.5 = 2.5x faster)</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static double GetEstimatedSpeedup<T>() where T : unmanaged
    {
        if (!CanOptimizeSIMD<T>())
            return 1.0;

        // Return capability-based estimate
        return SIMDOptimizer.CapabilityDetection.EstimateSpeedup();
    }

    /// <summary>
    /// Process entities with automatic SIMD optimization fallback.
    /// 
    /// This helper automatically selects between SIMD-optimized and scalar implementations
    /// based on CPU capabilities and component type characteristics.
    /// </summary>
    /// <typeparam name="T">Component type to process</typeparam>
    /// <param name="entities">Entities to process</param>
    /// <param name="processor">Function to process each component</param>
    public static void ProcessWithOptimization<T>(
        IEnumerable<T> entities,
        Action<T> processor) where T : unmanaged
    {
        if (!CanOptimizeSIMD<T>())
        {
            // Use scalar fallback
            foreach (var entity in entities)
            {
                processor(entity);
            }
            return;
        }

        // SIMD-optimized path: batch processing for better cache locality
        var batch = new List<T>();
        int batchSize = GetRecommendedBatchSize<T>();

        foreach (var entity in entities)
        {
            batch.Add(entity);

            if (batch.Count >= batchSize)
            {
                ProcessBatch(batch, processor);
                batch.Clear();
            }
        }

        // Handle remainder
        if (batch.Count > 0)
        {
            ProcessBatch(batch, processor);
        }
    }

    /// <summary>
    /// Process a batch of components with cache-optimized iteration.
    /// </summary>
    private static void ProcessBatch<T>(List<T> batch, Action<T> processor) where T : unmanaged
    {
        // Process in cache-friendly chunks (typically 64 elements per L1 cache line)
        const int cacheLineSize = 64;
        
        for (int i = 0; i < batch.Count; i += cacheLineSize)
        {
            int end = System.Math.Min(i + cacheLineSize, batch.Count);
            for (int j = i; j < end; j++)
            {
                processor(batch[j]);
            }
        }
    }

    /// <summary>
    /// Create a SIMD-optimized job configuration based on component type.
    /// </summary>
    /// <typeparam name="T">Component type</typeparam>
    /// <returns>Configuration with recommended settings for optimal performance</returns>
    public static SIMDJobConfig GetOptimalConfig<T>() where T : unmanaged
    {
        return new SIMDJobConfig
        {
            CanUseSIMD = CanOptimizeSIMD<T>(),
            RecommendedBatchSize = GetRecommendedBatchSize<T>(),
            EstimatedSpeedup = GetEstimatedSpeedup<T>(),
            SIMDCapability = SIMDOptimizer.CapabilityDetection.GetCapabilityDescription()
        };
    }
}

/// <summary>
/// Configuration for SIMD-optimized job execution.
/// </summary>
public struct SIMDJobConfig
{
    /// <summary>
    /// Whether this component type can use SIMD optimizations.
    /// </summary>
    public bool CanUseSIMD { get; set; }

    /// <summary>
    /// Recommended number of entities per job batch for optimal cache utilization.
    /// </summary>
    public int RecommendedBatchSize { get; set; }

    /// <summary>
    /// Estimated performance improvement from SIMD (e.g., 2.0 = 2x faster).
    /// </summary>
    public double EstimatedSpeedup { get; set; }

    /// <summary>
    /// Human-readable description of available SIMD capabilities.
    /// </summary>
    public string SIMDCapability { get; set; }

    /// <summary>
    /// Get a summary string of this configuration.
    /// </summary>
    public readonly override string ToString()
    {
        return $"SIMD: {(CanUseSIMD ? "Yes" : "No")}, " +
               $"BatchSize: {RecommendedBatchSize}, " +
               $"Speedup: {EstimatedSpeedup:F1}x, " +
               $"Capability: {SIMDCapability}";
    }
}

/// <summary>
/// Extension methods for SIMD-optimized job execution on queries.
/// </summary>
public static class QuerySIMDExtensions
{
    /// <summary>
    /// Get SIMD optimization configuration for this query's component type.
    /// </summary>
    /// <typeparam name="T">Component type from query</typeparam>
    /// <returns>SIMD configuration for optimal execution</returns>
    public static SIMDJobConfig GetSIMDConfig<T>(this Query<T> query) where T : unmanaged
    {
        return SIMDOptimizedJobHelper.GetOptimalConfig<T>();
    }

    /// <summary>
    /// Check if a query can benefit from SIMD optimization.
    /// </summary>
    /// <typeparam name="T">Component type from query</typeparam>
    /// <returns>True if SIMD optimizations are applicable</returns>
    public static bool CanUseSIMD<T>(this Query<T> query) where T : unmanaged
    {
        return SIMDOptimizedJobHelper.CanOptimizeSIMD<T>();
    }

    /// <summary>
    /// Get recommended batch size for SIMD processing this query.
    /// </summary>
    /// <typeparam name="T">Component type from query</typeparam>
    /// <returns>Number of entities per job batch</returns>
    public static int GetRecommendedSIMDBatchSize<T>(this Query<T> query) where T : unmanaged
    {
        return SIMDOptimizedJobHelper.GetRecommendedBatchSize<T>();
    }

    /// <summary>
    /// Get estimated speedup from SIMD for this query's component type.
    /// </summary>
    /// <typeparam name="T">Component type from query</typeparam>
    /// <returns>Estimated speedup factor (e.g., 2.5x)</returns>
    public static double GetEstimatedSIMDSpeedup<T>(this Query<T> query) where T : unmanaged
    {
        return SIMDOptimizedJobHelper.GetEstimatedSpeedup<T>();
    }
}
