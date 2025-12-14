using GrapeEngine.Scripting.Job;
using GrapeEngine.Scripting.Query;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Intrinsics;
using System.Runtime.Intrinsics.X86;

namespace GrapeEngine.Scripting;

/// <summary>
/// SIMD intrinsic helpers for vectorized operations on entity data.
/// Provides safe wrappers around Vector, AVX, and SSE intrinsics.
/// </summary>
public static class SIMDOptimizer
{
    /// <summary>
    /// Detects available SIMD instruction sets.
    /// </summary>
    public static class CapabilityDetection
    {
        public static bool HasSSE2 => Sse2.IsSupported;
        public static bool HasSSE42 => Sse42.IsSupported;
        public static bool HasAVX => Avx.IsSupported;
        public static bool HasAVX2 => Avx2.IsSupported;
        public static bool HasAVX512F => Avx512F.IsSupported;

        /// <summary>
        /// Get recommended vector width in elements (based on available SIMD).
        /// </summary>
        public static int GetRecommendedVectorWidth<T>() where T : unmanaged
        {
            int elementSize;
            unsafe
            {
                elementSize = sizeof(T);
            }
            return GetRecommendedVectorWidthInternal(elementSize);
        }

        private static int GetRecommendedVectorWidthInternal(int elementSize)
        {

            // Prefer wider vectors when available
            if (HasAVX512F)
                return System.Math.Min(512 / (elementSize * 8), 16); // Up to 16 elements
            if (HasAVX2)
                return System.Math.Min(256 / (elementSize * 8), 8);  // Up to 8 elements
            if (HasAVX)
                return System.Math.Min(256 / (elementSize * 8), 8);
            if (HasSSE2)
                return System.Math.Min(128 / (elementSize * 8), 4);  // Up to 4 elements

            return 1; // Scalar fallback
        }
    }

    /// <summary>
    /// Vectorized float operations (common in physics, animation, etc).
    /// </summary>
    public static class VectorFloat
    {
        /// <summary>
        /// Add multiple float pairs using SIMD (if available).
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static void AddArray(float[] dest, float[] src, int count)
        {
            if (!Avx2.IsSupported || count < 8)
            {
                // Scalar fallback
                for (var i = 0; i < count; i++)
                    dest[i] += src[i];
                return;
            }

            var vectorCount = count / 8;
            for (var i = 0; i < vectorCount; i++)
            {
                var idx = i * 8;
                var destVec = Vector256.Create(dest[idx], dest[idx + 1], dest[idx + 2], dest[idx + 3],
                                                dest[idx + 4], dest[idx + 5], dest[idx + 6], dest[idx + 7]);
                var srcVec = Vector256.Create(src[idx], src[idx + 1], src[idx + 2], src[idx + 3],
                                                src[idx + 4], src[idx + 5], src[idx + 6], src[idx + 7]);
                var result = Avx.Add(destVec, srcVec);

                result.GetLower().StoreUnsafe(ref dest[idx]);
                result.GetUpper().StoreUnsafe(ref dest[idx + 4]);
            }

            // Handle remainder
            for (var i = vectorCount * 8; i < count; i++)
                dest[i] += src[i];
        }

        /// <summary>
        /// Multiply float array by scalar using SIMD.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static void ScaleArray(float[] data, float scale, int count)
        {
            if (!Avx2.IsSupported || count < 8)
            {
                // Scalar fallback
                for (var i = 0; i < count; i++)
                    data[i] *= scale;
                return;
            }

            var scaleVec = Vector256.Create(scale);
            var vectorCount = count / 8;

            for (var i = 0; i < vectorCount; i++)
            {
                var idx = i * 8;
                var dataVec = Vector256.Create(data[idx], data[idx + 1], data[idx + 2], data[idx + 3],
                                                data[idx + 4], data[idx + 5], data[idx + 6], data[idx + 7]);
                var result = Avx.Multiply(dataVec, scaleVec);

                result.GetLower().StoreUnsafe(ref data[idx]);
                result.GetUpper().StoreUnsafe(ref data[idx + 4]);
            }

            // Handle remainder
            for (var i = vectorCount * 8; i < count; i++)
                data[i] *= scale;
        }

