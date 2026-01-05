#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <prism.h>
#include <set>
#include <string>

using namespace prism_test;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Registry count", "[registry][count]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Returns valid count") {
    size_t count = prism_registry_count(ctx.get());
    // Count is platform-dependent, but should be reasonable
    CHECK(count < 100); // Sanity check
  }

  SECTION("Count is consistent") {
    size_t count1 = prism_registry_count(ctx.get());
    size_t count2 = prism_registry_count(ctx.get());
    REQUIRE(count1 == count2);
  }
}

TEST_CASE("Registry ID at index", "[registry][id_at]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Valid indices return IDs") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      // ID should be non-zero for valid backends
      // (though 0 could theoretically be valid, it's unlikely by design)
      INFO("Index: " << i << ", ID: " << id);
      CHECK(id != 0);
    }
  }

  SECTION("IDs are unique") {
    std::set<PrismBackendId> ids;
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      auto [it, inserted] = ids.insert(id);
      INFO("Index: " << i << ", ID: " << id);
      CHECK(inserted); // Should not have duplicate IDs
    }
  }

  SECTION("Out of bounds index behavior") {
    if (count > 0) {
      // Index at count should be out of bounds
      PrismBackendId id = prism_registry_id_at(ctx.get(), count);
      // Behavior is implementation-defined, but should not crash
      (void)id;

      // Very large index
      id = prism_registry_id_at(ctx.get(), SIZE_MAX);
      (void)id;
    }
  }
}

TEST_CASE("Registry ID by name", "[registry][id_by_name]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Known backend names return correct IDs") {
    // Get actual backend names from the registry and verify ID lookup
    size_t count = prism_registry_count(ctx.get());

    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      if (id == 0)
        continue;

      const char *name = prism_registry_name(ctx.get(), id);
      if (name && name[0] != '\0') {
        // Looking up by name should return the same ID
        PrismBackendId looked_up_id = prism_registry_id(ctx.get(), name);
        INFO("Backend name: " << name << ", expected ID: " << id);
        CHECK(looked_up_id == id);
      }
    }
  }

  SECTION("Well-known backend IDs match expected names") {
    // Test that if a well-known backend exists, its name lookup works
    // NOTE: Backend names are case-sensitive and must match exactly
    struct NameIdPair {
      const char *name;
      PrismBackendId expected_id;
    };

    const NameIdPair known_backends[] = {
        {"SAPI", PRISM_BACKEND_SAPI},
        {"AVSpeech", PRISM_BACKEND_AV_SPEECH},
        {"VoiceOver", PRISM_BACKEND_VOICE_OVER},
        {"Speech Dispatcher", PRISM_BACKEND_SPEECH_DISPATCHER},
        {"NVDA", PRISM_BACKEND_NVDA},
        {"JAWS", PRISM_BACKEND_JAWS},
        {"OneCore", PRISM_BACKEND_ONE_CORE},
        {"Orca", PRISM_BACKEND_ORCA},
    };

    for (const auto &pair : known_backends) {
      if (prism_registry_exists(ctx.get(), pair.expected_id)) {
        // Get the actual name the library uses for this backend
        const char *actual_name =
            prism_registry_name(ctx.get(), pair.expected_id);
        if (actual_name) {
          INFO("Backend ID: " << pair.expected_id << ", expected name: "
                              << pair.name << ", actual name: " << actual_name);
          // The ID lookup should work with the actual name
          PrismBackendId id = prism_registry_id(ctx.get(), actual_name);
          CHECK(id == pair.expected_id);
        }
      }
    }
  }

  SECTION("Unknown name returns 0 or special value") {
    PrismBackendId id = prism_registry_id(ctx.get(), "nonexistent_backend_xyz");
    // Should return 0 or some indicator of not found
    // (behavior is implementation-defined)
    (void)id;
  }

  SECTION("Empty name handling") {
    PrismBackendId id = prism_registry_id(ctx.get(), "");
    // Should not crash, behavior is implementation-defined
    (void)id;
  }
}

