namespace GrapeEngine.Scripting.Core;

/// <summary>
/// Configuration for batch operation optimization.
/// </summary>
public static class BatchConfiguration
{
    /// <summary>
    /// Minimum number of operations to consider batching.
    /// If below this threshold, individual operations may be faster.
    /// </summary>
    public static int MinBatchSize { get; set; } = 10;

    /// <summary>
    /// Maximum operations to batch in a single call.
    /// Larger batches reduce P/Invoke overhead but increase latency per batch.
    /// </summary>
    public static int MaxBatchSize { get; set; } = 1000;

    /// <summary>
    /// Enable automatic batching for component access.
    /// </summary>
    public static bool AutoBatchingEnabled { get; set; } = true;
}
