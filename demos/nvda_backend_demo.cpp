#include <print>
#include <prism.h>

int main() {
  auto *const ctx = prism_init(nullptr);
  if (ctx == nullptr) {
    std::println("Could not initialize prism");
    return 1;
  }
  if (!prism_registry_exists(ctx, PRISM_BACKEND_NVDA)) {
    std::println(
        "Error: NVDA backend was not compiled into this build of Prism");
    prism_shutdown(ctx);
    return 1;
  }
  auto *const backend = prism_registry_create(ctx, PRISM_BACKEND_NVDA);
  if (backend == nullptr) {
    std::println("Error: could not instantiate Prism NVDA backend");
    prism_shutdown(ctx);
    return 1;
  }
  if (const auto res = prism_backend_initialize(backend); res != PRISM_OK) {
    std::println("Error: could not initialize Prism NVDA backend: {}",
                 prism_error_string(res));
    prism_backend_free(backend);
    prism_shutdown(ctx);
    return 1;
  }
  if (const auto res = prism_backend_speak(
          backend, "Hello! Testing the NVDA backend!", true);
      res != PRISM_OK) {
    std::println("Error: could not speak using NVDA: {}",
                 prism_error_string(res));
    prism_backend_free(backend);
    prism_shutdown(ctx);
    return 1;
  }
  prism_backend_free(backend);
  prism_shutdown(ctx);
  return 0;
}
