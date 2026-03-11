#:package Terminal.Gui@1.19.0

using System.Diagnostics;
using Terminal.Gui;

namespace GitTrackLoc;

class Program
{
    private readonly record struct Contributor(int Commits, string Name, string Email);
    private readonly record struct CommitSummary(string Hash, string Subject);
    private readonly record struct CommitEntry(string Hash, string Name, string Email, string Subject);
    private readonly record struct ContributionData(
        Dictionary<string, (long added, long removed)> PerFile,
        Dictionary<string, List<CommitSummary>> FileCommits);

    private static string Quote(string s) => '"' + s.Replace("\"", "\\\"") + '"';

    static int Main(string[] args)
    {
        Application.Init();
        var repoPath = Directory.GetCurrentDirectory();
        var aliasGroups = LoadAliasGroups(repoPath);

        var contributors = GetContributors(repoPath);

        var top = Application.Top;

        var win = new Window("GitTrackLoc — Contributors")
        {
            X = 0,
            Y = 1,
            Width = Dim.Fill(),
            Height = Dim.Fill()
        };

        var status = new Label($"Repo: {repoPath}") { X = 0, Y = 0 };

        // Left: contributors list
        var left = new FrameView("Contributors") { X = 0, Y = 0, Width = 40, Height = Dim.Fill() };
        var contribNames = contributors.Select(c => $"{c.Commits,4}  {c.Name} <{c.Email}>").ToList();
        var listView = new ListView(contribNames) { X = 0, Y = 0, Width = Dim.Fill(), Height = Dim.Fill() };
        left.Add(listView);

        // Right: filters + contributions
        var right = new FrameView("Details") { X = Pos.Right(left) + 1, Y = 0, Width = Dim.Fill(), Height = Dim.Fill() };

        var sinceLabel = new Label("Since (YYYY-MM-DD):") { X = 0, Y = 0 };
        var sinceField = new TextField("") { X = Pos.Right(sinceLabel) + 1, Y = 0, Width = 20 };
        var untilLabel = new Label("Until (YYYY-MM-DD):") { X = 0, Y = 1 };
        var untilField = new TextField("") { X = Pos.Right(untilLabel) + 1, Y = 1, Width = 20 };
        var pathLabel = new Label("Path filters:") { X = 0, Y = 2 };
        var pathField = new TextField("externals/*;src/externals/*;include/externals/*;libs/*") { X = Pos.Right(pathLabel) + 1, Y = 2, Width = 60 };

        var loadBtn = new Button("Load Contributions") { X = 0, Y = 4 };

        var totalsLabel = new Label("Added: 0   Removed: 0   Net: 0") { X = 0, Y = 5 };
        var toggleBtn = new Button("Toggle Include") { X = 0, Y = 6 };
        var commitsBtn = new Button("Show Commits") { X = Pos.Right(toggleBtn) + 2, Y = 6 };

        var filesList = new ListView(new List<string>()) { X = 0, Y = Pos.Bottom(toggleBtn) + 1, Width = Dim.Fill(), Height = Dim.Fill() };

        // current per-file state and inclusion flags (reset on load)
        Dictionary<string, (long added, long removed)> currentPerFile = new();
        Dictionary<string, List<CommitSummary>> currentFileCommits = new(StringComparer.OrdinalIgnoreCase);
        List<string> orderedFiles = [];
        Dictionary<string, bool> included = new(StringComparer.OrdinalIgnoreCase);

        right.Add(sinceLabel, sinceField, untilLabel, untilField, pathLabel, pathField, loadBtn, totalsLabel, toggleBtn, commitsBtn, filesList);

        win.Add(left, right);
        top.Add(status, win);

        loadBtn.Clicked += async () => await ShowContributionsForSelectedAsync();

        // double-click or Enter to load contributor
        listView.OpenSelectedItem += async _ => await ShowContributionsForSelectedAsync();

        toggleBtn.Clicked += ToggleIncludeForSelected;
        commitsBtn.Clicked += ShowCommitsForSelected;
        filesList.OpenSelectedItem += _ => ShowCommitsForSelected();

        Application.Run();
        return 0;

        void UpdateFilesListUI()
        {
            // builds display items honoring included flags and updates totals
            var items = new List<string>();
            long totalAdded = 0, totalRemoved = 0;
            foreach (var f in orderedFiles)
            {
                if (!currentPerFile.TryGetValue(f, out var v))
                    continue;

                var incl = included.GetValueOrDefault(f, true);
                if (incl)
                {
                    totalAdded += v.added;
                    totalRemoved += v.removed;
                }

                var mark = incl ? "[x]" : "[ ]";
                var suffix = incl ? string.Empty : "  (excluded)";
                var commitCount = currentFileCommits.TryGetValue(f, out var commits) ? commits.Count : 0;
                items.Add($"{mark} {v.added,6} +  {v.removed,6} -  {f}  ({commitCount} commits){suffix}");
            }

            filesList.SetSource(items);
            totalsLabel.Text = $"Added: {totalAdded}   Removed: {totalRemoved}   Net: {totalAdded - totalRemoved}";

            // visual hint: if any file is excluded, change totals label color
            var hasExcluded = included.Values.Any(v => v == false);
            totalsLabel.ColorScheme = hasExcluded ? Colors.Dialog : Colors.Base;
        }

        void ToggleIncludeForSelected()
        {
            var idx = filesList.SelectedItem;
            if (idx < 0 || idx >= orderedFiles.Count) return;
            var file = orderedFiles[idx];
            var cur = included.GetValueOrDefault(file, true);
            included[file] = !cur;
            UpdateFilesListUI();
        }

        void ShowCommitsForSelected()
        {
            var idx = filesList.SelectedItem;
            if (idx < 0 || idx >= orderedFiles.Count) return;

            var file = orderedFiles[idx];
            if (!currentFileCommits.TryGetValue(file, out var commits) || commits.Count == 0)
            {
                MessageBox.Query("File Commits", $"No commit subjects found for:\n{file}", "OK");
                return;
            }
            ShowCommitDetailsDialog(file, commits);
        }

        async Task ShowContributionsForSelectedAsync()
        {
            if (listView.SelectedItem < 0 || listView.SelectedItem >= contributors.Count) return;
            var c = contributors[listView.SelectedItem];
            var since = sinceField.Text.ToString() ?? string.Empty;
            var until = untilField.Text.ToString() ?? string.Empty;
            var path = pathField.Text.ToString() ?? string.Empty;
            var authorMatchers = BuildAuthorMatchers(c.Name, c.Email, aliasGroups);

            // update UI to show loading and disable controls
            status.Text = $"Loading contributions for {c.Name}...";
            loadBtn.Enabled = false;
            listView.Enabled = false;
            toggleBtn.Enabled = false;
            commitsBtn.Enabled = false;
            Application.Refresh();

            try
            {
                // run the expensive calculation on a thread-pool thread
                var contributions = await Task.Run(() => CalculateLocForAuthor(
                    repoPath,
                    authorMatchers,
                    since,
                    until,
                    path,
                    (processed, total) => Application.MainLoop.Invoke(() =>
                    {
                        status.Text = total == 0
                            ? $"Loading contributions for {c.Name}... (no commits)"
                            : $"Loading contributions for {c.Name}... ({processed}/{total} commits)";
                        Application.Refresh();
                    })));

                // prepare display state: set currentPerFile, orderedFiles, and included flags
                currentPerFile = contributions.PerFile;
                currentFileCommits = contributions.FileCommits;
                orderedFiles = currentPerFile.OrderByDescending(kv => kv.Value.added - kv.Value.removed).Select(kv => kv.Key).ToList();
                included.Clear();
                foreach (var f in orderedFiles) included[f] = true; // default: count towards total

                // marshal UI updates back to the Terminal.Gui main loop
                Application.MainLoop.Invoke(() =>
                {
                    UpdateFilesListUI();
                    status.Text = $"Loaded for {c.Name} — files: {currentPerFile.Count}";
                    loadBtn.Enabled = true;
                    listView.Enabled = true;
                    toggleBtn.Enabled = orderedFiles.Count > 0;
                    commitsBtn.Enabled = orderedFiles.Count > 0;
                    Application.Refresh();
                });
            }
            catch (Exception ex) when (
                ex is InvalidOperationException ||
                ex is IOException ||
                ex is UnauthorizedAccessException)
            {
                Application.MainLoop.Invoke(() =>
                {
                    status.Text = $"Load failed: {ex.Message}";
                    loadBtn.Enabled = true;
                    listView.Enabled = true;
                    toggleBtn.Enabled = orderedFiles.Count > 0;
                    commitsBtn.Enabled = orderedFiles.Count > 0;
                    Application.Refresh();
                });
            }
        }
    }

