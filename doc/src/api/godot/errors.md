## Error Values

### `PrismError`

Operations that can fail return a `PrismError` value. The values and their meanings are those given in the C API's chapter on [error codes](../c/error-codes.md). They are bound as constants of `GodotPrismBackend` under the same names, and are accessed as in `GodotPrismBackend.PRISM_OK`.

Getters and setters do not return these values, and instead report failures through Godot's error log, as described for each.

### get_error_string

Returns a description of a `PrismError` value. This static method of `GodotPrismContext` wraps [`prism_error_string`](../c/utilities.md#prism_error_string).

#### Syntax

```gdscript
static func get_error_string(error: int) -> String
```

#### Parameters

`error`

The value to describe.

#### Return Value

Returns the description of the value.

#### Example

```gdscript
var err := backend.speak("Hello from Prism.", true)
if err != GodotPrismBackend.PRISM_OK:
	push_error("Speech failed: %s" % Prism.get_error_string(err))
```
