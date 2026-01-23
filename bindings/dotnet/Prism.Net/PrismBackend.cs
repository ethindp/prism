using System.Runtime.InteropServices;
using System.Collections.Concurrent;
using System.Buffers;
using Prism.Net.NativeInterop;

namespace Prism.Net;

internal sealed unsafe class PrismBackend : IPrismBackend
{
    private static readonly ConcurrentDictionary<IntPtr, object> _locks = new();
    private readonly NativeInterop.PrismBackend* _handle;
    private readonly object _lock;

    public PrismBackend(NativeInterop.PrismBackend* handle)
    {
        _handle = handle;
        _lock = _locks.GetOrAdd((IntPtr)handle, _ => new object());

        lock (_lock)
        {
            var err = Methods.prism_backend_initialize(_handle);
            if (err is not PrismError.PRISM_OK and not PrismError.PRISM_ERROR_ALREADY_INITIALIZED)
            {
                throw new PrismException(err);
            }
        }
    }

    public string Name
    {
        get
        {
            lock (_lock)
            {
                return Marshal.PtrToStringUTF8((IntPtr)Methods.prism_backend_name(_handle)) ?? "Unknown";
            }
        }
    }

    public int Channels
    {
        get
        {
            lock (_lock)
            {
                nuint channels;
                PrismException.ThrowIfError(Methods.prism_backend_get_channels(_handle, &channels));
                return (int)channels;
            }
        }
    }

    public int SampleRate
    {
        get
        {
            lock (_lock)
            {
                nuint sampleRate;
                PrismException.ThrowIfError(Methods.prism_backend_get_sample_rate(_handle, &sampleRate));
                return (int)sampleRate;
            }
        }
    }

    public float? Volume
    {
        get
        {
            lock (_lock)
            {
                float volume;
                var err = Methods.prism_backend_get_volume(_handle, &volume);
                if (err is PrismError.PRISM_ERROR_NOT_IMPLEMENTED) return null;
                PrismException.ThrowIfError(err);
                return volume;
            }
        }
        set
        {
            if (value is float v)
            {
                lock (_lock)
                {
                    var err = Methods.prism_backend_set_volume(_handle, v);
                    if (err is not PrismError.PRISM_ERROR_NOT_IMPLEMENTED) PrismException.ThrowIfError(err);
                }
            }
        }
    }

    public float? Rate
    {
        get
        {
            lock (_lock)
            {
                float rate;
                var err = Methods.prism_backend_get_rate(_handle, &rate);
                if (err is PrismError.PRISM_ERROR_NOT_IMPLEMENTED) return null;
                PrismException.ThrowIfError(err);
                return rate;
            }
        }
        set
        {
            if (value is float v)
            {
                lock (_lock)
                {
                    var err = Methods.prism_backend_set_rate(_handle, v);
                    if (err is not PrismError.PRISM_ERROR_NOT_IMPLEMENTED) PrismException.ThrowIfError(err);
                }
            }
        }
    }

    public float? Pitch
    {
        get
        {
            lock (_lock)
            {
                float pitch;
                var err = Methods.prism_backend_get_pitch(_handle, &pitch);
                if (err is PrismError.PRISM_ERROR_NOT_IMPLEMENTED) return null;
                PrismException.ThrowIfError(err);
                return pitch;
            }
        }
        set
        {
            if (value is float v)
            {
                lock (_lock)
                {
                    var err = Methods.prism_backend_set_pitch(_handle, v);
                    if (err is not PrismError.PRISM_ERROR_NOT_IMPLEMENTED) PrismException.ThrowIfError(err);
                }
            }
        }
    }

    public bool IsSpeaking
    {
        get
        {
            lock (_lock)
            {
                bool speaking;
                var err = Methods.prism_backend_is_speaking(_handle, &speaking);
                if (err is PrismError.PRISM_ERROR_NOT_IMPLEMENTED) return false;
                PrismException.ThrowIfError(err);
                return speaking;
            }
        }
    }

    public IEnumerable<PrismVoice> Voices
    {
        get
        {
            nuint count;
            lock (_lock)
            {
                var refreshErr = Methods.prism_backend_refresh_voices(_handle);
                if (refreshErr is PrismError.PRISM_ERROR_NOT_IMPLEMENTED) return Enumerable.Empty<PrismVoice>();
                PrismException.ThrowIfError(refreshErr);

                nuint internalCount;
                var countErr = Methods.prism_backend_count_voices(_handle, &internalCount);
                if (countErr is PrismError.PRISM_ERROR_NOT_IMPLEMENTED) return Enumerable.Empty<PrismVoice>();
                PrismException.ThrowIfError(countErr);
                count = internalCount;
            }

            return IterateVoices(count);
        }
    }

    private IEnumerable<PrismVoice> IterateVoices(nuint count)
    {
        for (nuint i = 0; i < count; i++)
        {
            yield return GetVoice(i);
        }
    }

    private PrismVoice GetVoice(nuint index)
    {
        string name;
        string lang;

        lock (_lock)
        {
            sbyte* namePtr;
            sbyte* langPtr;
            PrismException.ThrowIfError(Methods.prism_backend_get_voice_name(_handle, index, &namePtr));
            PrismException.ThrowIfError(Methods.prism_backend_get_voice_language(_handle, index, &langPtr));
            name = Marshal.PtrToStringUTF8((IntPtr)namePtr) ?? "Unknown";
            lang = Marshal.PtrToStringUTF8((IntPtr)langPtr) ?? "Unknown";
        }

        return new PrismVoice((int)index, name, lang);
    }

