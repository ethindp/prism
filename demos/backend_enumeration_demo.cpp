#include <atomic>
#include <csignal>
#include <print>
#include <prism.h>

static std::atomic_flag stop;

extern "C" void sighandler(int sig) {
  switch (sig) {
  case SIGTERM:
  case SIGINT: {
    stop.test_and_set();
    stop.notify_all();
  } break;
  default:
    return;
  }
}

static void
on_prism_backend_availability_changed([[maybe_unused]] void *ud,
                                      [[maybe_unused]] PrismBackendId backend,
                                      const char *name, bool available) {
  std::println("Backend {} is {}", name,
               available ? "available" : "unavailable");
}

int main() {
  std::signal(SIGTERM, &sighandler);
  std::signal(SIGINT, &sighandler);
  auto cfg = prism_config_init();
  cfg.availability_callback = &on_prism_backend_availability_changed;
  cfg.availability_poll_interval_ms = 250;
  cfg.availability_debounce_samples = 1;
  cfg.availability_auto_power_manage = true;
  auto *const ctx = prism_init(&cfg);
  if (ctx == nullptr) {
    std::println("Could not initialize prism");
    return 1;
  }
  stop.wait(false);
  prism_shutdown(ctx);
  return 0;
}