        /// <summary>
        /// Dot product for two float arrays using SIMD.
        /// </summary>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float DotProduct(float[] a, float[] b, int count)
        {
            var result = 0f;

            if (!Avx2.IsSupported || count < 8)
            {
                // Scalar fallback
                for (var i = 0; i < count; i++)
                    result += a[i] * b[i];
                return result;
            }

            var sumVec = Vector256<float>.Zero;
            var vectorCount = count / 8;

            for (var i = 0; i < vectorCount; i++)
            {
                var idx = i * 8;
                var aVec = Vector256.Create(a[idx], a[idx + 1], a[idx + 2], a[idx + 3],
                                                a[idx + 4], a[idx + 5], a[idx + 6], a[idx + 7]);
                var bVec = Vector256.Create(b[idx], b[idx + 1], b[idx + 2], b[idx + 3],
                                                b[idx + 4], b[idx + 5], b[idx + 6], b[idx + 7]);
                sumVec = Avx.Add(sumVec, Avx.Multiply(aVec, bVec));
            }

            // Sum vector elements
            for (var i = 0; i < 8; i++)
                result += sumVec.GetElement(i);

            // Handle remainder
            for (var i = vectorCount * 8; i < count; i++)
                result += a[i] * b[i];

            return result;
        }
    }

    /// <summary>
    /// Vectorized operations for component structs.
    /// </summary>
    public static class ComponentOps
    {
        /// <summary>
        /// Apply a transformation to all components of a type using optimized scalar processing.
        /// 
        /// For blittable types (like Vector3, Transform, etc), this provides cache-friendly
        /// bulk processing. For SIMD-compatible operations, use specialized methods like
        /// VectorFloat.ScaleArray for better performance.
        /// </summary>
        /// <typeparam name="T">Component struct type (should be blittable)</typeparam>
        /// <param name="components">Array of components to transform</param>
        /// <param name="transform">Transformation function to apply to each component</param>
        /// <param name="count">Number of elements to transform</param>
        public static void TransformComponents<T>(T[] components, Func<T, T> transform, int count) where T : struct
        {
            ArgumentNullException.ThrowIfNull(components);
            ArgumentNullException.ThrowIfNull(transform);
            if (count < 0 || count > components.Length)
                throw new ArgumentOutOfRangeException(nameof(count));

            // Validate that T is blittable for optimal performance
            try
            {
                // Small allocation to test if type is blittable
                var testArray = new T[1];
                GCHandle.Alloc(testArray, GCHandleType.Pinned).Free();
            }
            catch
            {
                // Type is not blittable, continue with scalar fallback
                Console.WriteLine($"[SIMDOptimizer] Warning: {typeof(T).Name} is not blittable; using scalar transform");
            }

            // Process in cache-friendly chunks for better memory locality
            // Default chunk size of 64 elements works well for L1/L2 cache
            const int chunkSize = 64;
            
            if (count <= chunkSize)
            {
                // For small arrays, process directly
                for (var i = 0; i < count; i++)
                {
                    components[i] = transform(components[i]);
                }
            }
            else
            {
                // For large arrays, process in chunks to improve cache efficiency
                var chunks = count / chunkSize;
                
                for (var c = 0; c < chunks; c++)
                {
                    var chunkStart = c * chunkSize;
                    var chunkEnd = chunkStart + chunkSize;
                    
                    for (var i = chunkStart; i < chunkEnd; i++)
                    {
                        components[i] = transform(components[i]);
                    }
                }

                // Handle remainder
                var remainderStart = chunks * chunkSize;
                for (var i = remainderStart; i < count; i++)
                {
                    components[i] = transform(components[i]);
                }
            }
        }