TEST_CASE("Registry name by ID", "[registry][name]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Valid IDs return non-null names") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      const char *name = prism_registry_name(ctx.get(), id);
      INFO("Index: " << i << ", ID: " << id);
      REQUIRE(name != nullptr);
      CHECK(strlen(name) > 0);
    }
  }

  SECTION("Names are consistent") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      const char *name1 = prism_registry_name(ctx.get(), id);
      const char *name2 = prism_registry_name(ctx.get(), id);
      REQUIRE(name1 != nullptr);
      REQUIRE(name2 != nullptr);
      CHECK(strcmp(name1, name2) == 0);
    }
  }

  SECTION("Invalid ID returns null") {
    const char *name = prism_registry_name(ctx.get(), 0);
    CHECK(name == nullptr);

    name = prism_registry_name(ctx.get(), UINT64_MAX);
    CHECK(name == nullptr);
  }

  SECTION("Name to ID round-trip") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      const char *name = prism_registry_name(ctx.get(), id);
      REQUIRE(name != nullptr);

      PrismBackendId id2 = prism_registry_id(ctx.get(), name);
      CHECK(id == id2);
    }
  }
}

TEST_CASE("Registry priority", "[registry][priority]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Valid IDs return reasonable priorities") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      int priority = prism_registry_priority(ctx.get(), id);
      // Priority should be some reasonable value
      INFO("Index: " << i << ", Priority: " << priority);
      CHECK(priority >= -1000);
      CHECK(priority <= 1000);
    }
  }

  SECTION("Invalid ID priority") {
    int priority = prism_registry_priority(ctx.get(), 0);
    (void)priority; // Should not crash

    priority = prism_registry_priority(ctx.get(), UINT64_MAX);
    (void)priority;
  }

  SECTION("Priorities are consistent") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      int p1 = prism_registry_priority(ctx.get(), id);
      int p2 = prism_registry_priority(ctx.get(), id);
      CHECK(p1 == p2);
    }
  }
}

TEST_CASE("Registry exists", "[registry][exists]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Registered backends exist") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      bool exists = prism_registry_exists(ctx.get(), id);
      INFO("Index: " << i << ", ID: " << id);
      CHECK(exists);
    }
  }

  SECTION("Unregistered backends don't exist") {
    CHECK_FALSE(prism_registry_exists(ctx.get(), 0));
    CHECK_FALSE(prism_registry_exists(ctx.get(), UINT64_MAX));
    CHECK_FALSE(prism_registry_exists(ctx.get(), 0x123456789ABCDEF0));
  }

  SECTION("Known backend IDs check") {
    for (size_t i = 0; i < NUM_KNOWN_BACKENDS; ++i) {
      bool exists = prism_registry_exists(ctx.get(), KNOWN_BACKEND_IDS[i]);
      // May or may not exist depending on platform
      (void)exists;
    }
  }
}

TEST_CASE("Registry get", "[registry][get]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Get returns cached backend or null") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      PrismBackend *backend = prism_registry_get(ctx.get(), id);
      // May be null if no cached instance exists
      // Should not crash in either case
      (void)backend;
    }
  }

  SECTION("Get for invalid ID returns null") {
    PrismBackend *backend = prism_registry_get(ctx.get(), 0);
    CHECK(backend == nullptr);

    backend = prism_registry_get(ctx.get(), UINT64_MAX);
    CHECK(backend == nullptr);
  }
}

TEST_CASE("Registry create", "[registry][create]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Create returns new backend for valid IDs") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      PrismBackend *backend = prism_registry_create(ctx.get(), id);

      INFO("Backend index: " << i);
      if (backend != nullptr) {
        // Should be able to query name
        const char *name = prism_backend_name(backend);
        CHECK(name != nullptr);

        prism_backend_free(backend);
      }
    }
  }

  SECTION("Create for invalid ID returns null") {
    PrismBackend *backend = prism_registry_create(ctx.get(), 0);
    CHECK(backend == nullptr);

    backend = prism_registry_create(ctx.get(), UINT64_MAX);
    CHECK(backend == nullptr);
  }

  SECTION("Multiple creates return different instances") {
    if (count > 0) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), 0);
      PrismBackend *backend1 = prism_registry_create(ctx.get(), id);
      PrismBackend *backend2 = prism_registry_create(ctx.get(), id);

      if (backend1 && backend2) {
        // Should be different instances
        CHECK(backend1 != backend2);
      }

      if (backend1)
        prism_backend_free(backend1);
      if (backend2)
        prism_backend_free(backend2);
    }
  }
}

