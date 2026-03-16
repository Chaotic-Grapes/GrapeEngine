namespace GrapeEngine.Scripting.Internal.Query;


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
        _componentHashes =
        [
            ComponentTypeHelper.GetTypeHash<T1>()
        ];
    }

    public QueryEnumerator<T1> GetEnumerator()
    {
        return new QueryEnumerator<T1>(_world, _componentHashes);
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
        private readonly List<uint> _withAll;
        private readonly List<uint> _optional;
        private readonly List<uint> _exclude;

        internal QueryFilterBuilder(World world, uint[] required)
        {
            _world = world;
            _required = required;
            _withAll = [];
            _optional = [];
            _exclude = [];
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
            _withAll.Add(ComponentTypeHelper.GetTypeHash<TOpt>());
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
            return new QueryEnumerator<T1>(_world, [.. _required, .. _withAll], [.. _optional], [.. _exclude]);
        }
    }
}
