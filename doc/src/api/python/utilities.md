## Utilities

### `version`

Returns the version of the loaded library as an encoded integer. This function wraps [`prism_version`](../c/utilities.md#prism_version).

#### Syntax

```python
def version() -> int: ...
```

#### Return Value

Returns the encoded version.

### `version_string`

Returns the version of the loaded library as a string. This function wraps [`prism_version_string`](../c/utilities.md#prism_version_string).

#### Syntax

```python
def version_string() -> str: ...
```

#### Return Value

Returns the version.

### `__version__`

The value returned by `version_string` when the module was imported.

#### Syntax

```python
__version__: str
```
