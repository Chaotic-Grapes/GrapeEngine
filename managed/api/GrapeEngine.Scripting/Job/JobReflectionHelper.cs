/* Start Header *****************************************************************/
/*!
\file   JobReflectionHelper.cs
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
\brief
Reflection utilities for analyzing C# job types and extracting component access patterns.
Automatically infers read/write component declarations from job implementation.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using GrapeEngine.Scripting.Core;

namespace GrapeEngine.Scripting.Job;

/// <summary>
/// Analyzes job types using reflection to extract metadata and component access patterns.
/// </summary>
internal static class JobReflectionHelper
{
    /// <summary>
    /// Get component access declarations for a job type by analyzing attributes.
    /// </summary>
    public static ComponentAccessInfo GetComponentAccess(Type jobType)
    {
        var info = new ComponentAccessInfo { JobType = jobType };

        // Check for JobComponentAccess attributes on the type
        var accessAttributes = jobType.GetCustomAttributes<JobComponentAccessAttribute>();
        foreach (var attr in accessAttributes)
        {
            foreach (var readType in attr.ReadComponents)
                info.ReadableComponents.Add(readType);
            foreach (var writeType in attr.WriteComponents)
                info.WritableComponents.Add(writeType);
            foreach (var readWriteType in attr.ReadWriteComponents)
            {
                info.ReadableComponents.Add(readWriteType);
                info.WritableComponents.Add(readWriteType);
            }
        }

        // Analyze Execute method parameters for component types
        var executeMethod = FindExecuteMethod(jobType);
        if (executeMethod != null)
        {
            var parameters = executeMethod.GetParameters();
            foreach (var param in parameters)
            {
                // Skip Entity parameter
                if (param.ParameterType == typeof(Entity))
                    continue;

                var paramType = param.ParameterType;

                // Check for 'ref' parameter - indicates write access
                if (param.GetCustomAttribute<System.Runtime.InteropServices.InAttribute>() != null)
                {
                    // 'in' parameter - read-only
                    if (!info.ReadableComponents.Contains(paramType))
                        info.ReadableComponents.Add(paramType);
                }
                else if (paramType.IsByRef)
                {
                    // 'ref' parameter - read-write
                    var elementType = paramType.GetElementType();
                    if (elementType != null)
                    {
                        if (!info.WritableComponents.Contains(elementType))
                            info.WritableComponents.Add(elementType);
                    }
                }
                else
                {
                    // Regular parameter - likely component type, treat as readable
                    if (!info.ReadableComponents.Contains(paramType))
                        info.ReadableComponents.Add(paramType);
                }
            }
        }

        return info;
    }

    /// <summary>
    /// Get the job name from type or explicit attribute.
    /// </summary>
    public static string GetJobName(Type jobType)
    {
        var nameAttr = jobType.GetCustomAttribute<JobNameAttribute>();
        if (nameAttr?.Name != null)
            return nameAttr.Name;

        return jobType.Name;
    }

    /// <summary>
    /// Get the job name from an instance.
    /// </summary>
    public static string GetJobName(object job)
    {
        if (job is IJob jobInterface)
        {
            return jobInterface.GetJobName();
        }

        return GetJobName(job.GetType());
    }

    /// <summary>
    /// Find the Execute method in a job type.
    /// </summary>
    private static MethodInfo? FindExecuteMethod(Type jobType)
    {
        return jobType.GetMethod("Execute", 
            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
    }

    /// <summary>
    /// Get all jobs in an assembly matching the IJob interface.
    /// </summary>
    public static IEnumerable<Type> GetAllJobTypes(Assembly? assembly = null)
    {
        assembly ??= Assembly.GetExecutingAssembly();

        return assembly.GetTypes()
            .Where(t => typeof(IJob).IsAssignableFrom(t) && !t.IsInterface && !t.IsAbstract);
    }

    /// <summary>
    /// Validate that a job's actual component accesses match declared accesses.
    /// Checks Execute method parameters against JobComponentAccess attributes.
    /// </summary>
    /// <returns>Validation result with details if any mismatches found</returns>
    public static ComponentAccessValidationResult ValidateComponentAccess(Type jobType)
    {
        var result = new ComponentAccessValidationResult { JobType = jobType };
        
        // Get declared access from attributes
        var declaredAccess = GetComponentAccess(jobType);
        
        // Find Execute method and analyze parameters
        var executeMethod = FindExecuteMethod(jobType);
        if (executeMethod == null)
        {
            result.IsValid = true;
            result.Message = "No Execute method found";
            return result;
        }

        var parameters = executeMethod.GetParameters();
        var actualReadComponents = new HashSet<Type>();
        var actualWriteComponents = new HashSet<Type>();

        foreach (var param in parameters)
        {
            // Skip Entity and non-component parameters
            if (param.ParameterType == typeof(Entity))
                continue;
            if (param.ParameterType.IsPrimitive || param.ParameterType == typeof(string))
                continue;

            var paramType = param.ParameterType;

            // Determine access mode from parameter modifier
            if (param.GetCustomAttribute<System.Runtime.InteropServices.InAttribute>() != null)
            {
                // 'in' parameter - read-only
                actualReadComponents.Add(paramType);
            }
            else if (paramType.IsByRef)
            {
                // 'ref' parameter - read-write
                var elementType = paramType.GetElementType();
                if (elementType != null)
                {
                    actualWriteComponents.Add(elementType);
                    actualReadComponents.Add(elementType);
                }
            }
            else
            {
                // Regular parameter - assume read access
                actualReadComponents.Add(paramType);
            }
        }

        // Check for undeclared reads
        var undeclaredReads = actualReadComponents
            .Where(t => !declaredAccess.ReadableComponents.Contains(t) && !declaredAccess.WritableComponents.Contains(t))
            .ToList();

        // Check for undeclared writes
        var undeclaredWrites = actualWriteComponents
            .Where(t => !declaredAccess.WritableComponents.Contains(t))
            .ToList();

        if (undeclaredReads.Any() || undeclaredWrites.Any())
        {
            result.IsValid = false;
            result.UndeclaredReadComponents = undeclaredReads;
            result.UndeclaredWriteComponents = undeclaredWrites;
            
            var messages = new List<string>();
            if (undeclaredReads.Any())
                messages.Add($"Undeclared read access: {string.Join(", ", undeclaredReads.Select(t => t.Name))}");
            if (undeclaredWrites.Any())
                messages.Add($"Undeclared write access: {string.Join(", ", undeclaredWrites.Select(t => t.Name))}");
            
            result.Message = string.Join("; ", messages);
        }
        else
        {
            result.IsValid = true;
            result.Message = "All component accesses properly declared";
        }

        return result;
    }

    /// <summary>
    /// Generate a debug report for job component access.
    /// </summary>
    public static string GenerateAccessReport(ComponentAccessInfo info)
    {
        var lines = new List<string>
        {
            $"Job: {info.JobType?.Name ?? "Unknown"}",
            $"  Readable Components: {string.Join(", ", info.ReadableComponents.Select(t => t.Name))}",
            $"  Writable Components: {string.Join(", ", info.WritableComponents.Select(t => t.Name))}"
        };

        return string.Join("\n", lines);
    }
}

/// <summary>
/// Result of validating a job's component access declarations.
/// </summary>
public class ComponentAccessValidationResult
{
    public Type? JobType { get; set; }
    public bool IsValid { get; set; }
    public string Message { get; set; } = string.Empty;
    public List<Type> UndeclaredReadComponents { get; set; } = [];
    public List<Type> UndeclaredWriteComponents { get; set; } = [];
}

/// <summary>
/// Metadata about component access for a job type.
/// </summary>
internal class ComponentAccessInfo
{
    public Type? JobType { get; set; }
    public HashSet<Type> ReadableComponents { get; } = [];
    public HashSet<Type> WritableComponents { get; } = [];

    public bool HasConflict(ComponentAccessInfo other)
    {
        // Write-write conflict
        if (WritableComponents.Overlaps(other.WritableComponents))
            return true;

        // Read-write conflict (both directions)
        if (WritableComponents.Overlaps(other.ReadableComponents))
            return true;

        if (ReadableComponents.Overlaps(other.WritableComponents))
            return true;

        return false;
    }

    public override string ToString() => JobReflectionHelper.GenerateAccessReport(this);
}

/// <summary>
/// Attribute to explicitly declare a job's name for profiling.
/// </summary>
[AttributeUsage(AttributeTargets.Class | AttributeTargets.Struct)]
public sealed class JobNameAttribute(string name) : Attribute
{
    public string Name { get; } = name;
}