    private static void ShowCommitDetailsDialog(string file, IReadOnlyList<CommitSummary> commits)
    {
        var commitLines = commits.Select(c => $"{c.Hash}  {c.Subject}").ToList();
        var content = $"File: {file}\nCommits: {commits.Count}\n\n" + string.Join('\n', commitLines);

        var dialog = new Dialog("File Commits")
        {
            Width = Dim.Percent(90),
            Height = Dim.Percent(80),
        };

        var text = new TextView
        {
            X = 0,
            Y = 0,
            Width = Dim.Fill(),
            Height = Dim.Fill(1),
            ReadOnly = true,
            WordWrap = false,
            Text = content,
        };

        var copyButton = new Button("Copy") { X = 0, Y = Pos.Bottom(text) };
        var closeButton = new Button("Close") { X = Pos.Right(copyButton) + 2, Y = Pos.Bottom(text) };

        copyButton.Clicked += () =>
        {
            if (TryCopyTextToClipboard(content, out var error))
            {
                MessageBox.Query("File Commits", "Commit list copied to clipboard.", "OK");
            }
            else
            {
                MessageBox.Query("File Commits", $"Clipboard copy failed:\n{error}", "OK");
            }
        };

        closeButton.Clicked += () => Application.RequestStop(dialog);
        dialog.Add(text, copyButton, closeButton);
        Application.Run(dialog);
    }

