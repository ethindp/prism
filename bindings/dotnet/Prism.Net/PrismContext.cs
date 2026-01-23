using System.Runtime.InteropServices;
using Prism.Net.NativeInterop;

namespace Prism.Net;

/// <summary>
/// Represents a Prism context through which backends are acquired and managed.
/// </summary>
public sealed unsafe class PrismContext : IDisposable
{
    private readonly NativeInterop.PrismContext* _handle;
    private readonly object _lock = new();

    /// <summary>
    /// Initializes a new instance of the <see cref="PrismContext"/> class.
    /// </summary>
    /// <param name="platformHandle">Optional handle for backends that require it (HWND on Windows, JNIEnv* on Android).</param>
    public PrismContext(IntPtr platformHandle = default)
    {
        if (platformHandle != IntPtr.Zero)
        {
            var config = new PrismConfig
            {
                version = 1,
                platformPointer = platformHandle
            };
            _handle = Methods.prism_init(&config);
        }
        else
        {
            _handle = Methods.prism_init(null);
        }

        if (_handle is null)
        {
            throw new PrismException(PrismError.PRISM_ERROR_INTERNAL);
        }
    }

    /// <summary>
    /// Gets a list of all backends registered with Prism and their current status.
    /// </summary>
    public IReadOnlyList<PrismBackendInfo> AvailableBackends
    {
        get
        {
            lock (_lock)
            {
                if (_handle is null) return [];
                var count = Methods.prism_registry_count(_handle);
                var backends = new List<PrismBackendInfo>((int)count);
                for (nuint i = 0; i < count; i++)
                {
                    var id = Methods.prism_registry_id_at(_handle, i);
                    var namePtr = Methods.prism_registry_name(_handle, id);
                    var name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "Unknown";
                    var priority = Methods.prism_registry_priority(_handle, id);
                    var isSupported = Methods.prism_registry_exists(_handle, id);

                    backends.Add(new PrismBackendInfo(id, name, priority, isSupported));
                }
                return backends;
            }
        }
    }

    /// <summary>
    /// Acquires the highest-priority backend that successfully initializes, reusing a cached instance if available.
    /// </summary>
    /// <returns>An initialized backend instance.</returns>
    /// <remarks>
    /// This method uses the internal Prism registry cache. Multiple calls to this method (or <see cref="AcquireBackend"/>) 
    /// for the same backend ID will return wrappers pointing to the same native resource. 
    /// </remarks>
    /// <exception cref="PrismException">Thrown if no backends are available.</exception>
    public IPrismBackend AcquireBestBackend()
    {
        lock (_lock)
        {
            var backendPtr = Methods.prism_registry_acquire_best(_handle);
            if (backendPtr is null)
            {
                throw new PrismException(PrismError.PRISM_ERROR_BACKEND_NOT_AVAILABLE);
            }
            return new PrismBackend(backendPtr);
        }
    }

    /// <summary>
    /// Creates a fresh instance of the highest-priority backend, bypassing the cache.
    /// </summary>
    /// <returns>A new initialized backend instance.</returns>
    /// <remarks>
    /// Unlike <see cref="AcquireBestBackend"/>, this creates a unique native instance. 
    /// </remarks>
    public IPrismBackend CreateBestBackend()
    {
        lock (_lock)
        {
            var backendPtr = Methods.prism_registry_create_best(_handle);
            if (backendPtr is null)
            {
                throw new PrismException(PrismError.PRISM_ERROR_BACKEND_NOT_AVAILABLE);
            }
            return new PrismBackend(backendPtr);
        }
    }

    /// <summary>
    /// Acquires a backend instance, reusing a cached instance if available or creating a new one otherwise.
    /// </summary>
    /// <param name="info">The info of the backend to acquire.</param>
    /// <returns>A backend instance (either existing or newly created).</returns>
    public IPrismBackend AcquireBackend(PrismBackendInfo info) => AcquireBackend(info.Id);

    /// <summary>
    /// Acquires a backend instance, reusing a cached instance if available or creating a new one otherwise.
    /// </summary>
    /// <param name="backendId">The ID of the backend to acquire.</param>
    /// <returns>A backend instance (either existing or newly created).</returns>
    /// <remarks>
    /// This method uses the internal Prism registry cache. Multiple calls for the same ID will return wrappers 
    /// sharing the same native instance.
    /// </remarks>
    /// <exception cref="PrismException">Thrown if the specified backend is not available.</exception>
    public IPrismBackend AcquireBackend(ulong backendId)
    {
        lock (_lock)
        {
            var backendPtr = Methods.prism_registry_acquire(_handle, backendId);
            if (backendPtr is null)
            {
                throw new PrismException(PrismError.PRISM_ERROR_BACKEND_NOT_AVAILABLE);
            }
            return new PrismBackend(backendPtr);
        }
    }

    /// <summary>
    /// Creates a fresh instance of a backend, bypassing the cache.
    /// </summary>
    /// <param name="backendId">The ID of the backend to create.</param>
    /// <returns>A new initialized backend instance.</returns>
    /// <remarks>
    /// Each call creates a unique native instance with its own state. 
    /// Use this if you need independent voice/rate/pitch state from the rest of the application.
    /// </remarks>
    public IPrismBackend CreateBackend(ulong backendId)
    {
        lock (_lock)
        {
            var backendPtr = Methods.prism_registry_create(_handle, backendId);
            if (backendPtr is null)
            {
                throw new PrismException(PrismError.PRISM_ERROR_BACKEND_NOT_AVAILABLE);
            }
            return new PrismBackend(backendPtr);
        }
    }

    /// <summary>
    /// Disposes the context and releases all associated resources.
    /// </summary>
    /// <remarks>
    /// This does not automatically dispose backend instances that were obtained from the context. If any backends obtained through this context stay alive when the context is disposed, those backends remain valid and may continue to be used.
    /// </remarks>
    public void Dispose()
    {
        lock (_lock)
        {
            if (_handle is not null)
            {
                Methods.prism_shutdown(_handle);
            }
        }
    }
}
