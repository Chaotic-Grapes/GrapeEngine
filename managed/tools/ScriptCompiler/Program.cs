using GrapeEngine.Scripting.Internal.Hosting;


// Expecting two arguments: ProjectRoot and OutputAssemblyPath
if (args.Length < 2)
{
    Console.WriteLine("Usage: ScriptCompiler <ProjectRoot> <OutputAssemblyPath>");
    return 1;
}

// Resolve full paths
var projectRoot = Path.GetFullPath(args[0]);
var outputAssemblyPath = Path.GetFullPath(args[1]);

// Validate project settings (stored in Documents/Grape Engine/<Project>/ProjectSettings.json)
var projectName = new DirectoryInfo(projectRoot).Name;
var documents = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
var settingsPath = Path.Combine(documents, "Grape Engine", projectName, "ProjectSettings.json");

if (!File.Exists(settingsPath))
{
    Console.Error.WriteLine($"ProjectSettings.json not found in {settingsPath}");
    return 1;
}

// Create output directory if it doesn't exist
Directory.CreateDirectory(Path.GetDirectoryName(outputAssemblyPath) ?? ".");

// Gather all .cs files excluding bin and obj directories
var csFiles = Directory.GetFiles(projectRoot, "*.cs", SearchOption.AllDirectories)
    .Where(path => !path.Contains($"{Path.DirectorySeparatorChar}bin{Path.DirectorySeparatorChar}") &&
                   !path.Contains($"{Path.DirectorySeparatorChar}obj{Path.DirectorySeparatorChar}"))
    .ToArray();

// Log information
Console.WriteLine($"Compiling scripts from: {projectRoot}");
Console.WriteLine($"Found C# files: {csFiles.Length}");
Console.WriteLine($"Output assembly: {outputAssemblyPath}");

// Compile scripts
var result = ScriptHost.CompileScriptsManaged(projectRoot, outputAssemblyPath);
if (result != 0)
{
    Console.Error.WriteLine("Script compilation failed.");
    return 1;
}

if (!File.Exists(outputAssemblyPath))
{
    Console.Error.WriteLine("Script compilation reported success but output file was not created.");
    return 1;
}

Console.WriteLine("Script compilation completed.");
return 0;
