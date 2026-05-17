// SPDX-License-Identifier: MPL-2.0

#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#include <concurrentqueue/concurrentqueue.h>
#include <simdutf/simdutf.h>
#if (defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) ||      \
     defined(__OpenBSD__) || defined(__DragonFly__)) &&                        \
    !defined(__ANDROID__)
#ifndef NO_LIBSPIEL
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <gio/gio.h>
#include <memory>
#include <mutex>
#include <optional>
#include <spiel/spiel.h>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {
constexpr std::string_view PROVIDER_SUFFIX = ".Speech.Provider";

template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

struct VoiceInfo {
  std::string id;
  std::string name;
  std::string language;
};

using VoiceList = std::vector<VoiceInfo>;
using VoiceListPtr = std::shared_ptr<const VoiceList>;

struct SpeakCommand {
  std::string text;
  bool interrupt;
  float native_rate;
  float native_pitch;
  float native_volume;
  std::string voice_id;
  std::string language;
};

struct StopCommand {};
struct PauseCommand {};
struct ResumeCommand {};
struct RefreshVoicesCommand {};
struct ShutdownCommand {};

using Command =
    std::variant<SpeakCommand, StopCommand, PauseCommand, ResumeCommand,
                 RefreshVoicesCommand, ShutdownCommand>;

bool valid_normalized(float v) {
  return v >= 0.0F && v <= 1.0F &&
         (std::isnormal(v) || std::fpclassify(v) == FP_ZERO);
}
} // namespace

class SpielBackend final : public TextToSpeechBackend {
private:
  std::jthread thread;
  GMainContext *worker_ctx{nullptr};
  GMainLoop *worker_loop{nullptr};
  GCancellable *init_cancellable{nullptr};
  SpielSpeaker *speaker{nullptr};
  gulong notify_speaking_handler{0};
  gulong notify_paused_handler{0};
  gulong voices_changed_handler{0};
  std::mutex ready_mtx;
  std::condition_variable ready_cv;
  std::optional<bool> ready;
  moodycamel::ConcurrentQueue<Command> command_queue;
  std::atomic_bool wake_pending;
  std::atomic_flag initialized;
  std::atomic_bool speaking;
  std::atomic_bool paused;
  std::atomic_size_t voice_idx{0};
  std::atomic<float> rate{0.5};
  std::atomic<float> pitch{0.5};
  std::atomic<float> volume{1.0}; // libspiel default is max (1.0)
  std::atomic<VoiceListPtr> voices_snapshot;

  static gboolean on_drain_queue(gpointer ud) {
    auto *self = static_cast<SpielBackend *>(ud);
    self->wake_pending.store(false);
    Command cmd;
    while (self->command_queue.try_dequeue(cmd))
      self->dispatch(cmd);
    return G_SOURCE_REMOVE;
  }

  void dispatch(const Command &cmd) {
    std::visit(
        overloaded{
            [this](const SpeakCommand &c) {
              if (speaker == nullptr)
                return;
              if (c.interrupt)
                spiel_speaker_cancel(speaker);
              SpielUtterance *u = spiel_utterance_new(c.text.c_str());
              if (u == nullptr)
                return;
              spiel_utterance_set_rate(u, c.native_rate);
              spiel_utterance_set_pitch(u, c.native_pitch);
              spiel_utterance_set_volume(u, c.native_volume);
              if (!c.language.empty())
                spiel_utterance_set_language(u, c.language.c_str());
              if (!c.voice_id.empty()) {
                if (SpielVoice *v = find_voice_by_id(c.voice_id);
                    v != nullptr) {
                  spiel_utterance_set_voice(u, v);
                  g_object_unref(v);
                }
              }
              spiel_speaker_speak(speaker, u);
              g_object_unref(u);
            },
            [this](const StopCommand &) {
              if (speaker != nullptr)
                spiel_speaker_cancel(speaker);
            },
            [this](const PauseCommand &) {
              if (speaker != nullptr)
                spiel_speaker_pause(speaker);
            },
            [this](const ResumeCommand &) {
              if (speaker != nullptr)
                spiel_speaker_resume(speaker);
            },
            [this](const RefreshVoicesCommand &) { rebuild_voice_snapshot(); },
            [this](const ShutdownCommand &) {
              if (worker_loop != nullptr)
                g_main_loop_quit(worker_loop);
            }},
        cmd);
  }

