## Logging

Prism emits diagnostic messages through an internal logging subsystem. By default these messages are discarded; an application that wishes to observe them installs a log handler, and the library then delivers messages to that handler. The subsystem is designed so that logging never blocks the calling thread and never interferes with speech synthesis: messages are handed to a background thread and delivered to the handler from there.

Logging is a process-wide facility rather than a per-context one. There is a single logger shared by the entire library, and the functions in this chapter operate on it directly. They do not take a `PrismContext` and MAY be called before `prism_init` or after `prism_shutdown`.

### `PrismLogLevel`

An enumeration describing the severity of a log message and, when used as a threshold, the minimum severity an application wishes to receive.

#### Syntax

```c
typedef enum PrismLogLevel {
  PRISM_LOG_LEVEL_TRACE,
  PRISM_LOG_LEVEL_DEBUG,
  PRISM_LOG_LEVEL_INFO,
  PRISM_LOG_LEVEL_WARN,
  PRISM_LOG_LEVEL_ERROR,
  PRISM_LOG_LEVEL_NONE
} PrismLogLevel;
```

#### Members

`PRISM_LOG_LEVEL_TRACE`

The most verbose level, used for fine-grained tracing of internal operations.

`PRISM_LOG_LEVEL_DEBUG`

Diagnostic information useful during development.

`PRISM_LOG_LEVEL_INFO`

Informational messages describing normal operation.

`PRISM_LOG_LEVEL_WARN`

Conditions that are not errors but MAY indicate a problem.

`PRISM_LOG_LEVEL_ERROR`

Error conditions.

`PRISM_LOG_LEVEL_NONE`

Not a message severity. When supplied to `prism_set_log_level`, it suppresses all messages, since no message has a severity greater than or equal to it.

#### Remarks

The levels are ordered from least to most severe, with `PRISM_LOG_LEVEL_NONE` ordered above all message severities. A message is delivered only when its level is greater than or equal to the current threshold, so a threshold of `PRISM_LOG_LEVEL_WARN` delivers warnings and errors while discarding trace, debug, and informational messages. The numeric values are part of the ABI and will not change; new levels, if any are ever added, would be inserted in a way that preserves this ordering.

### `PrismLogCallback`

The type of a function invoked to deliver a single log message.

#### Syntax

```c
typedef void(PRISM_CALL *PrismLogCallback)(void *userdata, PrismLogLevel level,
                                           const char *source,
                                           const char *message);
```

#### Parameters

`userdata`

The opaque pointer supplied in the `PrismLogHandler` that installed this callback. Prism does not interpret this value.

`level`

The severity of the message.

`source`

A null-terminated string naming the component that produced the message. This string is valid only for the duration of the call.

`message`

A null-terminated string containing the message text. This string is valid only for the duration of the call.

#### Remarks

The callback is invoked from Prism's internal logging thread, not from the thread that produced the message. Callback implementations MUST be prepared for this and MUST provide their own synchronization if they touch shared state. Because delivery is serialized on a single thread, a given handler is never invoked concurrently with itself.

The `source` and `message` pointers are owned by Prism and are valid only for the duration of the call. A handler that needs to retain either string MUST copy it.

A handler SHOULD do as little work as possible and MUST NOT block for an extended period, since the logging thread cannot deliver further messages until the handler returns and messages produced in the meantime MAY be dropped. A handler MUST NOT call `prism_log`, `prism_log_flush`, or `prism_log_shutdown`; doing so results in undefined behavior.

### `PrismLogHandler`

A structure pairing a log callback with an opaque user pointer.

#### Syntax

```c
typedef struct PrismLogHandler {
  PrismLogCallback fn;
  void *userdata;
} PrismLogHandler;
```

#### Members

`fn`

The callback invoked to deliver messages, or `NULL` to install no handler. When `fn` is `NULL`, messages are discarded.

`userdata`

An opaque pointer passed unmodified to `fn` on each invocation. Prism does not interpret or take ownership of this value. The lifetime of this value is the lifetime of the handler function, and therefore this value must be valid for as long as the handler is alive.

### prism_set_log_handler

Installs the handler that receives log messages, replacing any previously installed handler.

#### Syntax

```c
PrismLogHandler prism_set_log_handler(PrismLogHandler handler);
```

#### Parameters

`handler`

The handler to install. A handler whose `fn` is `NULL` disables delivery.

#### Return Value

Returns the handler that was previously installed. If no handler was installed, the returned structure's `fn` member is `NULL`.

#### Remarks

Installing a handler makes Prism's diagnostic output visible to the application. Until a handler is installed, messages are discarded, except when the `PRISM_LOG` environment variable is set.