    public PrismVoice? CurrentVoice
    {
        get
        {
            lock (_lock)
            {
                nuint voiceId;
                var err = Methods.prism_backend_get_voice(_handle, &voiceId);
                if (err is PrismError.PRISM_ERROR_NO_VOICES or PrismError.PRISM_ERROR_NOT_IMPLEMENTED) return null;
                PrismException.ThrowIfError(err);

                return GetVoice(voiceId);
            }
        }
        set
        {
            if (value is { } voice)
            {
                lock (_lock)
                {
                    var err = Methods.prism_backend_set_voice(_handle, (nuint)voice.Index);
                    if (err is not PrismError.PRISM_ERROR_NOT_IMPLEMENTED) PrismException.ThrowIfError(err);
                }
            }
        }
    }

    public void Speak(string text, bool interrupt = true)
    {
        using var utf8 = new Utf8String(text);
        lock (_lock)
        {
            PrismException.ThrowIfError(Methods.prism_backend_speak(_handle, utf8.Pointer, interrupt));
        }
    }

    public IEnumerable<ReadOnlyMemory<float>> SpeakToMemory(string text)
    {
        var collection = new BlockingCollection<(float[] Buffer, int Count)>(boundedCapacity: 64);
        var state = new MemorySpeechState(collection);
        var stateHandle = GCHandle.Alloc(state);

        _ = Task.Run(() =>
        {
            try
            {
                using var utf8 = new Utf8String(text);
                PrismError err;
                lock (_lock)
                {
                    unsafe
                    {
                        err = Methods.prism_backend_speak_to_memory(
                            _handle,
                            utf8.Pointer,
                            &OnAudioAvailable,
                            (void*)GCHandle.ToIntPtr(stateHandle)
                        );
                    }
                }

                if (err != PrismError.PRISM_OK)
                {
                    state.Exception = new PrismException(err);
                }
            }
            catch (Exception ex)
            {
                state.Exception = ex;
            }
            finally
            {
                collection.CompleteAdding();
                stateHandle.Free();
            }
        });

        foreach (var (buffer, countItems) in collection.GetConsumingEnumerable())
        {
            yield return new ReadOnlyMemory<float>(buffer, 0, countItems);
            ArrayPool<float>.Shared.Return(buffer);
        }

        if (state.Exception is not null)
        {
            throw state.Exception;
        }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
    private static void OnAudioAvailable(void* userdata, float* samples, nuint sampleCount, nuint channels, nuint sampleRate)
    {
        var state = (MemorySpeechState)GCHandle.FromIntPtr((IntPtr)userdata).Target!;
        if (sampleCount == 0) return;

        float[] buffer = ArrayPool<float>.Shared.Rent((int)sampleCount);
        fixed (float* pBuffer = buffer)
        {
            Buffer.MemoryCopy(samples, pBuffer, (long)buffer.Length * sizeof(float), (long)sampleCount * sizeof(float));
        }

        try
        {
            state.Collection.Add((buffer, (int)sampleCount));
        }
        catch (InvalidOperationException)
        {
            ArrayPool<float>.Shared.Return(buffer);
        }
    }

    private class MemorySpeechState(BlockingCollection<(float[] Buffer, int Count)> collection)
    {
        public readonly BlockingCollection<(float[] Buffer, int Count)> Collection = collection;
        public Exception? Exception { get; set; }
    }

    public void Braille(string text)
    {
        using var utf8 = new Utf8String(text);
        lock (_lock)
        {
            PrismException.ThrowIfError(Methods.prism_backend_braille(_handle, utf8.Pointer));
        }
    }

    public void Output(string text, bool interrupt = true)
    {
        using var utf8 = new Utf8String(text);
        lock (_lock)
        {
            PrismException.ThrowIfError(Methods.prism_backend_output(_handle, utf8.Pointer, interrupt));
        }
    }

    public void Stop()
    {
        lock (_lock)
        {
            PrismException.ThrowIfError(Methods.prism_backend_stop(_handle));
        }
    }

    public void Pause()
    {
        lock (_lock)
        {
            PrismException.ThrowIfError(Methods.prism_backend_pause(_handle));
        }
    }

    public void Resume()
    {
        lock (_lock)
        {
            PrismException.ThrowIfError(Methods.prism_backend_resume(_handle));
        }
    }

    public void Dispose()
    {
        lock (_lock)
        {
            Methods.prism_backend_free(_handle);
        }
    }
}

internal readonly unsafe ref struct Utf8String : IDisposable
{
    public sbyte* Pointer { get; }

    public Utf8String(string? text)
    {
        if (text is null)
        {
            Pointer = null;
            return;
        }

        IntPtr ptr = Marshal.StringToCoTaskMemUTF8(text);
        Pointer = (sbyte*)ptr;
    }

    public void Dispose()
    {
        if (Pointer is not null)
        {
            Marshal.FreeCoTaskMem((IntPtr)Pointer);
        }
    }
}