  SpielVoice *find_voice_by_id(std::string_view id) {
    if (speaker == nullptr)
      return nullptr;
    GListModel *model = spiel_speaker_get_voices(speaker);
    const guint n = g_list_model_get_n_items(model);
    for (guint i = 0; i < n; ++i) {
      auto *v = SPIEL_VOICE(g_list_model_get_object(model, i));
      const char *vid = spiel_voice_get_identifier(v);
      if (vid != nullptr && id == vid)
        return v;
      g_object_unref(v);
    }
    return nullptr;
  }

  void rebuild_voice_snapshot() {
    if (speaker == nullptr)
      return;
    auto new_list = std::make_shared<VoiceList>();
    GListModel *model = spiel_speaker_get_voices(speaker);
    const guint n = g_list_model_get_n_items(model);
    for (guint i = 0; i < n; ++i) {
      auto *v = SPIEL_VOICE(g_list_model_get_object(model, i));
      const char *id = spiel_voice_get_identifier(v);
      const char *name = spiel_voice_get_name(v);
      const char *const *langs = spiel_voice_get_languages(v);
      if (id != nullptr && name != nullptr && langs != nullptr) {
        for (const char *const *lp = langs; *lp != nullptr; ++lp) {
          new_list->push_back({.id = id, .name = name, .language = *lp});
        }
      }
      g_object_unref(v);
    }
    std::optional<std::pair<std::string, std::string>> preserve;
    if (auto old = voices_snapshot.load(std::memory_order_acquire); old) {
      const size_t i = voice_idx.load(std::memory_order_acquire);
      if (i < old->size())
        preserve.emplace((*old)[i].id, (*old)[i].language);
    }
    VoiceListPtr new_ptr =
        std::shared_ptr<const VoiceList>(std::move(new_list));
    voices_snapshot.store(new_ptr);
    if (preserve.has_value()) {
      for (size_t i = 0; i < new_ptr->size(); ++i) {
        if ((*new_ptr)[i].id == preserve->first &&
            (*new_ptr)[i].language == preserve->second) {
          voice_idx.store(i, std::memory_order_release);
          return;
        }
      }
    }
    if (voice_idx.load(std::memory_order_acquire) >= new_ptr->size())
      voice_idx.store(0, std::memory_order_release);
  }

  static void on_notify_speaking(GObject *obj,
                                 [[maybe_unused]] GParamSpec *spec,
                                 gpointer ud) {
    gboolean v = FALSE;
    g_object_get(obj, "speaking", &v, nullptr);
    static_cast<SpielBackend *>(ud)->speaking.store(static_cast<bool>(v),
                                                    std::memory_order_release);
  }

  static void on_notify_paused(GObject *obj, [[maybe_unused]] GParamSpec *spec,
                               gpointer ud) {
    gboolean v = FALSE;
    g_object_get(obj, "paused", &v, nullptr);
    static_cast<SpielBackend *>(ud)->paused.store(static_cast<bool>(v),
                                                  std::memory_order_release);
  }

  static void on_voices_items_changed([[maybe_unused]] GListModel *model,
                                      [[maybe_unused]] guint position,
                                      [[maybe_unused]] guint removed,
                                      [[maybe_unused]] guint added,
                                      gpointer ud) {
    static_cast<SpielBackend *>(ud)->rebuild_voice_snapshot();
  }

