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

namespace GrapeEngine;

/// <summary>
/// Query for iterating over entities with one component type.
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
