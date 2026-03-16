using System.Text.RegularExpressions;
using TUnit.Assertions;
using TUnit.Core;

namespace GrapeEngine.Scripting.Tests;

public class QueryFilterBuilderTests
{
    [Test]
    public async Task WithAll_ShouldAppendToWithAll_InAllBuilders()
    {
        // Arrange
        string querySource = LoadQuerySource();

        // Act
        const string expectedLine = "_withAll.Add(ComponentTypeHelper.GetTypeHash<TOpt>());";
        int withAllCount = CountOccurrences(querySource, expectedLine);

        // Assert
        await Assert.That(withAllCount).IsEqualTo(8);
    }

    [Test]
    public async Task WithAll_ShouldNotAppendToOptional_InAllBuilders()
    {
        // Arrange
        string querySource = LoadQuerySource();

        // Act
        MatchCollection withAllBlocks = Regex.Matches(
            querySource,
            @"public QueryFilterBuilder WithAll<TOpt>\(\) where TOpt : unmanaged\s*\{(?<body>.*?)\}",
            RegexOptions.Singleline);

        int optionalAddCount = withAllBlocks
            .Select(m => m.Groups["body"].Value)
            .Count(body => body.Contains("_optional.Add(ComponentTypeHelper.GetTypeHash<TOpt>());", StringComparison.Ordinal));

        // Assert
        await Assert.That(withAllBlocks.Count).IsEqualTo(8);
        await Assert.That(optionalAddCount).IsEqualTo(0);
    }

    private static string LoadQuerySource()
    {
        string queryDirectory = Path.GetFullPath(Path.Combine(
            AppContext.BaseDirectory,
            "..",
            "..",
            "..",
            "..",
            "..",
            "api",
            "GrapeEngine.Scripting",
            "Internal",
            "Query"));

        string[] queryFiles = Directory.GetFiles(queryDirectory, "Query*.cs", SearchOption.TopDirectoryOnly)
            .Where(path => !path.EndsWith("QueryEnumerator.cs", StringComparison.Ordinal))
            .Where(path => !path.EndsWith("QueryResult.cs", StringComparison.Ordinal))
            .OrderBy(path => path, StringComparer.Ordinal)
            .ToArray();

        return string.Join(Environment.NewLine, queryFiles.Select(File.ReadAllText));
    }

    private static int CountOccurrences(string source, string value)
    {
        int count = 0;
        int start = 0;

        while (true)
        {
            int index = source.IndexOf(value, start, StringComparison.Ordinal);
            if (index < 0)
            {
                return count;
            }

            count++;
            start = index + value.Length;
        }
    }
}