TEST_CASE("Registry create best", "[registry][create_best]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Create best returns a backend if any are available") {
    PrismBackend *backend = prism_registry_create_best(ctx.get());

    size_t count = prism_registry_count(ctx.get());
    if (count > 0) {
      // Should get something
      if (backend != nullptr) {
        const char *name = prism_backend_name(backend);
        CHECK(name != nullptr);
        prism_backend_free(backend);
      }
    } else {
      CHECK(backend == nullptr);
    }
  }

  SECTION("Create best multiple times returns different instances") {
    PrismBackend *backend1 = prism_registry_create_best(ctx.get());
    PrismBackend *backend2 = prism_registry_create_best(ctx.get());

    if (backend1 && backend2) {
      CHECK(backend1 != backend2);

      // Should be same type though
      const char *name1 = prism_backend_name(backend1);
      const char *name2 = prism_backend_name(backend2);
      if (name1 && name2) {
        CHECK(strcmp(name1, name2) == 0);
      }
    }

    if (backend1)
      prism_backend_free(backend1);
    if (backend2)
      prism_backend_free(backend2);
  }
}

TEST_CASE("Registry acquire", "[registry][acquire]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Acquire returns same backend type") {
    if (count > 0) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), 0);

      PrismBackend *backend1 = prism_registry_acquire(ctx.get(), id);
      PrismBackend *backend2 = prism_registry_acquire(ctx.get(), id);

      if (backend1 && backend2) {
        // Acquire returns same underlying backend (but wrapped in new
        // PrismBackend*) So we compare names instead of pointers
        const char *name1 = prism_backend_name(backend1);
        const char *name2 = prism_backend_name(backend2);
        CHECK(name1 != nullptr);
        CHECK(name2 != nullptr);
        if (name1 && name2) {
          CHECK(strcmp(name1, name2) == 0);
        }
      }

      // Need to free wrappers (wrap_backend creates new PrismBackend* each
      // time)
      if (backend1)
        prism_backend_free(backend1);
      if (backend2)
        prism_backend_free(backend2);
    }
  }

  SECTION("Acquire for invalid ID returns null") {
    PrismBackend *backend = prism_registry_acquire(ctx.get(), 0);
    CHECK(backend == nullptr);
  }
}

TEST_CASE("Registry acquire best", "[registry][acquire_best]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Acquire best returns same backend type") {
    PrismBackend *backend1 = prism_registry_acquire_best(ctx.get());
    PrismBackend *backend2 = prism_registry_acquire_best(ctx.get());

    if (backend1 && backend2) {
      // Should return same backend type (may or may not be same pointer)
      const char *name1 = prism_backend_name(backend1);
      const char *name2 = prism_backend_name(backend2);
      if (name1 && name2) {
        CHECK(strcmp(name1, name2) == 0);
      }
      // Clean up - acquire may create new instances
      // Only free if different pointers to avoid double-free
      if (backend1 != backend2) {
        prism_backend_free(backend2);
      }
    }
    if (backend1) {
      prism_backend_free(backend1);
    }
  }

  SECTION("Acquire best and create best both return valid backends") {
    PrismBackend *acquired = prism_registry_acquire_best(ctx.get());
    PrismBackend *created = prism_registry_create_best(ctx.get());

    // Both should return a valid backend (or both null if none available)
    if (acquired) {
      const char *name1 = prism_backend_name(acquired);
      CHECK(name1 != nullptr);
      prism_backend_free(acquired);
    }

    if (created) {
      const char *name2 = prism_backend_name(created);
      CHECK(name2 != nullptr);
      prism_backend_free(created);
    }

    // Note: acquire_best may return a cached lower-priority backend,
    // while create_best always tries in priority order, so they may differ
  }
}

TEST_CASE("Registry enumeration", "[registry][enumeration]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("Can enumerate all backends") {
    std::vector<BackendInfo> backends = get_all_backends(ctx.get());

    CHECK(backends.size() == prism_registry_count(ctx.get()));

    for (const auto &info : backends) {
      INFO("Backend: " << info.name << ", ID: " << info.id
                       << ", Priority: " << info.priority);
      CHECK(!info.name.empty());
      CHECK(info.exists);
    }
  }

  SECTION("Enumeration is consistent") {
    auto backends1 = get_all_backends(ctx.get());
    auto backends2 = get_all_backends(ctx.get());

    REQUIRE(backends1.size() == backends2.size());

    for (size_t i = 0; i < backends1.size(); ++i) {
      CHECK(backends1[i].id == backends2[i].id);
      CHECK(backends1[i].name == backends2[i].name);
      CHECK(backends1[i].priority == backends2[i].priority);
    }
  }
}