    private static bool TryCopyTextToClipboard(string text, out string error)
    {
        if (!OperatingSystem.IsWindows())
        {
            error = "Clipboard copy is currently implemented for Windows only.";
            return false;
        }

        try
        {
            var psi = new ProcessStartInfo
            {
                FileName = "cmd",
                Arguments = "/c clip",
                RedirectStandardInput = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            };

            using var process = Process.Start(psi);
            if (process == null)
            {
                error = "Failed to start clipboard process.";
                return false;
            }

            process.StandardInput.Write(text);
            process.StandardInput.Close();
            process.WaitForExit();

            if (process.ExitCode != 0)
            {
                error = $"Clipboard process exited with code {process.ExitCode}.";
                return false;
            }

            error = string.Empty;
            return true;
        }
        catch (InvalidOperationException ex)
        {
            error = ex.Message;
            return false;
        }
        catch (System.ComponentModel.Win32Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }

    private static List<Contributor> GetContributors(string repoPath)
    {
        var args = new List<string> { "shortlog", "-sne" };
        var output = RunGit(args, repoPath, out var exit);
        var list = new List<Contributor>();
        if (exit != 0 || string.IsNullOrWhiteSpace(output))
            return list;

        foreach (var line in output.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries))
        {
            var trimmed = line.TrimStart();
            var tab = trimmed.IndexOf('\t');
            if (tab <= 0)
                continue;

            var cntStr = trimmed[..tab].Trim();
            if (!int.TryParse(cntStr, out var commits))
                continue;

            var rest = trimmed[(tab + 1)..].Trim();
            var emailStart = rest.LastIndexOf('<');
            var emailEnd = rest.LastIndexOf('>');

            if (emailStart >= 0 && emailEnd > emailStart)
            {
                var name = rest[..emailStart].Trim();
                var email = rest.Substring(emailStart + 1, emailEnd - emailStart - 1).Trim();
                list.Add(new Contributor(commits, name, email));
            }
            else
            {
                list.Add(new Contributor(commits, rest, ""));
            }
        }

        return list;
    }

