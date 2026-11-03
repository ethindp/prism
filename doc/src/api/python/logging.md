## Logging

This chapter documents the Python interface to the facility described in the C API's chapter on [logging](../c/logging.md).

### `LogLevel`

An enumeration corresponding to [`PrismLogLevel`](../c/logging.md#prismloglevel).

#### Syntax

```python
class LogLevel(IntEnum):
    TRACE = ...
    DEBUG = ...
    INFO = ...
    WARN = ...
    ERROR = ...
    NONE = ...
```

### `install_logging`

Forwards Prism's log messages to Python's `logging` module. This function wraps [`prism_set_log_handler`](../c/logging.md#prism_set_log_handler).

#### Syntax

```python
def install_logging(logger_name: str = "prism") -> None: ...
```

#### Parameters

`logger_name`

The prefix of the logger names to which messages are sent. A message from the source `source` is sent to the logger named `logger_name.source`.

#### Remarks

This function has no effect if forwarding is already installed. `TRACE` is mapped to a level of the same name with the numeric value 5, and the remaining levels are mapped to the corresponding levels of the `logging` module.

The first installation also registers a function with `atexit` which calls [`prism_log_flush`](../c/logging.md#prism_log_flush) and [`prism_log_shutdown`](../c/logging.md#prism_log_shutdown) when the interpreter exits.

No messages are delivered until the threshold is raised through `set_log_level`, unless it has already been raised by other means.

#### Example

```python
import logging
import prism

logging.basicConfig(level=logging.INFO)
prism.install_logging()
prism.set_log_level(prism.LogLevel.INFO)
```

### `set_log_level`

Sets the minimum severity of messages to deliver. This function wraps [`prism_set_log_level`](../c/logging.md#prism_set_log_level).

#### Syntax

```python
def set_log_level(level: LogLevel) -> LogLevel: ...
```

#### Parameters

`level`

The new threshold.

#### Return Value

Returns the previous threshold.

### `uninstall_logging`

Stops forwarding log messages and reinstalls the handler that was previously in effect. This function has no effect if forwarding is not installed.

#### Syntax

```python
def uninstall_logging() -> None: ...
```

### `write_log`

Emits a log message. This function wraps [`prism_log`](../c/logging.md#prism_log).

#### Syntax

```python
def write_log(level: LogLevel, source: str, message: str) -> None: ...
```

#### Parameters

`level`

The severity of the message.

`source`

The component producing the message.

`message`

The text of the message.

#### Exceptions

Raises `PrismInvalidParamError` if `source` or `message` is empty or contains an embedded NUL character.

#### Remarks

Handlers attached to the loggers that receive forwarded messages run on Prism's logging thread, and consequently MUST NOT call this function or `flush_log`, as described under [`PrismLogCallback`](../c/logging.md#prismlogcallback).

### `flush_log`

Blocks until queued log messages have been delivered. This function wraps [`prism_log_flush`](../c/logging.md#prism_log_flush).

#### Syntax

```python
def flush_log() -> None: ...
```
