#include <prism.h>
#include "tolk.h"
#include "prism_lock.h"
#include "prism_utf.h"
#include <stdatomic.h>

static PrismContext* context = NULL;
static PrismBackend* backend = NULL;
static PrismBackend* sapi_backend = NULL;
static PrismLock backend_lock = PRISM_LOCK_INIT;
static atomic_flag try_sapi = ATOMIC_FLAG_INIT;
static atomic_flag prefer_sapi = ATOMIC_FLAG_INIT;

TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Load() {
context = prism_init(NULL);
prism_lock_acquire(&backend_lock);
backend = prism_registry_create_best(context);
sapi_backend = prism_registry_create(context, PRISM_BACKEND_SAPI);
prism_lock_release(&backend_lock);
}

TOLK_DLL_DECLSPEC bool TOLK_CALL Tolk_IsLoaded() {
return context != NULL && backend != NULL && sapi_backend != NULL;
}

TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_Unload() {
prism_lock_acquire(&backend_lock);
prism_backend_free(backend);
backend = NULL;
prism_backend_free(sapi_backend);
sapi_backend = NULL;
prism_lock_release(&backend_lock);
prism_shutdown(context);
context = NULL;
}

TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_TrySAPI(bool trySAPI) {
if (trySAPI) {
atomic_flag_test_and_set(&try_sapi);
} else {
atomic_flag_clear(&try_sapi);
}
}

TOLK_DLL_DECLSPEC void TOLK_CALL Tolk_PreferSAPI(bool preferSAPI) {
if (preferSAPI) {
atomic_flag_test_and_set(&prefer_sapi);
} else {
atomic_flag_clear(&prefer_sapi);
}
}

TOLK_DLL_DECLSPEC const wchar_t * TOLK_CALL Tolk_DetectScreenReader() {
if (atomic_load(&prefer_sapi)) {