This function is thread-safe and MAY be called at any time, including before `prism_init` and concurrently with logging activity on other threads. The replacement takes effect for messages delivered after the call; a message already in flight MAY still be delivered to the previous handler. For this reason, an application MUST NOT assume that its previous handler has stopped being invoked the instant this function returns, and MUST keep any state the previous handler depends on valid until the application has otherwise ensured that no delivery is in progress.

The returned handler MAY be retained and reinstalled later to restore prior behavior.

### prism_set_log_level

Sets the minimum severity of messages that will be delivered.

#### Syntax

```c
PrismLogLevel prism_set_log_level(PrismLogLevel level);
```

#### Parameters

`level`

The minimum severity to deliver. Messages whose severity is below this level are discarded before they are queued. `PRISM_LOG_LEVEL_NONE` suppresses all messages.

#### Return Value

Returns the threshold that was in effect before the call.

#### Remarks

The threshold is applied on the thread that produces a message, before the message is queued, so raising the threshold immediately reduces the work performed for suppressed messages. This function is thread-safe and MAY be called at any time.

The threshold and the installed handler are independent.

### prism_log

Emits a log message.

#### Syntax

```c
void prism_log(PrismLogLevel level, const char *source, const char *message);
```

#### Parameters

`level`

The severity of the message.

`source`

A null-terminated string naming the component producing the message. This parameter MUST NOT be `NULL`.

`message`

A null-terminated string containing the message text. This parameter MUST NOT be `NULL`.

#### Return Value

This function does not return a value.

#### Remarks

This function is provided primarily for use by backend implementations and other components layered on top of Prism, so that their diagnostics are routed through the same handler as the library's own. Applications MAY also call it.

If the message's severity is below the current threshold, the function returns without queuing anything. Otherwise the message is copied and placed on an internal queue for delivery by the logging thread; the function does not invoke the handler directly and does not block waiting for delivery.

The queue has a fixed capacity. If it is full when a message is submitted, the message is dropped and an internal counter is incremented. The next time the logging thread delivers messages, it reports the number of dropped messages to the handler as a single `PRISM_LOG_LEVEL_WARN` message whose `source` is `"prism"`. Applications that observe such a report are producing messages faster than the handler consumes them and SHOULD either raise the threshold or make the handler faster.

This function is thread-safe and MAY be called from any thread concurrently.

### prism_log_flush

Blocks until all messages queued before the call have been delivered.

#### Syntax

```c
void prism_log_flush(void);
```

#### Parameters

This function has no parameters.

#### Return Value

This function does not return a value.

#### Remarks

`prism_log_flush` returns only after every message submitted before the call has been passed to the installed handler. It is useful before installing a new handler, before shutting the process down, or at any point where the application needs to be certain that pending diagnostics have been delivered.

A flush is never dropped, even when the message queue is otherwise full. This function MUST NOT be called from within a log handler.

### prism_log_shutdown

Stops the logging thread and releases the resources associated with the logger.

#### Syntax

```c
void prism_log_shutdown(void);
```

#### Parameters

This function has no parameters.

#### Return Value

This function does not return a value.

#### Remarks

`prism_log_shutdown` drains any remaining messages, stops the internal logging thread, and joins it. It is not ordinarily necessary to call this function, since the logger is torn down automatically when the process exits. It is provided for applications that must guarantee the logging thread has terminated at a specific point, for example before unloading the library.

After this function returns, the logger is no longer running. Calls to `prism_log` after shutdown are accepted but their messages are not delivered. This function MUST NOT be called from within a log handler, and the application MUST ensure that no other thread is calling any logging function concurrently.

#### Warning

There is no way to re-launch the logging thread after this function is called. Applications SHOULD take care that this  function is only called when they are absolutely certain they will not need the logging thread.

### Default behavior and the `PRISM_LOG` environment variable

When the library is first initialized by `prism_init`, it inspects the `PRISM_LOG` environment variable. If the variable is set to one of the values `trace`, `debug`, `info`, `warn`, `error`, or `none`, Prism installs a built-in handler that writes messages to the standard error stream and sets the threshold to the corresponding level. The values are matched exactly and in lower case; any other value, or an unset variable, leaves the default configuration unchanged.

In the absence of both an application-installed handler and the `PRISM_LOG` variable, no handler is installed and all messages are discarded. Logging therefore has negligible cost by default.

Because the `PRISM_LOG` handler is installed during `prism_init`, an application that installs its own handler with `prism_set_log_handler` after initialization will replace the standard-error handler. An application that wishes to observe messages produced before its own handler is installed SHOULD either set `PRISM_LOG` or install its handler before the first call to `prism_init`.