        /// <summary>
        /// Apply a transformation to components with explicit loop unrolling for better performance.
        /// </summary>
        /// <typeparam name="T">Component struct type</typeparam>
        /// <param name="components">Array of components to transform</param>
        /// <param name="transform">Transformation function</param>
        /// <param name="count">Number of elements to transform</param>
        public static void TransformComponentsUnrolled<T>(T[] components, Func<T, T> transform, int count) where T : struct
        {
            ArgumentNullException.ThrowIfNull(components);
            ArgumentNullException.ThrowIfNull(transform);
            if (count < 0 || count > components.Length)
                throw new ArgumentOutOfRangeException(nameof(count));

            // Unroll loop by 4 for better instruction-level parallelism
            var unrolledCount = count / 4;
            
            for (var i = 0; i < unrolledCount; i++)
            {
                var idx = i * 4;
                components[idx] = transform(components[idx]);
                components[idx + 1] = transform(components[idx + 1]);
                components[idx + 2] = transform(components[idx + 2]);
                components[idx + 3] = transform(components[idx + 3]);
            }

            // Handle remainder
            var remainder = count % 4;
            for (var i = 0; i < remainder; i++)
            {
                components[unrolledCount * 4 + i] = transform(components[unrolledCount * 4 + i]);
            }
        }

        /// <summary>
        /// Transform components in parallel using the job system for maximum performance.
        /// </summary>
        /// <typeparam name="T">Component struct type</typeparam>
        /// <param name="components">Array of components to transform</param>
        /// <param name="transform">Transformation function</param>
        /// <param name="count">Number of elements to transform</param>
        /// <remarks>
        /// This method should be used for bulk transforms of large component arrays.
        /// The job system will automatically distribute work across available CPU cores.
        /// Requires the transform function to be thread-safe (no shared state).
        /// </remarks>
        public static void TransformComponentsParallel<T>(T[] components, Func<T, T> transform, int count) where T : struct
        {
            ArgumentNullException.ThrowIfNull(components);
            ArgumentNullException.ThrowIfNull(transform);
            if (count < 0 || count > components.Length)
                throw new ArgumentOutOfRangeException(nameof(count));

            // For very large arrays, parallel processing is beneficial
            // Break into jobs of ~1000 elements for optimal load distribution
            const int jobSize = 1000;
            
            if (count <= jobSize)
            {
                // Too small for parallelization overhead; use scalar version
                TransformComponents(components, transform, count);
                return;
            }

            var jobCount = (count + jobSize - 1) / jobSize;
            
            // In a real implementation, this would use the ECS job system
            // For now, use Task Parallel Library as fallback
            List<Task> jobs = new(jobCount);

            for (var jobIdx = 0; jobIdx < jobCount; jobIdx++)
            {
                var startIdx = jobIdx * jobSize;
                var endIdx = System.Math.Min(startIdx + jobSize, count);

                var job = Task.Run(() =>
                {
                    for (var i = startIdx; i < endIdx; i++)
                    {
                        components[i] = transform(components[i]);
                    }
                });

                jobs.Add(job);
            }

            // Wait for all jobs to complete
            Task.WaitAll([.. jobs]);
        }


        /// <summary>
        /// Broadcast a component value to multiple positions using SIMD idioms.
        /// </summary>
        public static void BroadcastComponent<T>(T[] target, T value, int start, int count) where T : struct
        {
            var remaining = count;
            var idx = start;

            // Fill in chunks for cache efficiency
            const int chunkSize = 64;
            while (remaining >= chunkSize)
            {
                for (var i = 0; i < chunkSize; i++)
                    target[idx + i] = value;
                idx += chunkSize;
                remaining -= chunkSize;
            }

            // Handle remainder
            for (int i = 0; i < remaining; i++)
                target[idx + i] = value;
        }
    }