  static void on_speaker_ready([[maybe_unused]] GObject *obj,
                               GAsyncResult *result, gpointer ud) {
    auto *self = static_cast<SpielBackend *>(ud);
    GError *err = nullptr;
    self->speaker = spiel_speaker_new_finish(result, &err);
    if (err != nullptr || self->speaker == nullptr) {
      if (err != nullptr)
        g_error_free(err);
      {
        std::scoped_lock g(self->ready_mtx);
        self->ready = false;
      }
      self->ready_cv.notify_all();
      g_main_loop_quit(self->worker_loop);
      return;
    }
    self->notify_speaking_handler =
        g_signal_connect(self->speaker, "notify::speaking",
                         G_CALLBACK(&on_notify_speaking), self);
    self->notify_paused_handler = g_signal_connect(
        self->speaker, "notify::paused", G_CALLBACK(&on_notify_paused), self);
    if (GListModel *vm = spiel_speaker_get_voices(self->speaker);
        vm != nullptr) {
      self->voices_changed_handler = g_signal_connect(
          vm, "items-changed", G_CALLBACK(&on_voices_items_changed), self);
    }
    self->rebuild_voice_snapshot();
    {
      std::scoped_lock g(self->ready_mtx);
      self->ready = true;
    }
    self->ready_cv.notify_all();
  }

  void thread_proc([[maybe_unused]] const std::stop_token &st) {
    worker_ctx = g_main_context_new();
    g_main_context_push_thread_default(worker_ctx);
    worker_loop = g_main_loop_new(worker_ctx, FALSE);
    spiel_speaker_new(init_cancellable, &on_speaker_ready, this);
    g_main_loop_run(worker_loop);
    if (speaker != nullptr) {
      if (notify_speaking_handler != 0)
        g_signal_handler_disconnect(speaker, notify_speaking_handler);
      if (notify_paused_handler != 0)
        g_signal_handler_disconnect(speaker, notify_paused_handler);
      if (voices_changed_handler != 0) {
        if (GListModel *vm = spiel_speaker_get_voices(speaker); vm != nullptr)
          g_signal_handler_disconnect(vm, voices_changed_handler);
      }
      g_object_unref(speaker);
      speaker = nullptr;
    }
    g_main_loop_unref(worker_loop);
    worker_loop = nullptr;
    g_main_context_pop_thread_default(worker_ctx);
    g_main_context_unref(worker_ctx);
    worker_ctx = nullptr;
  }

  void post(Command cmd) {
    command_queue.enqueue(std::move(cmd));
    if (!wake_pending.exchange(true, std::memory_order_acq_rel) &&
        worker_ctx != nullptr) {
      g_main_context_invoke(worker_ctx, &SpielBackend::on_drain_queue, this);
    }
  }

public:
  ~SpielBackend() override {
    if (initialized.test(std::memory_order_acquire))
      post(ShutdownCommand{});
    if (init_cancellable != nullptr) {
      g_object_unref(init_cancellable);
      init_cancellable = nullptr;
    }
  }

