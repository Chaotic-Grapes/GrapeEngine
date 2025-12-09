#:package Terminal.Gui@1.19.0

using System.Diagnostics;
using Terminal.Gui;

namespace GitTrackLoc;

class Program
{
    private readonly record struct Contributor(int Commits, string Name, string Email);

    private static string Quote(string s) => '"' + s.Replace("\"", "\\\"") + '"';

    static int Main(string[] args)
    {
        Application.Init();
        var repoPath = Directory.GetCurrentDirectory();

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

        var filesList = new ListView(new List<string>()) { X = 0, Y = Pos.Bottom(totalsLabel) + 1, Width = Dim.Fill(), Height = Dim.Fill() };

        // current per-file state and inclusion flags (reset on load)
        Dictionary<string, (long added, long removed)> currentPerFile = new();
        List<string> orderedFiles = [];
        Dictionary<string, bool> included = new(StringComparer.OrdinalIgnoreCase);

        right.Add(sinceLabel, sinceField, untilLabel, untilField, pathLabel, pathField, loadBtn, totalsLabel, filesList);

        win.Add(left, right);
        top.Add(status, win);

        loadBtn.Clicked += async () => await ShowContributionsForSelectedAsync();

        // double-click or Enter to load contributor
        listView.OpenSelectedItem += async _ => await ShowContributionsForSelectedAsync();

        // clicking a file toggles inclusion for totals
        filesList.OpenSelectedItem += _ => ToggleIncludeForSelected();

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
                items.Add($"{mark} {v.added,6} +  {v.removed,6} -  {f}{suffix}");
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

        async Task ShowContributionsForSelectedAsync()
        {
            if (listView.SelectedItem < 0 || listView.SelectedItem >= contributors.Count) return;
            var c = contributors[listView.SelectedItem];
            var since = sinceField.Text.ToString() ?? string.Empty;
            var until = untilField.Text.ToString() ?? string.Empty;
            var path = pathField.Text.ToString() ?? string.Empty;

            // update UI to show loading and disable controls
            status.Text = $"Loading contributions for {c.Name}...";
            loadBtn.Enabled = false;
            listView.Enabled = false;
            Application.Refresh();

            // run the expensive calculation on a thread-pool thread
            var perFile = await Task.Run(() => CalculateLocForAuthor(repoPath, c.Name, c.Email, since, until, path));

            // prepare display state: set currentPerFile, orderedFiles, and included flags
            currentPerFile = perFile;
            orderedFiles = perFile.OrderByDescending(kv => kv.Value.added - kv.Value.removed).Select(kv => kv.Key).ToList();
            included.Clear();
            foreach (var f in orderedFiles) included[f] = true; // default: count towards total

            // marshal UI updates back to the Terminal.Gui main loop
            Application.MainLoop.Invoke(() =>
            {
                UpdateFilesListUI();
                status.Text = $"Loaded for {c.Name} — files: {currentPerFile.Count}";
                loadBtn.Enabled = true;
                listView.Enabled = true;
                Application.Refresh();
            });
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

    private static Dictionary<string, (long added, long removed)> CalculateLocForAuthor(string repoPath, string name, string email, string since, string until, string pathspec)
    {
        var allowed = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { ".c", ".cpp", ".h", ".hpp", ".cs" };
        var result = new Dictionary<string, (long added, long removed)>();

        var authorSpec = string.IsNullOrWhiteSpace(email) ? name : $"{name} <{email}>";

        var logArgs = new List<string> { "log", "--pretty=format:%H", $"--author={authorSpec}" };

        if (!string.IsNullOrWhiteSpace(since))
            logArgs.Add($"--since=\"{since}\"");
        if (!string.IsNullOrWhiteSpace(until))
            logArgs.Add($"--until=\"{until}\"");

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

        var commitsOut = RunGit(logArgs, repoPath, out var logExit);
        if (logExit != 0) return result;

        var commits = commitsOut.Split(['\r', '\n'], StringSplitOptions.RemoveEmptyEntries).Select(s => s.Trim()).Where(s => s.Length > 0).ToList();

        foreach (var showArgs in commits.Select(
                     commit => new List<string> { "show", "--pretty=format:", "--unified=0", commit }))
        {
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
            if (showExit != 0)
                continue;

            ParseDiffPerFile(diff, result, allowed);
        }

        return result;
    }

    private static void ParseDiffPerFile(string diff, Dictionary<string, (long added, long removed)> dict, HashSet<string> allowed)
    {
        if (string.IsNullOrEmpty(diff)) return;

        var currentFile = string.Empty;
        var inBlockComment = false;

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

            var content = line[1..].Trim();
            if (string.IsNullOrWhiteSpace(content))
                continue;
            if (content.StartsWith("Binary files") || content.StartsWith("GIT binary"))
                continue;

            if (!inBlockComment)
            {
                if (content.StartsWith("/*"))
                {
                    if (!content.Contains("*/"))
                        inBlockComment = true;

                    continue;
                }
            }
            else
            {
                if (content.Contains("*/"))
                    inBlockComment = false;

                continue;
            }

            if (content.StartsWith("//") || content.StartsWith("#") || content.StartsWith("--"))
                continue;
            if (content.TrimStart().StartsWith("*") && content.Trim().Length <= 3)
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
