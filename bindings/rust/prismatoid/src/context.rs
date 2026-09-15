// SPDX-License-Identifier: MPL-2.0

use crate::backend::Backend;
use crate::backend_id::BackendId;
use crate::config::{availability_trampoline, AvailabilityCallback, Config};
use crate::error::Error;
use std::ffi::{CStr, CString};
use std::sync::Mutex;

/// A descriptor containing metadata about an available backend in the registry.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct BackendDescriptor {
    pub id: BackendId,
    pub name: String,
    pub priority: i32,
}

/// The root context for all Prism operations.
///
/// A `Context` encapsulates access to the backend registry and backend enumeration
/// services. Multiple threads may safely query the registry and acquire backends concurrently.
///
/// Dropping the `Context` automatically performs an orderly shutdown via `prism_shutdown`.
pub struct Context {
    raw: *mut prismatoid_sys::PrismContext,
    _availability_box: Option<Box<Mutex<AvailabilityCallback>>>,
}

unsafe impl Send for Context {}
unsafe impl Sync for Context {}

impl Drop for Context {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                prismatoid_sys::prism_shutdown(self.raw);
            }
            self.raw = std::ptr::null_mut();
        }
    }
}

impl Context {
    /// Creates a new default Prism context.
    pub fn new() -> Result<Self, Error> {
        Self::with_config(Config::default())
    }

    /// Creates a new Prism context using the specified configuration.
    pub fn with_config(mut config: Config) -> Result<Self, Error> {
        let mut raw_config = unsafe { prismatoid_sys::prism_config_init() };
        raw_config.availability_poll_interval_ms = config.poll_interval_ms;
        raw_config.availability_debounce_samples = config.debounce_samples;
        raw_config.availability_backoff_max_ms = config.backoff_max_ms;
        raw_config.availability_auto_power_manage = config.auto_power_manage;

        if let Some(ref reg) = config.registry {
            raw_config.registry = reg.as_raw();
        }

        let boxed_cb = config
            .availability_callback
            .take()
            .map(Mutex::new)
            .map(Box::new);

        if let Some(ref b) = boxed_cb {
            raw_config.availability_callback = Some(availability_trampoline);
            raw_config.availability_userdata = b.as_ref() as *const _ as *mut _;
        }

        let raw = unsafe { prismatoid_sys::prism_init(&mut raw_config) };
        if raw.is_null() {
            return Err(Error::NotInitialized);
        }

        Ok(Self {
            raw,
            _availability_box: boxed_cb,
        })
    }

    /// Pauses background polling of backend availability.
    pub fn pause_availability_poll(&self) {
        unsafe {
            prismatoid_sys::prism_availability_poll_pause(self.raw);
        }
    }

    /// Resumes background polling of backend availability.
    pub fn resume_availability_poll(&self) {
        unsafe {
            prismatoid_sys::prism_availability_poll_resume(self.raw);
        }
    }

    /// Returns true if the host operating system supports power-management notifications.
    pub fn is_auto_power_supported() -> bool {
        unsafe { prismatoid_sys::prism_availability_auto_power_supported() }
    }

    /// Returns the number of backends registered in this context.
    pub fn backends_count(&self) -> usize {
        unsafe { prismatoid_sys::prism_registry_count(self.raw) }
    }

    /// Alias for [`backends_count`].
    pub fn count(&self) -> usize {
        self.backends_count()
    }

    /// Retrieves the backend ID at the given registry index.
    pub fn backend_id_at(&self, index: usize) -> Option<BackendId> {
        let raw_id = unsafe { prismatoid_sys::prism_registry_id_at(self.raw, index) };
        if raw_id == prismatoid_sys::PRISM_BACKEND_INVALID {
            None
        } else {
            Some(BackendId::from_raw(raw_id))
        }
    }

    /// Alias for [`backend_id_at`].
    pub fn id_at(&self, index: usize) -> Option<BackendId> {
        self.backend_id_at(index)
    }

    /// Looks up a backend ID by its name (e.g. "NVDA", "OneCore").
    pub fn backend_id_by_name(&self, name: &str) -> Option<BackendId> {
        let c_name = CString::new(name).ok()?;
        let raw_id = unsafe { prismatoid_sys::prism_registry_id(self.raw, c_name.as_ptr()) };
        if raw_id == prismatoid_sys::PRISM_BACKEND_INVALID {
            None
        } else {
            Some(BackendId::from_raw(raw_id))
        }
    }