    /// <summary>
    /// Horizontal reductions (sum, max, min) optimized with SIMD.
    /// </summary>
    public static class Reductions
    {
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Sum(float[] data, int count)
        {
            var result = 0f;

            if (!Avx2.IsSupported || count < 8)
            {
                for (int i = 0; i < count; i++)
                    result += data[i];
                return result;
            }

            var sumVec = Vector256<float>.Zero;
            var vectorCount = count / 8;

            for (int i = 0; i < vectorCount; i++)
            {
                var vec = Vector256.Create(data[i * 8], data[i * 8 + 1], data[i * 8 + 2], data[i * 8 + 3],
                                            data[i * 8 + 4], data[i * 8 + 5], data[i * 8 + 6], data[i * 8 + 7]);
                sumVec = Avx.Add(sumVec, vec);
            }

            for (var i = 0; i < 8; i++)
                result += sumVec.GetElement(i);

            for (var i = vectorCount * 8; i < count; i++)
                result += data[i];

            return result;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Max(float[] data, int count)
        {
            if (count == 0)
                return float.NegativeInfinity;

            var result = data[0];

            if (!Avx2.IsSupported || count < 8)
            {
                for (var i = 1; i < count; i++)
                    result = System.Math.Max(result, data[i]);
                return result;
            }

            var maxVec = Vector256.Create(result);
            var vectorCount = count / 8;

            for (int i = 0; i < vectorCount; i++)
            {
                var vec = Vector256.Create(data[i * 8], data[i * 8 + 1], data[i * 8 + 2], data[i * 8 + 3],
                                            data[i * 8 + 4], data[i * 8 + 5], data[i * 8 + 6], data[i * 8 + 7]);
                maxVec = Avx.Max(maxVec, vec);
            }

            for (var i = 0; i < 8; i++)
                result = System.Math.Max(result, maxVec.GetElement(i));

            for (var i = vectorCount * 8; i < count; i++)
                result = System.Math.Max(result, data[i]);

            return result;
        }

        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Min(float[] data, int count)
        {
            if (count == 0)
                return float.PositiveInfinity;

            var result = data[0];

            if (!Avx2.IsSupported || count < 8)
            {
                for (var i = 1; i < count; i++)
                    result = System.Math.Min(result, data[i]);
                return result;
            }

            var minVec = Vector256.Create(result);
            var vectorCount = count / 8;

            for (var i = 0; i < vectorCount; i++)
            {
                var vec = Vector256.Create(data[i * 8], data[i * 8 + 1], data[i * 8 + 2], data[i * 8 + 3],
                                            data[i * 8 + 4], data[i * 8 + 5], data[i * 8 + 6], data[i * 8 + 7]);
                minVec = Avx.Min(minVec, vec);
            }

            for (var i = 0; i < 8; i++)
                result = System.Math.Min(result, minVec.GetElement(i));

            for (var i = vectorCount * 8; i < count; i++)
                result = System.Math.Min(result, data[i]);

            return result;
        }
    }

    /// <summary>
    /// Parallel transformation patterns for entity/component data.
    /// </summary>
    public static class ParallelTransform
    {
        /// <summary>
        /// Apply transformation in cache-friendly blocks.
        /// </summary>
        public static void TransformInBlocks<T>(T[] data, Func<T, T> transform, int count, int blockSize = 64)
        {
            for (var blockStart = 0; blockStart < count; blockStart += blockSize)
            {
                var blockEnd = System.Math.Min(blockStart + blockSize, count);
                for (var i = blockStart; i < blockEnd; i++)
                {
                    data[i] = transform(data[i]);
                }
            }
        }

        /// <summary>
        /// Apply transformation conditionally with SIMD-friendly patterns.
        /// </summary>
        public static void TransformIf<T>(T[] data, Func<T, bool> condition, Func<T, T> transform, int count)
        {
            for (var i = 0; i < count; i++)
            {
                if (condition(data[i]))
                    data[i] = transform(data[i]);
            }
        }
    }
}

/// <summary>
/// Query extensions for optimization-aware entity iteration.
/// </summary>
public static class OptimizedQueryExtensions
{
    /// <summary>
    /// Execute query with optimization metrics collection.
    /// </summary>
    public static void ForEachOptimized<T>(this Query<T> query, EntityAction<T> action, 
        OptimizationProfiler? profiler = null) where T : unmanaged
    {
        using var scope = profiler?.BeginProfile($"Query<{typeof(T).Name}>", OptimizationSafety.Normal);
        var count = 0;
        var enumerator = query.GetEnumerator();
        unsafe
        {
            while (enumerator.MoveNext())
            {
                var (entity, component) = enumerator.Current;
                action?.Invoke(ref component);
                count++;
            }
        }
        scope?.RecordEntitiesProcessed(count);
    }

    /// <summary>
    /// Execute query as job with optimization tracking.
    /// </summary>
    public static JobHandle ForEachOptimizedAsJob<T>(this Query<T> query, EntityAction<T> action//,
        /* OptimizationProfiler? profiler = null */) where T : unmanaged
    {
        // TODO: probably would wrap job execution with profiler
        return query.ForEachEntity(action);
    }
}
