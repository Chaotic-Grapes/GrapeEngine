/* Start Header *****************************************************************/
/*!
\file   NativeSystemBridge.cs
\brief  Exposes unmanaged entry points so the native engine can invoke managed
        systems discovered by `SystemDiscovery`.
*/
/* End Header *******************************************************************/

using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using GrapeEngine.Scripting.Core;
using GrapeEngine.Scripting.Systems;

namespace GrapeEngine.Scripting.Hosting;

internal static class NativeSystemBridge
{
    // Called from native with a system handle assigned by SystemDiscovery.
    // The native side is expected to pass the same handle returned during discovery.
    [UnmanagedCallersOnly(EntryPoint = "ManagedSystem_InvokeOnCreate")]
    public static void InvokeOnCreate(ulong systemHandle, nint worldPtr)
    {
        var instance = SystemDiscovery.GetSystemInstance(systemHandle);
        if (instance is ISystem sys)
        {
            // World has an internal constructor that accepts IntPtr
            var world = new World((IntPtr)worldPtr);
            try
            {
                sys.OnCreate(world);
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[NativeSystemBridge] OnCreate exception: {ex.Message}", LogLevel.Error);
            }
        }
        else
        {
            Logging.LogInternal($"[NativeSystemBridge] InvokeOnCreate: system not found or not ISystem (handle={systemHandle})", LogLevel.Warning);
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "ManagedSystem_InvokeOnUpdate")]
    public static void InvokeOnUpdate(ulong systemHandle, nint worldPtr)
    {
        var instance = SystemDiscovery.GetSystemInstance(systemHandle);
        if (instance is ISystem sys)
        {
            var world = new World((IntPtr)worldPtr);
            try
            {
                sys.OnUpdate(world);
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[NativeSystemBridge] OnUpdate exception: {ex.Message}", LogLevel.Error);
            }
        }
        else
        {
            Logging.LogInternal($"[NativeSystemBridge] InvokeOnUpdate: system not found or not ISystem (handle={systemHandle})", LogLevel.Warning);
        }
    }

    // New job-capable update entrypoint. Returns native job handle (nint) if the
    // managed system schedules work and exposes it via ISystemJob. Returns 0 otherwise.
    [UnmanagedCallersOnly(EntryPoint = "ManagedSystem_InvokeOnUpdateJob")]
    public static nint InvokeOnUpdateJob(ulong systemHandle, nint worldPtr, nint dependsOnNativeHandle)
    {
        var instance = SystemDiscovery.GetSystemInstance(systemHandle);
        if (instance is ISystemJob sysJob)
        {
            var world = new World((IntPtr)worldPtr);
            try
            {
                Job.JobHandle? dependsOn = dependsOnNativeHandle == 0 ? null : new Job.JobHandle(dependsOnNativeHandle);
                var result = sysJob.OnUpdateWithJob(world, dependsOn);
                return result?.NativeHandle ?? nint.Zero;
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[NativeSystemBridge] OnUpdateWithJob exception: {ex.Message}", LogLevel.Error);
                return nint.Zero;
            }
        }
        else
        {
            // Fallback: call legacy OnUpdate and return invalid handle
            if (instance is ISystem sys)
            {
                try
                {
                    sys.OnUpdate(new World((IntPtr)worldPtr));
                }
                catch (Exception ex)
                {
                    Logging.LogInternal($"[NativeSystemBridge] OnUpdate fallback exception: {ex.Message}", LogLevel.Error);
                }
            }
            else
            {
                Logging.LogInternal($"[NativeSystemBridge] InvokeOnUpdateJob: system not found or not ISystem (handle={systemHandle})", LogLevel.Warning);
            }
            return nint.Zero;
        }
    }

    // Create and register a system instance by CLR type full name. Returns assigned handle or 0.
    [UnmanagedCallersOnly(EntryPoint = "ManagedSystem_CreateInstanceFromTypeName")]
    public static ulong CreateInstanceFromTypeName(nint typeNameUtf8Ptr)
    {
        try
        {
            var typeName = Marshal.PtrToStringUTF8(typeNameUtf8Ptr) ?? string.Empty;
            if (string.IsNullOrWhiteSpace(typeName))
                return 0;

            // Search loaded assemblies for the type
            var type = AppDomain.CurrentDomain.GetAssemblies()
                .SelectMany(a => a.GetTypes())
                .FirstOrDefault(t => string.Equals(t.FullName, typeName, StringComparison.Ordinal));

            if (type == null)
            {
                Logging.LogInternal($"[NativeSystemBridge] CreateInstanceFromTypeName: type not found: {typeName}", LogLevel.Warning);
                return 0;
            }

            return SystemDiscovery.CreateSystemInstanceFromType(type);
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[NativeSystemBridge] CreateInstanceFromTypeName error: {ex.Message}", LogLevel.Error);
            return 0;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "ManagedSystem_InvokeOnDestroy")]
    public static void InvokeOnDestroy(ulong systemHandle, nint worldPtr)
    {
        var instance = SystemDiscovery.GetSystemInstance(systemHandle);
        if (instance is ISystem sys)
        {
            var world = new World((IntPtr)worldPtr);
            try
            {
                sys.OnDestroy(world);
            }
            catch (Exception ex)
            {
                Logging.LogInternal($"[NativeSystemBridge] OnDestroy exception: {ex.Message}", LogLevel.Error);
            }
        }
        else
        {
            Logging.LogInternal($"[NativeSystemBridge] InvokeOnDestroy: system not found or not ISystem (handle={systemHandle})", LogLevel.Warning);
        }
    }
}
