#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <prism.h>
#include <thread>

using namespace prism_test;

TEST_CASE("Backend creation", "[backend][lifecycle][create]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Create backend for each registered type") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      BackendPtr backend = make_backend(ctx.get(), id);

      INFO("Backend index: " << i);
      if (backend) {
        const char *name = prism_backend_name(backend.get());
        CHECK(name != nullptr);
      }
    }
  }

  SECTION("Create best backend") {
    BackendPtr backend = make_best_backend(ctx.get());

    if (count > 0) {
      // Should get at least one backend
      if (backend) {
        const char *name = prism_backend_name(backend.get());
        CHECK(name != nullptr);
      }
    }
  }
}

TEST_CASE("Backend free", "[backend][lifecycle][free]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Free accepts nullptr") {
    prism_backend_free(nullptr);
    // Should not crash
  }

  SECTION("Free works with valid backend") {
    size_t count = prism_registry_count(ctx.get());
    if (count > 0) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), 0);
      PrismBackend *backend = prism_registry_create(ctx.get(), id);

      if (backend) {
        prism_backend_free(backend);
        // Should not crash
      }
    }
  }

  SECTION("Multiple creates and frees") {
    size_t count = prism_registry_count(ctx.get());
    if (count > 0) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), 0);

      for (int i = 0; i < 10; ++i) {
        PrismBackend *backend = prism_registry_create(ctx.get(), id);
        if (backend) {
          prism_backend_free(backend);
        }
      }
    }
  }
}

TEST_CASE("Backend name", "[backend][lifecycle][name]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Backend name is non-null") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      BackendPtr backend = make_backend(ctx.get(), id);

      if (backend) {
        const char *name = prism_backend_name(backend.get());
        REQUIRE(name != nullptr);
        CHECK(strlen(name) > 0);
      }
    }
  }

  SECTION("Backend name matches registry name") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      const char *registry_name = prism_registry_name(ctx.get(), id);

      BackendPtr backend = make_backend(ctx.get(), id);
      if (backend) {
        const char *backend_name = prism_backend_name(backend.get());
        REQUIRE(backend_name != nullptr);
        REQUIRE(registry_name != nullptr);
        CHECK(strcmp(backend_name, registry_name) == 0);
      }
    }
  }

  SECTION("Backend name is consistent") {
    BackendPtr backend = make_best_backend(ctx.get());

    if (backend) {
      const char *name1 = prism_backend_name(backend.get());
      const char *name2 = prism_backend_name(backend.get());

      REQUIRE(name1 != nullptr);
      REQUIRE(name2 != nullptr);
      CHECK(strcmp(name1, name2) == 0);
    }
  }
}

TEST_CASE("Backend initialization", "[backend][lifecycle][init]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Initialize each backend type") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      BackendPtr backend = make_backend(ctx.get(), id);

      if (backend) {
        const char *name = prism_backend_name(backend.get());
        INFO("Backend: " << (name ? name : "unknown"));

        PrismError err = prism_backend_initialize(backend.get());
        REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
      }
    }
  }

  SECTION("Double initialization") {
    BackendPtr backend = make_best_backend(ctx.get());

    if (backend) {
      PrismError err1 = prism_backend_initialize(backend.get());

      if (err1 == PRISM_OK) {
        PrismError err2 = prism_backend_initialize(backend.get());
        // Should be OK or ALREADY_INITIALIZED
        CHECK((err2 == PRISM_OK || err2 == PRISM_ERROR_ALREADY_INITIALIZED));
      }
    }
  }

  SECTION("Operations before initialization") {
    BackendPtr backend = make_best_backend(ctx.get());

    if (backend) {
      // Some operations might require initialization
      bool speaking = false;
      PrismError err = prism_backend_is_speaking(backend.get(), &speaking);
      // May return NOT_INITIALIZED or work anyway
      CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_INITIALIZED ||
             is_unavailable_error(err)));
    }
  }
}

TEST_CASE("Backend lifecycle sequence", "[backend][lifecycle][sequence]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Full lifecycle: create -> init -> use -> free") {
    BackendPtr backend = make_best_backend(ctx.get());

    if (backend) {
      const char *name = prism_backend_name(backend.get());
      INFO("Backend: " << (name ? name : "unknown"));

      // Initialize
      PrismError err = prism_backend_initialize(backend.get());
      if (err != PRISM_OK && !is_unavailable_error(err)) {
        FAIL("Initialize failed with unexpected error: "
             << prism_error_string(err));
      }

      if (err == PRISM_OK) {
        // Check if speaking (should not be)
        bool speaking = false;
        err = prism_backend_is_speaking(backend.get(), &speaking);
        if (err == PRISM_OK) {
          CHECK_FALSE(speaking);
        }

        // Get volume (should work)
        float volume = 0.0f;
        err = prism_backend_get_volume(backend.get(), &volume);
        if (err == PRISM_OK) {
          CHECK(volume >= 0.0f);
          CHECK(volume <= 1.0f);
        }
      }

      // Free happens automatically via RAII
    }
  }

  SECTION("Multiple backends simultaneously") {
    std::vector<BackendPtr> backends;

    size_t count = prism_registry_count(ctx.get());
    for (size_t i = 0; i < count && i < 5; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      BackendPtr backend = make_backend(ctx.get(), id);
      if (backend) {
        backends.push_back(std::move(backend));
      }
    }

    // Initialize all
    for (auto &backend : backends) {
      PrismError err = prism_backend_initialize(backend.get());
      CHECK((err == PRISM_OK || is_acceptable_error(err)));
    }

    // All should have valid names
    for (auto &backend : backends) {
      const char *name = prism_backend_name(backend.get());
      CHECK(name != nullptr);
    }

    // Cleanup happens via RAII
  }
}