  [[nodiscard]] std::string_view get_name() const override { return "Spiel"; }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> f;
    bool found = false;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
    if (bus != nullptr) {
      for (const char *method : {"ListActivatableNames", "ListNames"}) {
        GError *err = nullptr;
        GVariant *reply = g_dbus_connection_call_sync(
            bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
            "org.freedesktop.DBus", method, nullptr, G_VARIANT_TYPE("(as)"),
            G_DBUS_CALL_FLAGS_NONE, 100, nullptr, &err);
        if (err != nullptr) {
          g_error_free(err);
          continue;
        }
        if (reply == nullptr)
          continue;
        GVariant *names = g_variant_get_child_value(reply, 0);
        GVariantIter it;
        const char *name = nullptr;
        g_variant_iter_init(&it, names);
        while (g_variant_iter_loop(&it, "&s", &name) != FALSE) {
          if (std::string_view{name}.ends_with(PROVIDER_SUFFIX)) {
            found = true;
            break;
          }
        }
        g_variant_unref(names);
        g_variant_unref(reply);
        if (found)
          break;
      }
      g_object_unref(bus);
    }
    if (found)
      f |= IS_SUPPORTED_AT_RUNTIME;
    f |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_STOP | SUPPORTS_PAUSE |
         SUPPORTS_RESUME | SUPPORTS_IS_SPEAKING | SUPPORTS_SET_RATE |
         SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH | SUPPORTS_GET_PITCH |
         SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME | SUPPORTS_REFRESH_VOICES |
         SUPPORTS_COUNT_VOICES | SUPPORTS_GET_VOICE_NAME |
         SUPPORTS_GET_VOICE_LANGUAGE | SUPPORTS_GET_VOICE | SUPPORTS_SET_VOICE;
    return f;
  }

  BackendResult<> initialize() override {
    if (initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::AlreadyInitialized);
    init_cancellable = g_cancellable_new();
    std::unique_lock lock(ready_mtx);
    ready.reset();
    thread =
        std::jthread([this](const std::stop_token &st) { thread_proc(st); });
    const bool got_signal = ready_cv.wait_for(
        lock, std::chrono::seconds(5), [this] { return ready.has_value(); });
    if (!got_signal) {
      g_cancellable_cancel(init_cancellable);
      ready_cv.wait(lock, [this] { return ready.has_value(); });
      return std::unexpected(BackendError::InternalBackendError);
    }
    if (ready && !*ready)
      return std::unexpected(BackendError::BackendNotAvailable);
    initialized.test_and_set(std::memory_order_release);
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    std::string voice_id;
    std::string language;
    const auto r = rate.load();
    const auto p = pitch.load();
    const auto v = volume.load();
    if (auto snap = voices_snapshot.load(std::memory_order_acquire); snap) {
      const size_t i = voice_idx.load(std::memory_order_acquire);
      if (i < snap->size()) {
        voice_id = (*snap)[i].id;
        language = (*snap)[i].language;
      }
    }
    post(SpeakCommand{
        .text = std::string{text},
        .interrupt = interrupt,
        .native_rate =
            range_convert_midpoint(r, 0.0F, 0.5F, 1.0F, 0.1F, 1.0F, 10.0F),
        .native_pitch =
            range_convert_midpoint(p, 0.0F, 0.5F, 1.0F, 0.0F, 1.0F, 2.0F),
        .native_volume = v,
        .voice_id = std::move(voice_id),
        .language = std::move(language),
    });
    return {};
  }

  BackendResult<> output(std::string_view t, bool i) override {
    return speak(t, i);
  }

  BackendResult<> stop() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    post(StopCommand{});
    return {};
  }

  BackendResult<> pause() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    if (!speaking.load())
      return std::unexpected(BackendError::NotSpeaking);
    if (paused.load())
      return std::unexpected(BackendError::AlreadyPaused);
    post(PauseCommand{});
    return {};
  }

  BackendResult<> resume() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    if (!paused.load())
      return std::unexpected(BackendError::NotPaused);
    post(ResumeCommand{});
    return {};
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    return speaking.load();
  }

  BackendResult<> set_rate(float v) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    if (!valid_normalized(v))
      return std::unexpected(BackendError::RangeOutOfBounds);
    rate.store(v, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    return rate.load();
  }

  BackendResult<> set_pitch(float v) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    if (!valid_normalized(v))
      return std::unexpected(BackendError::RangeOutOfBounds);
    pitch.store(v, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    return pitch.load();
  }

  BackendResult<> set_volume(float v) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    if (!valid_normalized(v))
      return std::unexpected(BackendError::RangeOutOfBounds);
    volume.store(v, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    return volume.load();
  }

  BackendResult<> refresh_voices() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    post(RefreshVoicesCommand{});
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    const auto snap = voices_snapshot.load();
    return snap ? snap->size() : std::size_t{0};
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    const auto snap = voices_snapshot.load();
    if (snap == nullptr || id >= snap->size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return (*snap)[id].name;
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    const auto snap = voices_snapshot.load(std::memory_order_acquire);
    if (snap == nullptr || id >= snap->size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return (*snap)[id].language;
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    const auto snap = voices_snapshot.load(std::memory_order_acquire);
    if (snap == nullptr || id >= snap->size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    voice_idx.store(id, std::memory_order_release);
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!initialized.test(std::memory_order_acquire))
      return std::unexpected(BackendError::NotInitialized);
    const auto snap = voices_snapshot.load(std::memory_order_acquire);
    if (snap == nullptr || snap->empty())
      return std::unexpected(BackendError::InternalBackendError);
    const size_t i = voice_idx.load(std::memory_order_acquire);
    if (i >= snap->size())
      return std::unexpected(BackendError::InternalBackendError);
    return i;
  }
};

REGISTER_BACKEND_WITH_ID(SpielBackend, Backends::Spiel, "Spiel", 96);
#endif
#endif