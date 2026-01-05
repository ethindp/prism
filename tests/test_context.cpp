#include "test_helpers.h"
#include <atomic>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>
#include <prism.h>
#include <thread>
#include <vector>

using namespace prism_test;

TEST_CASE("Context initialization", "[context][init]") {
  SECTION("prism_init returns valid context") {
    PrismContext *ctx = prism_init();
    REQUIRE(ctx != nullptr);
    prism_shutdown(ctx);
  }

  SECTION("Multiple contexts can be created") {
    PrismContext *ctx1 = prism_init();
    PrismContext *ctx2 = prism_init();

    REQUIRE(ctx1 != nullptr);
    REQUIRE(ctx2 != nullptr);
    // They may or may not be the same pointer depending on implementation

    prism_shutdown(ctx1);
    prism_shutdown(ctx2);
  }

  SECTION("Context is valid after creation") {
    ContextPtr ctx = make_context();
    REQUIRE(ctx.get() != nullptr);

    // Should be able to query registry count without error
    size_t count = prism_registry_count(ctx.get());
    // Count could be 0 or more depending on platform
    CHECK(count >= 0); // Always true for size_t, but documents intent
  }
}

TEST_CASE("Context shutdown", "[context][shutdown]") {
  SECTION("prism_shutdown accepts valid context") {
    PrismContext *ctx = prism_init();
    REQUIRE(ctx != nullptr);
    // Should not crash
    prism_shutdown(ctx);
  }

  SECTION("prism_shutdown accepts nullptr") {
    // Should not crash
    prism_shutdown(nullptr);
  }

  SECTION("Multiple shutdowns of nullptr") {
    // Should not crash
    prism_shutdown(nullptr);
    prism_shutdown(nullptr);
    prism_shutdown(nullptr);
  }
}

TEST_CASE("Context RAII wrapper", "[context][raii]") {
  SECTION("ContextPtr manages lifetime correctly") {
    {
      ContextPtr ctx = make_context();
      REQUIRE(ctx.get() != nullptr);
      // Context should be valid here
      size_t count = prism_registry_count(ctx.get());
      (void)count;
    }
    // Context should be automatically cleaned up here
  }

  SECTION("ContextPtr can be moved") {
    ContextPtr ctx1 = make_context();
    REQUIRE(ctx1.get() != nullptr);

    PrismContext *raw_ptr = ctx1.get();
    ContextPtr ctx2 = std::move(ctx1);

    REQUIRE(ctx1.get() == nullptr);
    REQUIRE(ctx2.get() == raw_ptr);
  }

  SECTION("ContextPtr can be reset") {
    ContextPtr ctx = make_context();
    REQUIRE(ctx.get() != nullptr);

    ctx.reset();
    REQUIRE(ctx.get() == nullptr);
  }

  SECTION("ContextPtr can be reset with new context") {
    ContextPtr ctx = make_context();
    PrismContext *old_ptr = ctx.get();
    REQUIRE(old_ptr != nullptr);

    ctx.reset(prism_init());
    REQUIRE(ctx.get() != nullptr);
    // May or may not be same pointer
  }
}

TEST_CASE("Context thread safety", "[context][threads]") {
  SECTION("Multiple threads can create contexts") {
    constexpr int NUM_THREADS = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < NUM_THREADS; ++i) {
      threads.emplace_back([&success_count]() {
        PrismContext *ctx = prism_init();
        if (ctx != nullptr) {
          success_count++;
          prism_shutdown(ctx);
        }
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    CHECK(success_count == NUM_THREADS);
  }

  SECTION(
      "Single context can be used from multiple threads for registry queries") {
    ContextPtr ctx = make_context();
    REQUIRE(ctx.get() != nullptr);

    constexpr int NUM_THREADS = 5;
    std::vector<std::thread> threads;
    std::atomic<int> query_count{0};

    for (int i = 0; i < NUM_THREADS; ++i) {
      threads.emplace_back([&ctx, &query_count]() {
        size_t count = prism_registry_count(ctx.get());
        (void)count;
        query_count++;
      });
    }

    for (auto &t : threads) {
      t.join();
    }

    CHECK(query_count == NUM_THREADS);
  }
}

TEST_CASE("Context repeated initialization", "[context][repeated]") {
  SECTION("Can create and destroy context repeatedly") {
    for (int i = 0; i < 100; ++i) {
      PrismContext *ctx = prism_init();
      REQUIRE(ctx != nullptr);
      prism_shutdown(ctx);
    }
  }

  SECTION("RAII wrapper works in loop") {
    for (int i = 0; i < 50; ++i) {
      ContextPtr ctx = make_context();
      REQUIRE(ctx.get() != nullptr);
    }
  }
}

TEST_CASE("Context memory management", "[context][memory]") {
  SECTION("No obvious memory issues with rapid create/destroy") {
    // This test is designed to catch obvious memory leaks or double-frees
    // when run under a memory sanitizer
    for (int i = 0; i < 1000; ++i) {
      ContextPtr ctx = make_context();
      if (!ctx)
        continue;

      // Do some work
      size_t count = prism_registry_count(ctx.get());
      (void)count;
    }
  }
}

TEST_CASE("Context with backends", "[context][backends]") {
  SECTION("Context maintains backend registry") {
    ContextPtr ctx = make_context();
    REQUIRE(ctx.get() != nullptr);

    size_t count = prism_registry_count(ctx.get());

    // Query multiple times - should be consistent
    for (int i = 0; i < 10; ++i) {
      REQUIRE(prism_registry_count(ctx.get()) == count);
    }
  }

  SECTION("Backend IDs are consistent across queries") {
    ContextPtr ctx = make_context();
    REQUIRE(ctx.get() != nullptr);

    size_t count = prism_registry_count(ctx.get());
    if (count > 0) {
      PrismBackendId first_id = prism_registry_id_at(ctx.get(), 0);

      for (int i = 0; i < 10; ++i) {
        REQUIRE(prism_registry_id_at(ctx.get(), 0) == first_id);
      }
    }
  }
}
