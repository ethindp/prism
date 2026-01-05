## Audio Callback

The `PrismAudioCallback` type defines the signature for audio data callbacks used with `prism_backend_speak_to_memory`.

### Syntax

```c
typedef void (*PrismAudioCallback)(
    void *userdata,
    const float *samples,
    size_t sample_count,
    size_t channels,
    size_t sample_rate
);
```

### Parameters

`userdata`

The user-defined pointer that was passed to `prism_backend_speak_to_memory`.

`samples`

Pointer to an array of interleaved audio samples. Samples are normalized 32-bit floating-point values in the range [-1.0, 1.0]. For multi-channel audio, samples are interleaved: for stereo, samples alternate left-right-left-right (L0, R0, L1, R1, ...).

`sample_count`

The total number of samples in the array. For multi-channel audio, this is the number of frames multiplied by the number of channels. To compute the number of frames, divide by `channels`.

`channels`

The number of audio channels. Typically 1 (mono) or 2 (stereo).

`sample_rate`

The sample rate in Hz. This is the number of samples per second per channel.

### Remarks

The callback function receives synthesized audio data from `prism_backend_speak_to_memory`. The callback may be invoked once with all data, or multiple times with successive chunks of data.

The callback may be invoked from a different thread than the one that called `prism_backend_speak_to_memory`. Callback implementations MUST be thread-safe if they access any shared state.

The `samples` pointer is valid only for the duration of the callback invocation. If the callback needs to retain the audio data, it MUST copy the data to its own buffer before returning.

The callback MUST NOT call any Prism function on the backend that initiated the synthesis. Doing so may cause deadlocks or undefined behavior.

The callback MUST NOT throw exceptions (in C++) or call `longjmp`. Abnormal return from the callback results in undefined behavior.

### Example

```c
typedef struct {
    float *buffer;
    size_t capacity;
    size_t size;
} AudioBuffer;

void accumulate_audio(void *userdata, const float *samples,
                      size_t sample_count, size_t channels, size_t sample_rate) {
    AudioBuffer *buf = (AudioBuffer *)userdata;    
    if (buf->size + sample_count > buf->capacity) {
        buf->capacity = (buf->size + sample_count) * 2;
        buf->buffer = realloc(buf->buffer, buf->capacity * sizeof(float));
    }
    memcpy(buf->buffer + buf->size, samples, sample_count * sizeof(float));
    buf->size += sample_count;
}
```
