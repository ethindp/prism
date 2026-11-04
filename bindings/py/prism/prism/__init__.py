"""The Python binding for Prism.

The binding is distributed as the ``prismatoid`` package and imported as
``prism``. A :class:`Context` gives access to the backend registry, and its
methods return :class:`Backend` objects::

    import prism

    context = prism.Context()
    backend = context.create_best()
    backend.output("Hello from Prism.")

:meth:`Context.create_best` returns the user's screen reader if one is
running, and otherwise a speech engine.
"""

from . import _native as _native
from .common import (
    BackendFeatures,
    BackendId,
    PrismError,
    version,
    version_string,
)
from .core import AudioCallback, Backend, Context
from .custom import CustomBackend, Registry, RegistryBuilder
from .log import LogLevel, install_logging
from .log import flush as flush_log
from .log import set_level as set_log_level
from .log import uninstall as uninstall_logging
from .log import write as write_log

__version__ = version_string()
"""The version of the Prism library in use, as returned by :func:`version_string`."""

__all__ = [
    "AudioCallback",
    "Backend",
    "BackendFeatures",
    "BackendId",
    "Context",
    "CustomBackend",
    "LogLevel",
    "PrismError",
    "Registry",
    "RegistryBuilder",
    "__version__",
    "flush_log",
    "install_logging",
    "set_log_level",
    "uninstall_logging",
    "version",
    "version_string",
    "write_log",
]
