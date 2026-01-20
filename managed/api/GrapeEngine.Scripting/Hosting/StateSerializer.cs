/* Start Header *****************************************************************/
/*!
\file   StateSerializer.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Utility for serializing and deserializing system state during hot reload.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System.Reflection;

namespace GrapeEngine.Scripting.Hosting;

/// <summary>
/// Serializes and deserializes system state marked with [Preserve] attribute.
/// Supports primitive types, strings, and custom serializable objects.
/// </summary>
public static class StateSerializer
{
    /// <summary>
    /// Serialize all [Preserve] fields from an object into a byte array.
    /// </summary>
    public static byte[] SerializePreservedFields(object instance)
    {
        if (instance == null)
            return [];

        using var ms = new MemoryStream();
        using var writer = new BinaryWriter(ms);
        var type = instance.GetType();
        var preservedFields = GetPreservedFields(type);

        // Write field count
        writer.Write(preservedFields.Count);

        // Write each preserved field
        foreach (var field in preservedFields)
        {
            object? value = field.GetValue(instance);
            WriteFieldValue(writer, field.Name, field.FieldType, value);
        }

        return ms.ToArray();
    }

    /// <summary>
    /// Deserialize [Preserve] fields into an object from a byte array.
    /// </summary>
    public static void DeserializePreservedFields(object instance, byte[]? data)
    {
        if (instance == null || data == null || data.Length == 0)
            return;

        try
        {
            using var ms = new MemoryStream(data);
            using var reader = new BinaryReader(ms);
            var type = instance.GetType();
            var preservedFields = GetPreservedFields(type);
            var fieldsByName = preservedFields.ToDictionary(f => f.Name);

            // Read field count
            int fieldCount = reader.ReadInt32();

            // Read each field
            for (int i = 0; i < fieldCount; i++)
            {
                string fieldName = reader.ReadString();

                if (fieldsByName.TryGetValue(fieldName, out var field))
                {
                    object? value = ReadFieldValue(reader, field.FieldType);
                    try
                    {
                        field.SetValue(instance, value);
                    }
                    catch (Exception ex)
                    {
                        Logging.LogInternal($"[StateSerializer] Failed to set field {fieldName}: {ex.Message}", LogLevel.Error);
                    }
                }
                else
                {
                    // Field doesn't exist in new version, skip it
                    SkipFieldValue(reader);
                }
            }
        }
        catch (Exception ex)
        {
            Logging.LogInternal($"[StateSerializer] Error deserializing state: {ex.Message}", LogLevel.Error);
        }
    }

    /// <summary>
    /// Get all fields marked with [Preserve] attribute on a type.
    /// </summary>
    private static List<FieldInfo> GetPreservedFields(Type type)
    {
        return [.. type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance).Where(f => f.GetCustomAttribute<PreserveAttribute>() != null)];
    }

    /// <summary>
    /// Write a field value to the binary writer.
    /// </summary>
    private static void WriteFieldValue(BinaryWriter writer, string fieldName, Type fieldType, object? value)
    {
        writer.Write(fieldName);
        
        if (value == null)
        {
            writer.Write((byte)0); // Null marker
            return;
        }

        writer.Write((byte)1); // Not null marker

        if (fieldType == typeof(bool))
            writer.Write((bool)value);
        else if (fieldType == typeof(byte))
            writer.Write((byte)value);
        else if (fieldType == typeof(sbyte))
            writer.Write((sbyte)value);
        else if (fieldType == typeof(short))
            writer.Write((short)value);
        else if (fieldType == typeof(ushort))
            writer.Write((ushort)value);
        else if (fieldType == typeof(int))
            writer.Write((int)value);
        else if (fieldType == typeof(uint))
            writer.Write((uint)value);
        else if (fieldType == typeof(long))
            writer.Write((long)value);
        else if (fieldType == typeof(ulong))
            writer.Write((ulong)value);
        else if (fieldType == typeof(float))
            writer.Write((float)value);
        else if (fieldType == typeof(double))
            writer.Write((double)value);
        else if (fieldType == typeof(decimal))
            writer.Write((decimal)value);
        else if (fieldType == typeof(string))
            writer.Write((string)value ?? "");
        else if (fieldType == typeof(Vector2))
        {
            var v = (Vector2)value;
            writer.Write(v.X);
            writer.Write(v.Y);
        }
        else if (fieldType == typeof(Vector3))
        {
            var v = (Vector3)value;
            writer.Write(v.X);
            writer.Write(v.Y);
            writer.Write(v.Z);
        }
        else if (fieldType == typeof(Quaternion))
        {
            var q = (Quaternion)value;
            writer.Write(q.X);
            writer.Write(q.Y);
            writer.Write(q.Z);
            writer.Write(q.W);
        }
        else
        {
            // For complex types, try JSON serialization
            try
            {
                var json = System.Text.Json.JsonSerializer.Serialize(value);
                writer.Write(json);
            }
            catch
            {
                Logging.LogInternal($"[StateSerializer] Cannot serialize field {fieldName} of type {fieldType.Name}", LogLevel.Error);
                writer.Write(""); // Write empty JSON
            }
        }
    }

    /// <summary>
    /// Read a field value from the binary reader.
    /// </summary>
    private static object? ReadFieldValue(BinaryReader reader, Type fieldType)
    {
        byte isNull = reader.ReadByte();
        if (isNull == 0)
            return null;

        if (fieldType == typeof(bool))
            return reader.ReadBoolean();
        else if (fieldType == typeof(byte))
            return reader.ReadByte();
        else if (fieldType == typeof(sbyte))
            return reader.ReadSByte();
        else if (fieldType == typeof(short))
            return reader.ReadInt16();
        else if (fieldType == typeof(ushort))
            return reader.ReadUInt16();
        else if (fieldType == typeof(int))
            return reader.ReadInt32();
        else if (fieldType == typeof(uint))
            return reader.ReadUInt32();
        else if (fieldType == typeof(long))
            return reader.ReadInt64();
        else if (fieldType == typeof(ulong))
            return reader.ReadUInt64();
        else if (fieldType == typeof(float))
            return reader.ReadSingle();
        else if (fieldType == typeof(double))
            return reader.ReadDouble();
        else if (fieldType == typeof(decimal))
            return reader.ReadDecimal();
        else if (fieldType == typeof(string))
            return reader.ReadString();
        else if (fieldType == typeof(Vector2))
        {
            float x = reader.ReadSingle();
            float y = reader.ReadSingle();
            return new Vector2 { X = x, Y = y };
        }
        else if (fieldType == typeof(Vector3))
        {
            float x = reader.ReadSingle();
            float y = reader.ReadSingle();
            float z = reader.ReadSingle();
            return new Vector3 { X = x, Y = y, Z = z };
        }
        else if (fieldType == typeof(Quaternion))
        {
            float x = reader.ReadSingle();
            float y = reader.ReadSingle();
            float z = reader.ReadSingle();
            float w = reader.ReadSingle();
            return new Quaternion { X = x, Y = y, Z = z, W = w };
        }
        else
        {
            // Try JSON deserialization
            string json = reader.ReadString();
            if (string.IsNullOrEmpty(json))
                return Activator.CreateInstance(fieldType);

            try
            {
                return System.Text.Json.JsonSerializer.Deserialize(json, fieldType);
            }
            catch
            {
                Logging.LogInternal($"[StateSerializer] Cannot deserialize field of type {fieldType.Name}", LogLevel.Error);
                return Activator.CreateInstance(fieldType);
            }
        }
    }

    /// <summary>
    /// Skip a field value in the binary reader (for fields that no longer exist).
    /// </summary>
    private static void SkipFieldValue(BinaryReader reader)
    {
        byte isNull = reader.ReadByte();
        if (isNull == 0)
            return;

        // Try to read and skip - this is fragile, better to store type info
        try
        {
            // Read a string which might be the JSON representation
            reader.ReadString();
        }
        catch
        {
            // Couldn't skip properly
        }
    }
}