    private static ContributionData CalculateLocForAuthor(
        string repoPath,
        HashSet<string> authorMatchers,
        string since,
        string until,
        string pathspec,
        Action<int, int>? progress)
    {
        var allowed = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { ".c", ".cpp", ".h", ".hpp", ".cs" };
        var perFile = new Dictionary<string, (long added, long removed)>(StringComparer.OrdinalIgnoreCase);
        var fileCommits = new Dictionary<string, List<CommitSummary>>(StringComparer.OrdinalIgnoreCase);

        // Interpret pathspec as semicolon-separated exclusion filters. Default (empty) means no filters.
        // If pathspec is '*' we treat it as no filters as well.
        List<string> excludePathspecs = [];
        if (!string.IsNullOrWhiteSpace(pathspec) && pathspec != "*")
        {
            excludePathspecs = pathspec.Split([';', ','], StringSplitOptions.RemoveEmptyEntries)
                                       .Select(s => s.Trim())
                                       .Where(s => s.Length > 0)
                                       .ToList();
            if (excludePathspecs.Count > 0)
            {
                // add pathspecs later when invoking git show/log: include '.' then exclude patterns
                // (don't modify logArgs here because some git versions require the pathspecs after --)
            }
        }

        var commits = GetCommitsForAuthor(repoPath, authorMatchers, since, until);
        var totalCommits = commits.Count;
        progress?.Invoke(0, totalCommits);

        for (var i = 0; i < totalCommits; i++)
        {
            var commit = commits[i];
            var showArgs = new List<string> { "show", "--pretty=format:", "--unified=0", commit.Hash };

            // If exclude pathspecs were provided, add '.' then exclude specifiers so git ignores those paths
            if (excludePathspecs is { Count: > 0 })
            {
                showArgs.Add("--");
                showArgs.Add(".");

                foreach (var pat in excludePathspecs)
                {
                    showArgs.Add($":(exclude){pat}");
                }
            }

            var diff = RunGit(showArgs, repoPath, out var showExit);
            if (showExit == 0)
            {
                ParseDiffPerFile(diff, perFile, fileCommits, allowed, new CommitSummary(ShortHash(commit.Hash), commit.Subject));
            }

            var processed = i + 1;
            if (processed == totalCommits || processed % 10 == 0)
            {
                progress?.Invoke(processed, totalCommits);
            }
        }

        return new ContributionData(perFile, fileCommits);
    }

