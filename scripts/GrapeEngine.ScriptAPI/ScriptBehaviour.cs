using System;
using System.Runtime.InteropServices;

namespace GrapeEngine.Scripting
{
    /// <summary>
    /// Base class for all entity scripts in GrapeEngine.
    /// Inherit from this class to create custom behaviors for entities.
    /// </summary>
    public abstract class ScriptBehaviour
    {
        /// <summary>
        /// The unique identifier of the entity this script is attached to.
        /// </summary>
        public ulong EntityId { get; internal set; }

        /// <summary>
        /// Called once when the script is first initialized.
        /// Use this for setup and initialization logic.
        /// </summary>
        public virtual void OnStart() { }

        /// <summary>
        /// Called every frame while the entity is active.
        /// </summary>
        public virtual void OnUpdate() { }

        /// <summary>
        /// Called at fixed time intervals for physics updates.
        /// </summary>
        public virtual void OnFixedUpdate() { }

        /// <summary>
        /// Called every frame after all OnUpdate calls.
        /// </summary>
        public virtual void OnLateUpdate() { }

        /// <summary>
        /// Called when the script is enabled.
        /// </summary>
        public virtual void OnEnable() { }

        /// <summary>
        /// Called when the script is disabled.
        /// </summary>
        public virtual void OnDisable() { }

        /// <summary>
        /// Called when the script or entity is being destroyed.
        /// Use this for cleanup logic.
        /// </summary>
        public virtual void OnDestroy() { }

        // ------------ Entity API ------------ //

        /// <summary>
        /// Get a component from this entity.
        /// Returns null if the component doesn't exist.
        /// </summary>
        public T? GetComponent<T>() where T : unmanaged
        {
            unsafe
            {
                var size = sizeof(T);
                var buffer = stackalloc byte[size]; // this is type byte*
                
                if (EntityAPI.GetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), buffer, size))
                    return *(T*)buffer; // dereference and cast to T*

