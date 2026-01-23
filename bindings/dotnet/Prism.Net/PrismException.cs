using System.Runtime.InteropServices;
using Prism.Net.NativeInterop;

namespace Prism.Net;

/// <summary>
/// Represents errors that occur within the Prism library.
/// </summary>
public sealed unsafe class PrismException : Exception
{
    /// <summary>
    /// Gets the native error code.
    /// </summary>
    public PrismError Error { get; }

    internal PrismException(PrismError error) : base(GetErrorMessage(error))
    {
        Error = error;
    }

    private static string GetErrorMessage(PrismError error)
    {
        var ptr = Methods.prism_error_string(error);
        return Marshal.PtrToStringUTF8((IntPtr)ptr) ?? $"Prism operation failed with error: {error}";
    }

    internal static void ThrowIfError(PrismError error)
    {
        if (error is not PrismError.PRISM_OK)
        {
            throw new PrismException(error);
        }
    }
}