    /// Alias for [`backend_id_by_name`].
    pub fn id_for_name(&self, name: &str) -> Option<BackendId> {
        self.backend_id_by_name(name)
    }

    /// Looks up the human-readable name of a backend by its ID.
    pub fn backend_name(&self, id: BackendId) -> Option<&str> {
        unsafe {
            let ptr = prismatoid_sys::prism_registry_name(self.raw, id.raw());
            if ptr.is_null() {
                None
            } else {
                CStr::from_ptr(ptr).to_str().ok()
            }
        }
    }

    /// Alias for [`backend_name`].
    pub fn name(&self, id: BackendId) -> Option<&str> {
        self.backend_name(id)
    }

    /// Looks up the priority ranking of a backend by its ID.
    pub fn backend_priority(&self, id: BackendId) -> Option<i32> {
        let prio = unsafe { prismatoid_sys::prism_registry_priority(self.raw, id.raw()) };
        if prio == -1 {
            None
        } else {
            Some(prio)
        }
    }

    /// Returns true if a backend with the given ID exists in the registry.
    pub fn backend_exists(&self, id: BackendId) -> bool {
        unsafe { prismatoid_sys::prism_registry_exists(self.raw, id.raw()) }
    }

    /// Creates a newly allocated, independent instance of the specified backend.
    pub fn create_backend(&self, id: BackendId) -> Result<Backend, Error> {
        let ptr = unsafe { prismatoid_sys::prism_registry_create(self.raw, id.raw()) };
        if ptr.is_null() {
            Err(Error::BackendNotAvailable)
        } else {
            unsafe { Backend::from_raw(ptr) }
        }
    }

    /// Creates a newly allocated instance of the highest-priority supported backend.
    pub fn create_best_backend(&self) -> Result<Backend, Error> {
        let ptr = unsafe { prismatoid_sys::prism_registry_create_best(self.raw) };
        if ptr.is_null() {
            Err(Error::BackendNotAvailable)
        } else {
            unsafe { Backend::from_raw(ptr) }
        }
    }

    /// Alias for [`create_best_backend`].
    pub fn create_best(&self) -> Result<Backend, Error> {
        self.create_best_backend()
    }

    /// Acquires a cached or newly created instance of the specified backend.
    pub fn acquire_backend(&self, id: BackendId) -> Result<Backend, Error> {
        let ptr = unsafe { prismatoid_sys::prism_registry_acquire(self.raw, id.raw()) };
        if ptr.is_null() {
            Err(Error::BackendNotAvailable)
        } else {
            unsafe { Backend::from_raw(ptr) }
        }
    }

    /// Acquires a cached or newly created instance of the highest-priority supported backend.
    pub fn acquire_best_backend(&self) -> Result<Backend, Error> {
        let ptr = unsafe { prismatoid_sys::prism_registry_acquire_best(self.raw) };
        if ptr.is_null() {
            Err(Error::BackendNotAvailable)
        } else {
            unsafe { Backend::from_raw(ptr) }
        }
    }

    /// Alias for [`acquire_best_backend`].
    pub fn acquire_best(&self) -> Result<Backend, Error> {
        self.acquire_best_backend()
    }

    /// Returns a list of all backend descriptors currently registered in this context.
    pub fn available_backends(&self) -> Vec<BackendDescriptor> {
        let count = self.backends_count();
        let mut list = Vec::with_capacity(count);
        for i in 0..count {
            if let Some(id) = self.backend_id_at(i) {
                let name = self.backend_name(id).unwrap_or("").to_owned();
                let priority = self.backend_priority(id).unwrap_or(0);
                list.push(BackendDescriptor { id, name, priority });
            }
        }
        list
    }

    /// Looks up the descriptor for a specific backend ID.
    pub fn descriptor(&self, id: BackendId) -> Option<BackendDescriptor> {
        if !self.backend_exists(id) {
            return None;
        }
        let name = self.backend_name(id)?.to_owned();
        let priority = self.backend_priority(id)?;
        Some(BackendDescriptor { id, name, priority })
    }
}