                return null;
            }
        }

        /// <summary>
        /// Add or update a component on this entity.
        /// </summary>
        public void SetComponent<T>(T component) where T : unmanaged
        {
            unsafe // needed for pointer operations as C# is safe by default!
            {
                EntityAPI.SetComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>(), &component, sizeof(T));
            }
        }

        /// <summary>
        /// Check if this entity has a component.
        /// </summary>
        public bool HasComponent<T>() where T : unmanaged
        {
            // This does not need the unsafe context as no pointers are used
            // Check the method above. It uses pointers. (&component)
            return EntityAPI.HasComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
        }

        /// <summary>
        /// Remove a component from this entity.
        /// </summary>
        public void RemoveComponent<T>() where T : unmanaged
        {
            EntityAPI.RemoveComponent(EntityId, ComponentTypeRegistry.GetTypeHash<T>());
        }

        /// <summary>
        /// Destroy this entity.
        /// </summary>
        public void Destroy()
        {
            EntityAPI.DestroyEntity(EntityId);
        }

        /// <summary>
        /// Log a message to console.
        /// </summary>
        /// <param name="message">The message to log</param>
        /// <param name="level">The severity of the log</param>
        protected void Log(string message, LogLevel level = LogLevel.Info)
            => Logging.Log(message, level);
    }

    // ============================================================================
    // Math Types - MUST match C++ memory layout exactly for marshaling
    // ============================================================================
    // NOTE: Do NOT use System.Numerics types for marshaling!
    // It caused a memory corruption the last time I tried!
    // ============================================================================

    /// <summary>
    /// 3D Vector
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3(float x, float y, float z)
    {
        public float X = x, Y = y, Z = z;

        /// <summary>
        /// Zero vector (0, 0, 0)
        /// </summary>
        public static Vector3 Zero => new(0, 0, 0);

        /// <summary>
        /// One vector (1, 1, 1)
        /// </summary>
        public static Vector3 One => new(1, 1, 1);

        /// <summary>
        /// Up vector (0, 1, 0)
        /// </summary>
        public static Vector3 Up => new(0, 1, 0);

        /// <summary>
        /// Down vector (0, -1, 0)
        /// </summary>
        public static Vector3 Down => new(0, -1, 0);

        /// <summary>
        /// Left vector (-1, 0, 0)
        /// </summary>
        public static Vector3 Left => new(-1, 0, 0);

        /// <summary>
        /// Right vector (1, 0, 0)
        /// </summary>
        public static Vector3 Right => new(1, 0, 0);

        /// <summary>
        /// Forward vector (0, 0, 1)
        /// </summary>
        public static Vector3 Forward => new(0, 0, 1);
        
        /// <summary>
        /// Back vector (0, 0, -1)
        /// </summary>
        public static Vector3 Back => new(0, 0, -1);

        public static Vector3 operator +(Vector3 a, Vector3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        public static Vector3 operator -(Vector3 a, Vector3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        public static Vector3 operator *(Vector3 a, float scalar) => new(a.X * scalar, a.Y * scalar, a.Z * scalar);
        public static Vector3 operator *(float scalar, Vector3 a) => new(a.X * scalar, a.Y * scalar, a.Z * scalar);
        public static Vector3 operator /(Vector3 a, float scalar) => new(a.X / scalar, a.Y / scalar, a.Z / scalar);

        /// <summary>
        /// The magnitude (length) of the vector.
        /// </summary>
        public readonly float Magnitude => MathF.Sqrt(X * X + Y * Y + Z * Z);

        /// <summary>
        /// The normalized (unit length) vector.
        /// </summary>
        public readonly Vector3 Normalized => this / Magnitude;

        public readonly override string ToString() => $"({X}, {Y}, {Z})";
    }

    /// <summary>
    /// 2D Vector
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2(float x, float y)
    {
        public float X = x, Y = y;

        /// <summary>
        /// Zero vector (0, 0)
        /// </summary>
        public static Vector2 Zero => new(0, 0);

        /// <summary>
        /// One vector (1, 1)
        /// </summary>
        public static Vector2 One => new(1, 1);

        /// <summary>
        /// Up vector (0, 1)
        /// </summary>
        public static Vector2 Up => new(0, 1);

        /// <summary>
        /// Down vector (0, -1)
        /// </summary>
        public static Vector2 Down => new(0, -1);

        /// <summary>
        /// Left vector (-1, 0)
        /// </summary>
        public static Vector2 Left => new(-1, 0);

        /// <summary>
        /// Right vector (1, 0)
        /// </summary>
        public static Vector2 Right => new(1, 0);

        public static Vector2 operator +(Vector2 a, Vector2 b) => new(a.X + b.X, a.Y + b.Y);
        public static Vector2 operator -(Vector2 a, Vector2 b) => new(a.X - b.X, a.Y - b.Y);
        public static Vector2 operator *(Vector2 a, float scalar) => new(a.X * scalar, a.Y * scalar);
        public static Vector2 operator /(Vector2 a, float scalar) => new(a.X / scalar, a.Y / scalar);

        /// <summary>
        /// The magnitude (length) of the vector.
        /// </summary>
        public readonly float Magnitude => MathF.Sqrt(X * X + Y * Y);

        /// <summary>
        /// The normalized (unit length) vector.
        /// </summary>
        public readonly Vector2 Normalized => this / Magnitude;

        public readonly override string ToString() => $"({X}, {Y})";
    }

    /// <summary>
    /// Quaternion for rotations.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion(float x, float y, float z, float w)
    {
        public float X = x, Y = y, Z = z, W = w;

        public static Quaternion Identity => new(0, 0, 0, 1);

        public readonly override string ToString() => $"({X}, {Y}, {Z}, {W})";
    }

    /// <summary>
    /// Internal API - P/Invoke declarations for native engine functions.
    /// Do not call these directly - use ScriptBehaviour methods instead.
    /// </summary>
    internal static class EntityAPI
    {
        private const string NativeLib = "__Internal";

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe bool GetComponent(ulong entityId, uint typeHash, byte* outBuffer, int bufferSize);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern unsafe void SetComponent(ulong entityId, uint typeHash, void* componentData, int dataSize);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool HasComponent(ulong entityId, uint typeHash);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RemoveComponent(ulong entityId, uint typeHash);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DestroyEntity(ulong entityId);
    }

    /// <summary>
    /// Component type registry using compile-time hash matching.
    /// Type hashes MUST match the C++ side component type IDs.
    /// </summary>
    internal static class ComponentTypeRegistry
    {
        // FNV-1a hash function - matches C++ ComponentType::Hash()
        private static uint FNV1aHash(string str)
        {
            // FNV-1a prime and offset
            const uint fnvPrime = 0x01000193;
            const uint fnvOffset = 0x811C9DC5;

            var hash = fnvOffset;
            foreach (var c in str)
            {
                hash ^= c;
                hash *= fnvPrime;
            }

            return hash;
        }

        /// <summary>
        /// Get the type hash for a component type.
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <returns></returns>
        public static uint GetTypeHash<T>() where T : unmanaged
        {
            return FNV1aHash(typeof(T).Name);
        }
    }
}
