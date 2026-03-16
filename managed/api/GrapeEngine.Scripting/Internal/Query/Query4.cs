namespace GrapeEngine.Scripting.Internal.Query;


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
        _componentHashes =
        [
            ComponentTypeHelper.GetTypeHash<T1>(),
            ComponentTypeHelper.GetTypeHash<T2>(),
            ComponentTypeHelper.GetTypeHash<T3>(),
            ComponentTypeHelper.GetTypeHash<T4>()
        ];
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

        public QueryEnumerator<T1, T2, T3, T4> GetEnumerator()
        {
            return new QueryEnumerator<T1, T2, T3, T4>(_world, [.. _required, .. _withAll], [.. _optional], [.. _exclude]);
        }
    }
}