TEST_CASE("Backend lifecycle stress", "[backend][lifecycle][stress]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Rapid create/destroy cycles") {
    size_t count = prism_registry_count(ctx.get());
    if (count > 0) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), 0);

      for (int i = 0; i < 100; ++i) {
        BackendPtr backend = make_backend(ctx.get(), id);
        // Immediately destroyed
      }
    }
  }

  SECTION("Rapid init/free cycles") {
    size_t count = prism_registry_count(ctx.get());
    if (count > 0) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), 0);

      for (int i = 0; i < 20; ++i) {
        BackendPtr backend = make_backend(ctx.get(), id);
        if (backend) {
          PrismError err = prism_backend_initialize(backend.get());
          (void)err;
        }
      }
    }
  }
}

TEST_CASE("Backend with different contexts", "[backend][lifecycle][context]") {
  SECTION("Backends from different contexts are independent") {
    ContextPtr ctx1 = make_context();
    ContextPtr ctx2 = make_context();

    REQUIRE(ctx1.get() != nullptr);
    REQUIRE(ctx2.get() != nullptr);

    BackendPtr backend1 = make_best_backend(ctx1.get());
    BackendPtr backend2 = make_best_backend(ctx2.get());

    if (backend1 && backend2) {
      // Both should be usable
      const char *name1 = prism_backend_name(backend1.get());
      const char *name2 = prism_backend_name(backend2.get());

      CHECK(name1 != nullptr);
      CHECK(name2 != nullptr);
    }
  }

  SECTION("Backend survives context destruction (created backend)") {
    PrismBackend *backend = nullptr;

    {
      ContextPtr ctx = make_context();
      REQUIRE(ctx.get() != nullptr);

      // Create (not acquire) - gives us ownership
      backend = prism_registry_create_best(ctx.get());
      // Context is destroyed here
    }

    if (backend) {
      // Backend should still be valid (we own it)
      const char *name = prism_backend_name(backend);
      CHECK(name != nullptr);

      prism_backend_free(backend);
    }
  }
}

TEST_CASE("ScopedBackendInit helper", "[backend][lifecycle][helper]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Scoped init tracks initialization state") {
    BackendPtr backend = make_best_backend(ctx.get());

    if (backend) {
      ScopedBackendInit init(backend.get());

      if (init.is_initialized()) {
        // Should be able to use the backend
        bool speaking = false;
        PrismError err = prism_backend_is_speaking(backend.get(), &speaking);
        CHECK((err == PRISM_OK || is_unavailable_error(err)));
      }
    }
  }

  SECTION("Scoped init with nullptr") {
    ScopedBackendInit init(nullptr);
    CHECK_FALSE(init.is_initialized());
    CHECK(init.get() == nullptr);
  }
}

TEST_CASE("Backend RAII wrapper", "[backend][lifecycle][raii]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("BackendPtr manages lifetime") {
    {
      BackendPtr backend = make_best_backend(ctx.get());
      if (backend) {
        const char *name = prism_backend_name(backend.get());
        CHECK(name != nullptr);
      }
    }
    // Backend freed here
  }

  SECTION("BackendPtr can be moved") {
    BackendPtr backend1 = make_best_backend(ctx.get());

    if (backend1) {
      PrismBackend *raw = backend1.get();
      BackendPtr backend2 = std::move(backend1);

      CHECK(backend1.get() == nullptr);
      CHECK(backend2.get() == raw);
    }
  }

  SECTION("BackendPtr reset") {
    BackendPtr backend = make_best_backend(ctx.get());

    if (backend) {
      backend.reset();
      CHECK(backend.get() == nullptr);
    }
  }
}

TEST_CASE("Backend initialization errors", "[backend][lifecycle][errors]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Initialize reports proper error codes") {
    size_t count = prism_registry_count(ctx.get());

    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      BackendPtr backend = make_backend(ctx.get(), id);

      if (backend) {
        const char *name = prism_backend_name(backend.get());
        INFO("Backend: " << (name ? name : "unknown"));

        PrismError err = prism_backend_initialize(backend.get());

        // Check that error is one of the expected values
        CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_IMPLEMENTED ||
               err == PRISM_ERROR_ALREADY_INITIALIZED ||
               err == PRISM_ERROR_BACKEND_NOT_AVAILABLE ||
               err == PRISM_ERROR_INTERNAL || err == PRISM_ERROR_NO_VOICES));
      }
    }
  }
}