    private static void ParseDiffPerFile(
        string diff,
        Dictionary<string, (long added, long removed)> dict,
        Dictionary<string, List<CommitSummary>> fileCommits,
        HashSet<string> allowed,
        CommitSummary commit)
    {
        if (string.IsNullOrEmpty(diff)) return;

        var currentFile = string.Empty;
        var inBlockComment = false;
        var touchedFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        using var reader = new StringReader(diff);
        while (reader.ReadLine() is { } line)
        {
            if (line.StartsWith("diff --git"))
            {
                currentFile = null;
                inBlockComment = false;
                continue;
            }

            if (line.StartsWith("+++ "))
            {
                var path = line[4..].Trim();
                if (path.StartsWith("b/"))
                    path = path[2..];

                currentFile = path == "/dev/null"
                    ? string.Empty
                    : path;

                if (!string.IsNullOrEmpty(currentFile))
                {
                    var fileExt = Path.GetExtension(currentFile);
                    if (!string.IsNullOrEmpty(fileExt) && allowed.Contains(fileExt))
                    {
                        touchedFiles.Add(currentFile);
                    }
                }

                continue;
            }

            if (line.Length < 1 ||
                line.StartsWith("--- ") ||
                line.StartsWith("index ") ||
                line.StartsWith("@@ ") ||
                line.StartsWith("new file mode") ||
                line.StartsWith("deleted file mode") ||
                line.StartsWith("similarity index") ||
                line.StartsWith("rename from") ||
                line.StartsWith("rename to"))
                continue;

            var sign = line[0];
            if (sign != '+' && sign != '-')
                continue;

            if (string.IsNullOrEmpty(currentFile))
                continue;

            var ext = Path.GetExtension(currentFile);
            if (string.IsNullOrEmpty(ext) || !allowed.Contains(ext))
                continue;

            if (line.StartsWith("+++ ") || line.StartsWith("--- "))
                continue;

            var content = line[1..];
            var contentTrimmed = content.TrimStart();
            if (contentTrimmed.StartsWith("Binary files", StringComparison.Ordinal) ||
                contentTrimmed.StartsWith("GIT binary", StringComparison.Ordinal))
                continue;

            if (!ContainsCodeToken(content, ref inBlockComment))
                continue;

            if (!dict.TryGetValue(currentFile, out var v)) v = (0, 0);
            switch (sign)
            {
                case '+':
                    v.added++;
                    break;
                case '-':
                    v.removed++;
                    break;
            }
            dict[currentFile] = v;
        }

        foreach (var file in touchedFiles)
        {
            if (!fileCommits.TryGetValue(file, out var list))
            {
                list = [];
                fileCommits[file] = list;
            }

            if (list.Count == 0 || list[^1].Hash != commit.Hash)
            {
                list.Add(commit);
            }
        }
    }

    private static bool ContainsCodeToken(string content, ref bool inBlockComment)
    {
        if (string.IsNullOrWhiteSpace(content))
            return false;

        var hasCode = false;
        var inString = false;
        var inChar = false;
        var inVerbatimString = false;

        for (var i = 0; i < content.Length; i++)
        {
            var ch = content[i];

            if (inBlockComment)
            {
                if (ch == '*' && i + 1 < content.Length && content[i + 1] == '/')
                {
                    inBlockComment = false;
                    i++;
                }

                continue;
            }

            if (inString)
            {
                if (inVerbatimString)
                {
                    if (ch == '"')
                    {
                        if (i + 1 < content.Length && content[i + 1] == '"')
                        {
                            i++;
                        }
                        else
                        {
                            inString = false;
                            inVerbatimString = false;
                        }
                    }

                    continue;
                }

                if (ch == '\\' && i + 1 < content.Length)
                {
                    i++;
                    continue;
                }

                if (ch == '"')
                {
                    inString = false;
                }

                continue;
            }

            if (inChar)
            {
                if (ch == '\\' && i + 1 < content.Length)
                {
                    i++;
                    continue;
                }

                if (ch == '\'')
                {
                    inChar = false;
                }

                continue;
            }

            if (char.IsWhiteSpace(ch))
                continue;

            if (ch == '/' && i + 1 < content.Length)
            {
                var next = content[i + 1];
                if (next == '/')
                {
                    break;
                }

                if (next == '*')
                {
                    inBlockComment = true;
                    i++;
                    continue;
                }
            }

            if (ch == '@' && i + 1 < content.Length && content[i + 1] == '"')
            {
                hasCode = true;
                inString = true;
                inVerbatimString = true;
                i++;
                continue;
            }

            if (ch == '"')
            {
                hasCode = true;
                inString = true;
                continue;
            }

            if (ch == '\'')
            {
                hasCode = true;
                inChar = true;
                continue;
            }

            hasCode = true;
        }

        return hasCode;
    }

    private static string ShortHash(string hash) => hash.Length <= 8 ? hash : hash[..8];

