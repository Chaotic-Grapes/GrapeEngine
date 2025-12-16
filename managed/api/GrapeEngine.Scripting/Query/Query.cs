/* Start Header *****************************************************************/
/*!
\file   Query.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Query API for iterating over entities with specific components.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Collections;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Unsafe;

namespace GrapeEngine.Scripting.Query;

/// <summary>
/// QUERY SYSTEM - Type-safe entity iteration with component filtering.
/// 
/// The Query system provides the primary mechanism for iterating over entities based on
/// component composition. It supports efficient bulk operations on entities with specific
/// component types.
/// 
/// THREAD SAFETY:
/// - All query operations must be called from the main thread
/// - Query iterators are NOT reentrant-safe - do not store iterators across iterations
/// - Do NOT call query methods from multiple threads simultaneously
/// - For parallelized iteration, use the Job system (IJob) with WorldAPI directly
/// 
/// MAXIMUM COMPONENTS SUPPORTED:
/// This implementation supports up to 8 components per single query (Query<T1> through Query<T1..T8>).
/// For queries requiring more than 8 components, break them into multiple smaller queries or
/// restructure your component organization.
/// 
/// PERFORMANCE CHARACTERISTICS:
/// - Query construction: O(1) - just stores component hashes
/// - GetEnumerator(): O(1) - minimal allocation, native side does heavy lifting
/// - MoveNext() per entity: O(1) amortized - direct native API call
/// - Count(): O(n) where n = matching entities - iterates all results
/// - Any(): O(1) best case (first entity) to O(n) worst case (no entities match)
/// 
/// ALLOCATION BEHAVIOR:
/// - QueryFilterBuilder.Optional/WithAll/Without: Accumulates in Lists, deallocated on GetEnumerator()
/// - No per-entity allocations during iteration
/// - Iterator state is stack-based (QueryIterator struct)
/// 
/// FILTERING API (Fluent Chain):
/// - Without<T>(): Exclude entities with component T
/// - Optional<T>(): Include component T if present, but don't exclude if absent
/// - WithAll<T...>(): Require component T (up to 2 chained calls)
/// 
/// EXAMPLE USAGE:
/// ```csharp
/// // Basic iteration
/// foreach (var result in world.Query<LocalTransform>())
/// {
///     result.Component.Position += velocity * deltaTime;
/// }
/// 
/// // With filtering
/// var filtered = world.Query<LocalTransform>()
///     .Without<Frozen>()
///     .WithAll<Rigidbody>();
/// foreach (var result in filtered)
/// {
///     // Only active, unfrozen entities with physics bodies
/// }
/// 
/// // Convenience helpers
/// int count = world.Query<LocalTransform>().Count();
/// bool hasAny = world.Query<LocalTransform>().Any();
/// ```
/// 
/// IMPLEMENTATION NOTES:
/// - Component hashing uses FNV-1a algorithm (matches C++ side)
/// - QueryEnumerator interops with C++ engine for actual entity iteration
/// - Iterator state is stored in QueryIterator struct (unmanaged)
/// - All component types must be unmanaged structs with sequential layout
/// </summary>
public class Query<T1>
    where T1 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>()
        };
    }

    public QueryEnumerator<T1> GetEnumerator()
    {
        return new QueryEnumerator<T1>(_world, _componentHashes);
    }

    /// <summary>
    /// Get an enumerator for this query with automatic profiling.
    /// Use this for profiling-enabled query execution that tracks performance metrics.
    /// </summary>
    /// <param name="profiler">The optimization profiler to use (defaults to world's profiler)</param>
    /// <param name="queryName">Name for profiling (defaults to component type names)</param>
    /// <returns>A profiling-enabled query result</returns>
    public ProfiledQueryIterator<T1> GetEnumeratorWithProfiling(
        OptimizationProfiler? profiler = null,
        string? queryName = null)
    {
        profiler ??= _world.OptimizationProfiler;
        queryName ??= $"Query<{typeof(T1).Name}>";
        
        return new ProfiledQueryIterator<T1>(_world, _componentHashes, profiler, queryName);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    /// <summary>
    /// Get the count of matching entities with profiling.
    /// </summary>
    public int CountWithProfiling(OptimizationProfiler? profiler = null)
    {
        profiler ??= _world.OptimizationProfiler;
        using (var scope = profiler.BeginProfile(
            $"Query<{typeof(T1).Name}>.Count",
            OptimizationSafety.Normal))
        {
            return Count();
        }
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1> GetEnumerator()
        {
            return new QueryEnumerator<T1>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}

/// <summary>
/// Query for iterating over entities with two component types.
/// </summary>
public class Query<T1, T2>
    where T1 : unmanaged
    where T2 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>()
        };
    }

    public QueryEnumerator<T1, T2> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2>(_world, _componentHashes);
    }

    /// <summary>
    /// Get an enumerator for this query with automatic profiling.
    /// Use this for profiling-enabled query execution that tracks performance metrics.
    /// </summary>
    /// <param name="profiler">The optimization profiler to use (defaults to world's profiler)</param>
    /// <param name="queryName">Name for profiling (defaults to component type names)</param>
    /// <returns>A profiling-enabled query result</returns>
    public ProfiledQueryIterator<T1, T2> GetEnumeratorWithProfiling(
        OptimizationProfiler? profiler = null,
        string? queryName = null)
    {
        profiler ??= _world.OptimizationProfiler;
        queryName ??= $"Query<{typeof(T1).Name},{typeof(T2).Name}>";
        
        return new ProfiledQueryIterator<T1, T2>(_world, _componentHashes, profiler, queryName);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    /// <summary>
    /// Get the count of matching entities with profiling.
    /// </summary>
    public int CountWithProfiling(OptimizationProfiler? profiler = null)
    {
        profiler ??= _world.OptimizationProfiler;
        using (var scope = profiler.BeginProfile(
            $"Query<{typeof(T1).Name},{typeof(T2).Name}>.Count",
            OptimizationSafety.Normal))
        {
            return Count();
        }
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1, T2> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}

/// <summary>
/// Query for iterating over entities with three component types.
/// </summary>
public class Query<T1, T2, T3>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        ComponentRegistry.EnsureRegistered<T3>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>()
        };
    }

    public QueryEnumerator<T1, T2, T3> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2, T3>(_world, _componentHashes);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1, T2, T3> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2, T3>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}

// Add Query<T1, T2, T3, T4> and more as needed...

/// <summary>
/// Query for iterating over entities with four component types.
/// </summary>
public class Query<T1, T2, T3, T4>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        ComponentRegistry.EnsureRegistered<T3>();
        ComponentRegistry.EnsureRegistered<T4>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>(),
            ComponentTypeHelper.GetTypeHash<T4>()
        };
    }

    public QueryEnumerator<T1, T2, T3, T4> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2, T3, T4>(_world, _componentHashes);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1, T2, T3, T4> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2, T3, T4>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}

/// <summary>
/// Query for iterating over entities with five component types.
/// </summary>
public class Query<T1, T2, T3, T4, T5>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        ComponentRegistry.EnsureRegistered<T3>();
        ComponentRegistry.EnsureRegistered<T4>();
        ComponentRegistry.EnsureRegistered<T5>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>(),
            ComponentTypeHelper.GetTypeHash<T4>(),
            ComponentTypeHelper.GetTypeHash<T5>()
        };
    }

    public QueryEnumerator<T1, T2, T3, T4, T5> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2, T3, T4, T5>(_world, _componentHashes);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1, T2, T3, T4, T5> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2, T3, T4, T5>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}

/// <summary>
/// Query for iterating over entities with six component types.
/// </summary>
public class Query<T1, T2, T3, T4, T5, T6>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        ComponentRegistry.EnsureRegistered<T3>();
        ComponentRegistry.EnsureRegistered<T4>();
        ComponentRegistry.EnsureRegistered<T5>();
        ComponentRegistry.EnsureRegistered<T6>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>(),
            ComponentTypeHelper.GetTypeHash<T4>(),
            ComponentTypeHelper.GetTypeHash<T5>(),
            ComponentTypeHelper.GetTypeHash<T6>()
        };
    }

    public QueryEnumerator<T1, T2, T3, T4, T5, T6> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2, T3, T4, T5, T6>(_world, _componentHashes);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1, T2, T3, T4, T5, T6> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2, T3, T4, T5, T6>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}

/// <summary>
/// Query for iterating over entities with seven component types.
/// </summary>
public class Query<T1, T2, T3, T4, T5, T6, T7>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
    where T7 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        ComponentRegistry.EnsureRegistered<T3>();
        ComponentRegistry.EnsureRegistered<T4>();
        ComponentRegistry.EnsureRegistered<T5>();
        ComponentRegistry.EnsureRegistered<T6>();
        ComponentRegistry.EnsureRegistered<T7>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>(),
            ComponentTypeHelper.GetTypeHash<T4>(),
            ComponentTypeHelper.GetTypeHash<T5>(),
            ComponentTypeHelper.GetTypeHash<T6>(),
            ComponentTypeHelper.GetTypeHash<T7>()
        };
    }

    public QueryEnumerator<T1, T2, T3, T4, T5, T6, T7> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2, T3, T4, T5, T6, T7>(_world, _componentHashes);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1, T2, T3, T4, T5, T6, T7> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2, T3, T4, T5, T6, T7>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}

/// <summary>
/// Query for iterating over entities with eight component types.
/// </summary>
public class Query<T1, T2, T3, T4, T5, T6, T7, T8>
    where T1 : unmanaged
    where T2 : unmanaged
    where T3 : unmanaged
    where T4 : unmanaged
    where T5 : unmanaged
    where T6 : unmanaged
    where T7 : unmanaged
    where T8 : unmanaged
{
    private readonly World _world;
    private readonly uint[] _componentHashes;

    internal Query(World world)
    {
        ComponentRegistry.EnsureRegistered<T1>();
        ComponentRegistry.EnsureRegistered<T2>();
        ComponentRegistry.EnsureRegistered<T3>();
        ComponentRegistry.EnsureRegistered<T4>();
        ComponentRegistry.EnsureRegistered<T5>();
        ComponentRegistry.EnsureRegistered<T6>();
        ComponentRegistry.EnsureRegistered<T7>();
        ComponentRegistry.EnsureRegistered<T8>();
        _world = world;
        _componentHashes = new uint[]
        {
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>(),
            ComponentTypeHelper.GetTypeHash<T4>(),
            ComponentTypeHelper.GetTypeHash<T5>(),
            ComponentTypeHelper.GetTypeHash<T6>(),
            ComponentTypeHelper.GetTypeHash<T7>(),
            ComponentTypeHelper.GetTypeHash<T8>()
        };
    }

    public QueryEnumerator<T1, T2, T3, T4, T5, T6, T7, T8> GetEnumerator()
    {
        return new QueryEnumerator<T1, T2, T3, T4, T5, T6, T7, T8>(_world, _componentHashes);
    }

    public int Count()
    {
        int c = 0;
        var en = GetEnumerator();
        while (en.MoveNext()) c++;
        return c;
    }

    public bool Any()
    {
        var en = GetEnumerator();
        return en.MoveNext();
    }

    /// <summary>
    /// Get the world this query belongs to.
    /// </summary>
    internal World GetWorld()
    {
        return _world;
    }

    public QueryFilterBuilder Without<TExclude>() where TExclude : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Without<TExclude>();
    }

    public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.Optional<TOpt>();
    }

    public QueryFilterBuilder WithAll<TOpt1>() where TOpt1 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>();
    }

    public QueryFilterBuilder WithAll<TOpt1, TOpt2>() where TOpt1 : unmanaged where TOpt2 : unmanaged
    {
        var b = new QueryFilterBuilder(_world, _componentHashes);
        return b.WithAll<TOpt1>().WithAll<TOpt2>();
    }

    public struct QueryFilterBuilder
    {
        private readonly World _world;
        private readonly uint[] _required;
        private readonly System.Collections.Generic.List<uint> _optional;
        private readonly System.Collections.Generic.List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _optional = new System.Collections.Generic.List<uint>();
            _exclude = new System.Collections.Generic.List<uint>();
        }

        public QueryFilterBuilder Optional<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder WithAll<TOpt>() where TOpt : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TOpt>();
            _optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
            return this;
        }

        public QueryFilterBuilder Without<TEx>() where TEx : unmanaged
        {
            ComponentRegistry.EnsureRegistered<TEx>();
            _exclude.Add(ComponentTypeHelper.GetTypeHash<TEx>());
            return this;
        }

        public QueryEnumerator<T1, T2, T3, T4, T5, T6, T7, T8> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2, T3, T4, T5, T6, T7, T8>(_world, _required, _optional.ToArray(), _exclude.ToArray());
        }
    }
}
