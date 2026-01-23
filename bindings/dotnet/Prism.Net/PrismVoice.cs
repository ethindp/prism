namespace Prism.Net;

/// <summary>
/// Represents a voice provided by a speech backend.
/// </summary>
/// <param name="Index">The backend-specific index of the voice.</param>
/// <param name="Name">The display name of the voice.</param>
/// <param name="Language">The language tag of the voice.</param>
public readonly record struct PrismVoice(int Index, string Name, string Language);
