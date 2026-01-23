# Prism.Net

High-performance .NET bindings for the [Prism speech library](https://github.com/ethindp/prism).

## Usage

Initialize a `PrismContext` at application startup. Use the context to acquire backends, either by requesting the highest-priority one or selecting a specific driver.

```csharp
using Prism.Net;

// Initialize context, keep this around and dispose it when done.
using var context = new PrismContext();

// Acquire the current best backend.
using var backend = context.AcquireBestBackend();

// Output to speech, braille, or both
backend.Speak("Speech only", interrupt: false);
backend.Braille("Braille only");
backend.Output("Both speech and braille", interrupt: true);
```

## Advanced Voice and Backend Selection

Drivers and voices are exposed as enumerables, making them easy to query using LINQ.

```csharp
// Find an English voice with specific criteria
var englishVoice = backend.Voices
    .FirstOrDefault(v => v.Language.StartsWith("en") && v.Name.Contains("Hazel"));

if (englishVoice != default)
{
    backend.CurrentVoice = englishVoice;
}

// Adjust speech parameters (volume/rate/pitch are null if not supported by driver)
if (backend.Rate.HasValue) backend.Rate = 0.75f;
if (backend.Volume.HasValue) backend.Volume = 1.0f;
```

## Audio Synthesis

Some backends support direct synthesis to memory.

```csharp
// Synthesize text and stream 32-bit interleaved float PCM data
foreach (var chunk in backend.SpeakToMemory("Hello from the memory backend"))
{
    // Process audio chunk (ReadOnlyMemory<float>)
    Console.WriteLine($"Received {chunk.Length} samples");
}
```

## Manual Backend Management

You can inspect all registered backends to verify support or manually select preferred implementations using LINQ.

```csharp
// Enumerate available drivers
var drivers = context.AvailableBackends
    .OrderByDescending(b => b.Priority);

foreach (var driver in drivers)
{
    Console.WriteLine($"[{driver.Id:X}] {driver.Name} (Supported: {driver.IsSupported})");
}

// Select a backend using LINQ (e.g., find the SAPI driver)
var sapi = context.AvailableBackends.FirstOrDefault(b => b.Name == "SAPI");
if (sapi != default)
{
    // Acquire reuses a cached native instance if one exists
    using var backend = context.AcquireBackend(sapi);
}

// or create a unique instance bypassing the internal cache.
// Use this for independent state (voice, rate, pitch) from the rest of the application.
using var unique = context.CreateBestBackend();

// You can also create a specific backend manually
```
