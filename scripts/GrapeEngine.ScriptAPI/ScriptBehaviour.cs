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

        // TODO: Remove delta time
        /// <summary>
        /// Called every frame while the entity is active.
        /// </summary>
        public virtual void OnUpdate(float dt) { }

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

        // /// <summary>
        // /// Get the transform component of this entity.
        // /// </summary>
        // public Transform GetTransform()
        // {
        //     return EntityAPI.GetTransform(EntityId);
        // }

        // /// <summary>
        // /// Set the position of this entity.
        // /// </summary>
        // public void SetPosition(Vector3 position)
        // {
        //     EntityAPI.SetPosition(EntityId, position);
        // }

        // TODO: Create a struct wrapper for entities?
        /// <summary>
        /// Creates a new entity in the scene.
        /// </summary>
        // public void Create()
        // {
        //     EntityAPI.CreateEntity(EntityId);
        // }

        /// <summary>
        /// Destroy this entity.
        /// </summary>
        public void Destroy()
        {
            EntityAPI.DestroyEntity(EntityId);
        }

        /// <summary>
        /// Log a message to the console.
        /// </summary>
        protected void Log(string message)
        {
            Console.WriteLine($"[Script:{GetType().Name}] {message}");
        }
    }

    /// <summary>
    /// Transform component data.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Transform
    {
        public Vector3 Position;
        public Quaternion Rotation;
        public Vector3 Scale;

        public Transform(Vector3 position, Quaternion rotation, Vector3 scale)
        {
            Position = position;
            Rotation = rotation;
            Scale = scale;
        }
    }

    /// <summary>
    /// 3D Vector.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float X, Y, Z;

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public static Vector3 Zero => new Vector3(0, 0, 0);
        public static Vector3 One => new Vector3(1, 1, 1);
        public static Vector3 Up => new Vector3(0, 1, 0);
        public static Vector3 Down => new Vector3(0, -1, 0);
        public static Vector3 Left => new Vector3(-1, 0, 0);
        public static Vector3 Right => new Vector3(1, 0, 0);
        public static Vector3 Forward => new Vector3(0, 0, 1);
        public static Vector3 Back => new Vector3(0, 0, -1);

        public static Vector3 operator +(Vector3 a, Vector3 b) => new Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        public static Vector3 operator -(Vector3 a, Vector3 b) => new Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        public static Vector3 operator *(Vector3 a, float scalar) => new Vector3(a.X * scalar, a.Y * scalar, a.Z * scalar);
        public static Vector3 operator *(float scalar, Vector3 a) => new Vector3(a.X * scalar, a.Y * scalar, a.Z * scalar);
        public static Vector3 operator /(Vector3 a, float scalar) => new Vector3(a.X / scalar, a.Y / scalar, a.Z / scalar);

        public float Magnitude => MathF.Sqrt(X * X + Y * Y + Z * Z);
        public Vector3 Normalized => this / Magnitude;

        public override string ToString() => $"({X}, {Y}, {Z})";
    }

    /// <summary>
    /// 2D Vector.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float X, Y;

        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }

        public static Vector2 Zero => new Vector2(0, 0);
        public static Vector2 One => new Vector2(1, 1);
        public static Vector2 Up => new Vector2(0, 1);
        public static Vector2 Down => new Vector2(0, -1);
        public static Vector2 Left => new Vector2(-1, 0);
        public static Vector2 Right => new Vector2(1, 0);

        public static Vector2 operator +(Vector2 a, Vector2 b) => new Vector2(a.X + b.X, a.Y + b.Y);
        public static Vector2 operator -(Vector2 a, Vector2 b) => new Vector2(a.X - b.X, a.Y - b.Y);
        public static Vector2 operator *(Vector2 a, float scalar) => new Vector2(a.X * scalar, a.Y * scalar);
        public static Vector2 operator /(Vector2 a, float scalar) => new Vector2(a.X / scalar, a.Y / scalar);

        public float Magnitude => MathF.Sqrt(X * X + Y * Y);
        public Vector2 Normalized => this / Magnitude;

        public override string ToString() => $"({X}, {Y})";
    }

    /// <summary>
    /// Quaternion for rotations.
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion
    {
        public float X, Y, Z, W;

        public Quaternion(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public static Quaternion Identity => new Quaternion(0, 0, 0, 1);

        public override string ToString() => $"({X}, {Y}, {Z}, {W})";
    }

    /// <summary>
    /// Internal API - P/Invoke declarations for native engine functions.
    /// Do not call these directly - use ScriptBehaviour methods instead.
    /// </summary>
    internal static class EntityAPI
    {
        private const string NativeLib = "__Internal";

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern Transform GetTransform(ulong entityId);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SetPosition(ulong entityId, Vector3 position);

        // [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        // public static extern bool HasComponent<T>(ulong entityId) where T : struct;

        // [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        // public static extern T? GetComponent<T>(ulong entityId) where T : struct;

        // [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        // public static extern void AddComponent<T>(ulong entityId, T component) where T : struct;

        // [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        // public static extern void RemoveComponent<T>(ulong entityId) where T : struct;

        // TODO: Fix this.
        // Possibly need to create an entity struct wrapper
        // Need to figure out how to pass complex types via P/Invoke
        // Probably need Marshal
        // [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        // public static extern Entity CreateEntity(ulong entityId);

        [DllImport(NativeLib, CallingConvention = CallingConvention.Cdecl)]
        public static extern void DestroyEntity(ulong entityId);
    }
}
