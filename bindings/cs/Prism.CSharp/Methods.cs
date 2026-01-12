using System.Runtime.InteropServices;

namespace Prism.NativeInterop
{
    public static unsafe partial class Methods
    {
        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismConfig prism_config_init();

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismContext* prism_init(PrismConfig* cfg);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial void prism_shutdown(PrismContext* ctx);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        [return: NativeTypeName("size_t")]
        public static partial nuint prism_registry_count(PrismContext* ctx);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        [return: NativeTypeName("PrismBackendId")]
        public static partial ulong prism_registry_id_at(PrismContext* ctx, [NativeTypeName("size_t")] nuint index);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        [return: NativeTypeName("PrismBackendId")]
        public static partial ulong prism_registry_id(PrismContext* ctx, [NativeTypeName("const char *")] sbyte* name);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        [return: NativeTypeName("const char *")]
        public static partial sbyte* prism_registry_name(PrismContext* ctx, [NativeTypeName("PrismBackendId")] ulong id);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial int prism_registry_priority(PrismContext* ctx, [NativeTypeName("PrismBackendId")] ulong id);

[LibraryImport("prism")]
[UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
[return: MarshalAs(UnmanagedType.U1)]
public static partial bool prism_registry_exists(PrismContext* ctx, [NativeTypeName("PrismBackendId")] ulong id);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismBackend* prism_registry_get(PrismContext* ctx, [NativeTypeName("PrismBackendId")] ulong id);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismBackend* prism_registry_create(PrismContext* ctx, [NativeTypeName("PrismBackendId")] ulong id);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismBackend* prism_registry_create_best(PrismContext* ctx);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismBackend* prism_registry_acquire(PrismContext* ctx, [NativeTypeName("PrismBackendId")] ulong id);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismBackend* prism_registry_acquire_best(PrismContext* ctx);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial void prism_backend_free(PrismBackend* backend);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        [return: NativeTypeName("const char *")]
        public static partial sbyte* prism_backend_name(PrismBackend* backend);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_initialize(PrismBackend* backend);

[LibraryImport("prism")]
[UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
public static partial PrismError prism_backend_speak(PrismBackend* backend, [NativeTypeName("const char *")] sbyte* text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_speak_to_memory(PrismBackend* backend, [NativeTypeName("const char *")] sbyte* text, [NativeTypeName("PrismAudioCallback")] delegate* unmanaged[Cdecl]<void*, float*, nuint, nuint, nuint, void> callback, void* userdata);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_braille(PrismBackend* backend, [NativeTypeName("const char *")] sbyte* text);

[LibraryImport("prism")]
[UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
public static partial PrismError prism_backend_output(PrismBackend* backend, [NativeTypeName("const char *")] sbyte* text, [MarshalAs(UnmanagedType.U1)] bool interrupt);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_stop(PrismBackend* backend);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_pause(PrismBackend* backend);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_resume(PrismBackend* backend);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_is_speaking(PrismBackend* backend, bool* out_speaking);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_set_volume(PrismBackend* backend, float volume);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_set_rate(PrismBackend* backend, float rate);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_set_pitch(PrismBackend* backend, float pitch);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_volume(PrismBackend* backend, float* out_volume);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_rate(PrismBackend* backend, float* out_rate);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_pitch(PrismBackend* backend, float* out_pitch);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_refresh_voices(PrismBackend* backend);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_count_voices(PrismBackend* backend, [NativeTypeName("size_t *")] nuint* out_count);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_voice_name(PrismBackend* backend, [NativeTypeName("size_t")] nuint voice_id, [NativeTypeName("const char **")] sbyte** out_name);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_voice_language(PrismBackend* backend, [NativeTypeName("size_t")] nuint voice_id, [NativeTypeName("const char **")] sbyte** out_language);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_set_voice(PrismBackend* backend, [NativeTypeName("size_t")] nuint voice_id);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_voice(PrismBackend* backend, [NativeTypeName("size_t *")] nuint* out_voice_id);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_channels(PrismBackend* backend, [NativeTypeName("size_t *")] nuint* out_channels);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_sample_rate(PrismBackend* backend, [NativeTypeName("size_t *")] nuint* out_sample_rate);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        public static partial PrismError prism_backend_get_bit_depth(PrismBackend* backend, [NativeTypeName("size_t *")] nuint* out_bit_depth);

        [LibraryImport("prism")]
        [UnmanagedCallConv(CallConvs = [typeof(System.Runtime.CompilerServices.CallConvCdecl)])]
        [return: NativeTypeName("const char *")]
        public static partial sbyte* prism_error_string(PrismError error);

        [NativeTypeName("#define PRISM_BACKEND_INVALID UINT64_C(0)")]
        public const ulong PRISM_BACKEND_INVALID = (0UL);

        [NativeTypeName("#define PRISM_BACKEND_SAPI UINT64_C(0x1D6DF72422CEEE66)")]
        public const ulong PRISM_BACKEND_SAPI = (0x1D6DF72422CEEE66UL);

        [NativeTypeName("#define PRISM_BACKEND_AV_SPEECH UINT64_C(0x28E3429577805C24)")]
        public const ulong PRISM_BACKEND_AV_SPEECH = (0x28E3429577805C24UL);

        [NativeTypeName("#define PRISM_BACKEND_VOICE_OVER UINT64_C(0xCB4897961A754BCB)")]
        public const ulong PRISM_BACKEND_VOICE_OVER = (0xCB4897961A754BCBUL);

        [NativeTypeName("#define PRISM_BACKEND_SPEECH_DISPATCHER UINT64_C(0xE3D6F895D949EBFE)")]
        public const ulong PRISM_BACKEND_SPEECH_DISPATCHER = (0xE3D6F895D949EBFEUL);

        [NativeTypeName("#define PRISM_BACKEND_NVDA UINT64_C(0x89CC19C5C4AC1A56)")]
        public const ulong PRISM_BACKEND_NVDA = (0x89CC19C5C4AC1A56UL);

        [NativeTypeName("#define PRISM_BACKEND_JAWS UINT64_C(0xAC3D60E9BD84B53E)")]
        public const ulong PRISM_BACKEND_JAWS = (0xAC3D60E9BD84B53EUL);

        [NativeTypeName("#define PRISM_BACKEND_ONE_CORE UINT64_C(0x6797D32F0D994CB4)")]
        public const ulong PRISM_BACKEND_ONE_CORE = (0x6797D32F0D994CB4UL);

        [NativeTypeName("#define PRISM_BACKEND_ORCA UINT64_C(0x10AA1FC05A17F96C)")]
        public const ulong PRISM_BACKEND_ORCA = (0x10AA1FC05A17F96CUL);

        [NativeTypeName("#define PRISM_BACKEND_ANDROID_SCREEN_READER UINT64_C(0xD199C175AEEC494B)")]
        public const ulong PRISM_BACKEND_ANDROID_SCREEN_READER = (0xD199C175AEEC494BUL);

        [NativeTypeName("#define PRISM_BACKEND_ANDROID_TTS UINT64_C(0xBC175831BFE4E5CC)")]
        public const ulong PRISM_BACKEND_ANDROID_TTS = (0xBC175831BFE4E5CCUL);

        [NativeTypeName("#define PRISM_BACKEND_WEB_SPEECH UINT64_C(0x3572538D44D44A8F)")]
        public const ulong PRISM_BACKEND_WEB_SPEECH = (0x3572538D44D44A8FUL);
    }
}
