using System.Text;
using System.Text.Json;
using GrapeEngine.Scripting.Internal.Unsafe;

namespace GrapeEngine.Scripting.Services;

public sealed class SaveSlotInfo
{
    public uint Slot { get; set; }
    public string DisplayName { get; set; } = string.Empty;
    public string SceneName { get; set; } = string.Empty;
    public string SourceScenePath { get; set; } = string.Empty;
    public string SnapshotFile { get; set; } = string.Empty;
    public string SavedAtUtc { get; set; } = string.Empty;
    public string SaveVersion { get; set; } = string.Empty;
    public double PlayTimeSeconds { get; set; }
    public ulong SceneFileBytes { get; set; }

    // Raw JSON from UserData payload to keep schema ownership in gameplay code.
    public JsonElement UserData { get; set; }
}

public static class SaveGame
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true
    };

    public static bool Save(uint slot, string displayName = "", object? userData = null)
    {
        string userDataJson = userData is null ? "{}" : JsonSerializer.Serialize(userData, JsonOptions);
        return SaveGameAPI.SaveSlot(slot, displayName ?? string.Empty, userDataJson);
    }

    public static bool SaveRaw(uint slot, string displayName, string userDataJson)
    {
        return SaveGameAPI.SaveSlot(slot, displayName ?? string.Empty, userDataJson ?? "{}");
    }

    public static bool Load(uint slot) => SaveGameAPI.LoadSlot(slot);

    public static bool Delete(uint slot) => SaveGameAPI.DeleteSlot(slot);

    public static bool Exists(uint slot) => SaveGameAPI.HasSlot(slot);

    public static bool SetProfile(string profileId)
    {
        return SaveGameAPI.SetActiveProfile(string.IsNullOrWhiteSpace(profileId) ? "default" : profileId);
    }

    public static string GetProfile()
    {
        return GetProfileUnsafe();
    }

    public static IReadOnlyList<SaveSlotInfo> ListSlots()
    {
        string json = ListSlotsJsonUnsafe();

        try
        {
            var slots = JsonSerializer.Deserialize<List<SaveSlotInfo>>(json, JsonOptions);
            if (slots is null)
            {
                return Array.Empty<SaveSlotInfo>();
            }

            slots.Sort((a, b) => a.Slot.CompareTo(b.Slot));
            return slots;
        }
        catch
        {
            return Array.Empty<SaveSlotInfo>();
        }
    }

    public static SaveSlotInfo? GetSlot(uint slot)
    {
        string json = GetSlotJsonUnsafe(slot);

        try
        {
            var info = JsonSerializer.Deserialize<SaveSlotInfo>(json, JsonOptions);
            if (info is null || info.Slot == 0 && string.IsNullOrEmpty(info.SceneName))
            {
                return null;
            }
            return info;
        }
        catch
        {
            return null;
        }
    }

    private unsafe delegate int NativeUtf8Writer(byte* buffer, int bufferSize);

    private static unsafe string GetProfileUnsafe()
    {
        return ReadUtf8Dynamic((buffer, size) => SaveGameAPI.GetActiveProfile(buffer, size), "default");
    }

    private static unsafe string ListSlotsJsonUnsafe()
    {
        return ReadUtf8Dynamic((buffer, size) => SaveGameAPI.GetSlotsJson(buffer, size), "[]");
    }

    private static unsafe string GetSlotJsonUnsafe(uint slot)
    {
        return ReadUtf8Dynamic((buffer, size) => SaveGameAPI.GetSlotJson(slot, buffer, size), "{}");
    }

    private static unsafe string ReadUtf8Dynamic(NativeUtf8Writer writer, string fallback)
    {
        const int stackSize = 1024;
        Span<byte> stack = stackalloc byte[stackSize];

        fixed (byte* stackPtr = stack)
        {
            int required = writer(stackPtr, stackSize);
            if (required < 0)
            {
                return fallback;
            }

            if (required < stackSize)
            {
                return Encoding.UTF8.GetString(stack[..required]);
            }

            byte[] rented = new byte[required + 1];
            fixed (byte* heapPtr = rented)
            {
                int actual = writer(heapPtr, rented.Length);
                if (actual < 0)
                {
                    return fallback;
                }

                int len = System.Math.Min(actual, rented.Length - 1);
                return Encoding.UTF8.GetString(rented, 0, len);
            }
        }
    }
}
