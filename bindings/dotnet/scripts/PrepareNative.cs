// This will download the latest Prism SDK from GitHub releases, extract the native binaries for each supported platform, and stage them so that on the next `dotnet pack` they will be included properly in the NuGet package.

using System.IO.Compression;
using System.Net.Http.Headers;
using System.Net.Http.Json;
using System.Text.Json.Serialization;

const string GithubRepo = "ethindp/prism";
const string TargetProject = "Prism.Net";

(string SdkPath, string Rid)[] Rids = [
    ("windows/x64/release/bin",   "win-x64"),
    ("windows/arm64/release/bin", "win-arm64"),
    ("linux/x64/release/lib",     "linux-x64"),
    ("linux/arm64/release/lib",   "linux-arm64"),
    ("macos/universal/release/lib", "osx-x64"),
    ("macos/universal/release/lib", "osx-arm64")
];

var rootDir = FindProjectRoot(Directory.GetCurrentDirectory());
var stagingDir = Path.Combine(rootDir, "bindings", "dotnet", TargetProject, "staging", "runtimes");

Console.WriteLine($"""
    Target: {TargetProject}
    Repo:   {GithubRepo}
    Output: {stagingDir}
    """);

using var http = new HttpClient();
http.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("Prism-Stager", "1.0"));

var release = await http.GetFromJsonAsync($"https://api.github.com/repos/{GithubRepo}/releases/latest", ScriptContext.Default.GitHubRelease)
    ?? throw new InvalidOperationException("Could not retrieve latest release info.");

var sdkAsset = release.Assets.FirstOrDefault(a => a.Name.Contains("prism-sdk-") && a.Name.EndsWith(".zip"))
    ?? throw new FileNotFoundException("Could not find prism-sdk-*.zip in the latest release.");

Console.WriteLine($"Found: {sdkAsset.Name} ({release.TagName})");

var stagingRoot = Path.Combine(rootDir, "bindings", "dotnet", TargetProject, "staging");
var versionFile = Path.Combine(stagingRoot, "version.txt");

if (File.Exists(versionFile) && File.ReadAllText(versionFile).Trim() == release.TagName && Directory.Exists(stagingDir))
{
    Console.WriteLine($"Already up to date ({release.TagName}). Skipping.");
    return;
}

var tempZip = Path.GetTempFileName();
var extractDir = Path.Combine(Path.GetTempPath(), $"prism-sdk-{Guid.NewGuid()}");

try
{
    Console.WriteLine($"Downloading...");
    using (var stream = await http.GetStreamAsync(sdkAsset.DownloadUrl))
    using (var fs = File.Create(tempZip))
    {
        await stream.CopyToAsync(fs);
    }

    Console.WriteLine($"Extracting to temporary folder...");
    ZipFile.ExtractToDirectory(tempZip, extractDir);

    var actualSdkRoot = Directory.EnumerateDirectories(extractDir).FirstOrDefault()
        ?? throw new DirectoryNotFoundException("SDK zip did not contain the expected root folder.");

    if (Directory.Exists(stagingDir))
    {
        Directory.Delete(stagingDir, true);
    }

    foreach (var (sdkPath, rid) in Rids)
    {
        var sourceDir = Path.Combine(actualSdkRoot, sdkPath);
        var destDir = Path.Combine(stagingDir, rid, "native");

        if (Directory.Exists(sourceDir))
        {
            Directory.CreateDirectory(destDir);
            foreach (var file in Directory.EnumerateFiles(sourceDir))
            {
                var targetFile = Path.Combine(destDir, Path.GetFileName(file));
                File.Copy(file, targetFile, true);
            }
        }
        else
        {
            Console.WriteLine($"SDK path not found: {sdkPath}");
        }
    }

    File.WriteAllText(versionFile, release.TagName);
    Console.WriteLine("Native binaries successfully staged!");
}
finally
{
    if (File.Exists(tempZip)) File.Delete(tempZip);
    if (Directory.Exists(extractDir)) Directory.Delete(extractDir, true);
}

static string FindProjectRoot(string startDir)
{
    var curr = new DirectoryInfo(startDir);
    while (curr != null && !File.Exists(Path.Combine(curr.FullName, "CMakeLists.txt")))
        curr = curr.Parent;
    return curr?.FullName ?? throw new Exception("Could not find project root by looking for CMakeLists.txt");
}

record GitHubRelease(
    [property: JsonPropertyName("tag_name")] string TagName,
    [property: JsonPropertyName("assets")] GitHubAsset[] Assets
);

record GitHubAsset(
    [property: JsonPropertyName("name")] string Name,
    [property: JsonPropertyName("browser_download_url")] string DownloadUrl
);

[JsonSerializable(typeof(GitHubRelease))]
internal partial class ScriptContext : JsonSerializerContext { }
