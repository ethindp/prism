namespace Prism.Net;

/// <summary>
/// Provides information about a registered Prism backend.
/// </summary>
/// <param name="Id">The unique ID of the backend.</param>
/// <param name="Name">The human-readable name of the backend.</param>
/// <param name="Priority">The backend's priority (higher is more preferred).</param>
/// <param name="IsSupported">True if the backend is supported on the current platform.</param>
public readonly record struct PrismBackendInfo(ulong Id, string Name, int Priority, bool IsSupported);
