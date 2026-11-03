## Backend

### `Backend`

A backend instance. Instances are obtained from the methods of [`Context`](./context.md) and SHOULD NOT be constructed directly. The instance is released through [`prism_backend_free`](../c/backend-functions.md#prism_backend_free) when the object is destroyed. An instance is subject to the rules given in the chapter on [thread safety](../c/thread-safety.md).

Every method and property of this class raises the appropriate subclass of [`PrismError`](./exceptions.md) if the underlying C function fails. These exceptions are not listed again for each entry. Methods which accept text also raise `PrismInvalidParamError` if the text is empty or contains an embedded NUL character, neither of which the C API can represent.

### `Backend.name`

The name of the backend. This read-only property wraps [`prism_backend_name`](../c/backend-functions.md#prism_backend_name).

#### Syntax

```python
@property
def name(self) -> str: ...
```

### `Backend.features`

The backend's features. This read-only property wraps [`prism_backend_get_features`](../c/backend-functions.md#prism_backend_get_features) and queries the backend on each access.

#### Syntax

```python
@property
def features(self) -> BackendFeatures: ...
```

### `Backend.speak`

Speaks text. This method wraps [`prism_backend_speak`](../c/backend-functions.md#prism_backend_speak).

#### Syntax

```python
def speak(self, text: str, interrupt: bool = False) -> None: ...
```

#### Parameters

`text`

The text to speak.

`interrupt`

Whether to interrupt speech in progress.

#### Example

```python
try:
    backend.speak("Hello from Prism.", interrupt=True)
except prism.PrismError as err:
    print(f"Speech failed: {err}")
```

### `Backend.speak_to_memory`

Synthesizes text and delivers the audio to a function. This method wraps [`prism_backend_speak_to_memory`](../c/backend-functions.md#prism_backend_speak_to_memory).

#### Syntax

```python
def speak_to_memory(self, text: str, on_audio_data: AudioCallback) -> None: ...
```

#### Parameters

`text`

The text to synthesize.

`on_audio_data`

An [`AudioCallback`](./types.md#audiocallback) that receives the samples.

#### Remarks

An exception raised by `on_audio_data` is retained, later deliveries for the same call are skipped, and the exception is raised again from this method when synthesis has finished.

### `Backend.braille`

Presents text on a braille display. This method wraps [`prism_backend_braille`](../c/backend-functions.md#prism_backend_braille).

#### Syntax

```python
def braille(self, text: str) -> None: ...
```

#### Parameters

`text`

The text to present.

### `Backend.output`

Presents text through every modality the backend supports. This method wraps [`prism_backend_output`](../c/backend-functions.md#prism_backend_output).

#### Syntax

```python
def output(self, text: str, interrupt: bool = False) -> None: ...
```

#### Parameters

`text`

The text to present.

`interrupt`

Whether to interrupt output in progress.

### `Backend.stop`

Stops speech. This method wraps [`prism_backend_stop`](../c/backend-functions.md#prism_backend_stop).

#### Syntax

```python
def stop(self) -> None: ...
```

### `Backend.pause`

Pauses speech. This method wraps [`prism_backend_pause`](../c/backend-functions.md#prism_backend_pause).

#### Syntax

```python
def pause(self) -> None: ...
```

### `Backend.resume`

Resumes speech. This method wraps [`prism_backend_resume`](../c/backend-functions.md#prism_backend_resume).

#### Syntax

```python
def resume(self) -> None: ...
```

### `Backend.speaking`

Whether the backend is speaking. This read-only property wraps [`prism_backend_is_speaking`](../c/backend-functions.md#prism_backend_is_speaking).

#### Syntax

```python
@property
def speaking(self) -> bool: ...
```

### `Backend.volume`, `Backend.rate`, and `Backend.pitch`

The voice parameters. These properties wrap the corresponding functions described in the chapter on [voice parameters](../c/voice-parameters.md).

#### Syntax

```python
@property
def volume(self) -> float: ...
@volume.setter
def volume(self, volume: float) -> None: ...
```

The `rate` and `pitch` properties have the same form.

### `Backend.refresh_voices`

Refreshes the voice list. This method wraps `prism_backend_refresh_voices`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```python
def refresh_voices(self) -> None: ...
```

### `Backend.voices_count`

The number of voices. This read-only property wraps `prism_backend_count_voices`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```python
@property
def voices_count(self) -> int: ...
```

### `Backend.get_voice_name`

Returns the name of a voice. This method wraps `prism_backend_get_voice_name`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```python
def get_voice_name(self, idx: int) -> str: ...
```

#### Parameters

`idx`

The index of the voice.

#### Return Value

Returns the name of the voice.

### `Backend.get_voice_language`

Returns the language of a voice. This method wraps `prism_backend_get_voice_language`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```python
def get_voice_language(self, idx: int) -> str: ...
```

#### Parameters

`idx`

The index of the voice.

#### Return Value

Returns the language of the voice.

### `Backend.voice`

The index of the current voice. This property wraps `prism_backend_get_voice` and `prism_backend_set_voice`, described in the chapter on [voice selection](../c/voice-selection.md).

#### Syntax

```python
@property
def voice(self) -> int: ...
@voice.setter
def voice(self, idx: int) -> None: ...
```

### `Backend.channels`, `Backend.sample_rate`, and `Backend.bit_depth`

The backend's audio format. These read-only properties wrap the corresponding functions described in the chapter on [audio format](../c/audio-format.md).

#### Syntax

```python
@property
def channels(self) -> int: ...
@property
def sample_rate(self) -> int: ...
@property
def bit_depth(self) -> int: ...
```