    private static List<CommitEntry> GetCommitsForAuthor(
        string repoPath,
        HashSet<string> authorMatchers,
        string since,
        string until)
    {
        var logArgs = new List<string> { "log", "--pretty=format:%H%x1f%an%x1f%ae%x1f%s" };
        if (!string.IsNullOrWhiteSpace(since))
            logArgs.Add($"--since={since}");
        if (!string.IsNullOrWhiteSpace(until))
            logArgs.Add($"--until={until}");

        var output = RunGit(logArgs, repoPath, out var exitCode);
        if (exitCode != 0 || string.IsNullOrWhiteSpace(output))
            return [];

        var commits = new List<CommitEntry>();
        foreach (var line in output.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries))
        {
            var parts = line.Split('\x1f');
            if (parts.Length < 4)
                continue;

            var hash = parts[0].Trim();
            var name = parts[1].Trim();
            var email = parts[2].Trim();
            var subject = parts[3].Trim();
            if (hash.Length == 0)
                continue;

            if (!MatchesAuthor(name, email, authorMatchers))
                continue;

            commits.Add(new CommitEntry(hash, name, email, subject));
        }

        return commits;
    }

    private static bool MatchesAuthor(string name, string email, HashSet<string> authorMatchers)
    {
        if (authorMatchers.Count == 0)
            return false;

        if (authorMatchers.Contains(name))
            return true;
        if (!string.IsNullOrWhiteSpace(email) && authorMatchers.Contains(email))
            return true;
        var full = string.IsNullOrWhiteSpace(email) ? name : $"{name} <{email}>";
        return authorMatchers.Contains(full);
    }

    private static List<HashSet<string>> LoadAliasGroups(string repoPath)
    {
        var path = Path.Combine(repoPath, ".loc-aliases");
        if (!File.Exists(path))
            return [];

        var groups = new List<HashSet<string>>();
        foreach (var rawLine in File.ReadLines(path))
        {
            var line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith('#'))
                continue;

            var parts = line.Split('|', 2, StringSplitOptions.TrimEntries);
            var canonical = parts[0].Trim();
            if (canonical.Length == 0)
                continue;

            var identities = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            identities.Add(canonical);

            if (parts.Length > 1 && parts[1].Length > 0)
            {
                foreach (var alias in parts[1].Split([','], StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
                {
                    if (alias.Length > 0)
                        identities.Add(alias);
                }
            }

            groups.Add(identities);
        }

        return groups;
    }

    private static HashSet<string> BuildAuthorMatchers(string name, string email, List<HashSet<string>> aliasGroups)
    {
        var matchers = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        if (!string.IsNullOrWhiteSpace(name))
            matchers.Add(name);
        if (!string.IsNullOrWhiteSpace(email))
            matchers.Add(email);
        if (!string.IsNullOrWhiteSpace(name) && !string.IsNullOrWhiteSpace(email))
            matchers.Add($"{name} <{email}>");

        if (aliasGroups.Count == 0)
            return matchers;

        var expanded = true;
        while (expanded)
        {
            expanded = false;
            foreach (var group in aliasGroups)
            {
                if (!group.Any(matchers.Contains))
                    continue;

                foreach (var identity in group)
                {
                    if (matchers.Add(identity))
                        expanded = true;
                }
            }
        }

        return matchers;
    }

    private static string RunGit(IEnumerable<string> argsEnumerable, string workingDir, out int exitCode)
    {
        var args = string.Join(" ", argsEnumerable.Select(a => a.Contains(" ") ? Quote(a) : a));
        var psi = new ProcessStartInfo
        {
            FileName = "git",
            Arguments = args,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
            WorkingDirectory = workingDir,
        };

        try
        {
            using var p = Process.Start(psi);
            if (p == null)
            {
                exitCode = -1;
                return "Failed to start git process.";
            }

            var output = p.StandardOutput.ReadToEnd();
            var error = p.StandardError.ReadToEnd();

            p.WaitForExit();
            exitCode = p.ExitCode;

            if (exitCode != 0 && string.IsNullOrWhiteSpace(output))
                return error;
            return output + (string.IsNullOrEmpty(error) ? "" : "\n" + error);
        }
        catch (Exception ex)
        {
            exitCode = -1;
            return ex.Message;
        }
    }
}
