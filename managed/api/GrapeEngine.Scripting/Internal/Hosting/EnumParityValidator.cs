using GrapeEngine.Scripting.Services;
using GrapeEngine.Scripting.Systems.Attributes;
using System.Text;

namespace GrapeEngine.Scripting.Internal.Hosting;

/// <summary>
/// Validates that C# enum values match C++ engine definitions.
/// Called on assembly load to catch enum mismatches early.
/// </summary>
public static class EnumParityValidator
{
    /// <summary>
    /// Validate that all C# enums match C++ values.
    /// Should be called during assembly initialization.
    /// </summary>
    /// <returns>True if all enums match, false if mismatch detected.</returns>
    public static bool ValidateEnumParity()
    {
        bool valid = true;
        valid &= ValidateSystemGroupEnum();
        valid &= ValidateSystemRunModeEnum();

        if (!valid)
        {
            Logging.LogInternal(
                "[EnumParityValidator] WARNING: Enum mismatch detected! C# enum values do not match C++ definitions. This will cause P/Invoke marshaling errors.",
                LogLevel.Warning);
        }

        return valid;
    }

    /// <summary>
    /// Get a detailed report of all enum values for documentation purposes.
    /// Useful for verifying against C++ source files.
    /// </summary>
    public static string GetEnumParityReport()
    {
        var report = new StringBuilder();
        report.AppendLine("=== C# Enum Parity Report ===");
        report.AppendLine();

        report.AppendLine("SystemGroup values:");
        foreach (SystemGroup group in Enum.GetValues<SystemGroup>())
        {
            report.AppendLine($"  {group} = {(int)group}");
        }

        report.AppendLine();
        report.AppendLine("SystemRunMode values:");
        foreach (SystemRunMode mode in Enum.GetValues<SystemRunMode>())
        {
            report.AppendLine($"  {mode} = {(int)mode}");
        }

        return report.ToString();
    }

    private static bool ValidateSystemGroupEnum()
    {
        var expectedValues = new Dictionary<SystemGroup, int>
        {
            { SystemGroup.PreUpdate, 0 },
            { SystemGroup.Update, 1 },
            { SystemGroup.PostUpdate, 2 },
            { SystemGroup.PrePhysics, 3 },
            { SystemGroup.Physics, 4 },
            { SystemGroup.PostPhysics, 5 },
            { SystemGroup.PreRender, 6 },
            { SystemGroup.Render, 7 },
            { SystemGroup.PostRender, 8 }
        };

        bool valid = true;
        foreach (var (group, expectedValue) in expectedValues)
        {
            int actualValue = (int)group;
            if (actualValue != expectedValue)
            {
                Logging.LogInternal(
                    $"[EnumParityValidator] SystemGroup.{group} mismatch: expected {expectedValue}, got {actualValue}",
                    LogLevel.Warning);
                valid = false;
            }
        }

        return valid;
    }

    private static bool ValidateSystemRunModeEnum()
    {
        var expectedValues = new Dictionary<SystemRunMode, int>
        {
            { SystemRunMode.Always, 0 },
            { SystemRunMode.PlayOnly, 1 },
            { SystemRunMode.EditOnly, 2 }
        };

        bool valid = true;
        foreach (var (mode, expectedValue) in expectedValues)
        {
            int actualValue = (int)mode;
            if (actualValue != expectedValue)
            {
                Logging.LogInternal(
                    $"[EnumParityValidator] SystemRunMode.{mode} mismatch: expected {expectedValue}, got {actualValue}",
                    LogLevel.Warning);
                valid = false;
            }
        }

        return valid;
    }
}
