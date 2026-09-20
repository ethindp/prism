// SPDX-License-Identifier: MPL-2.0

#include "bridge.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <gio/gio.h>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <spiel/spiel.h>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

struct SpielVoiceEntry {
  std::string id;
  std::string name;
  std::string language;
};

struct SpielSpeakCommand {
  std::string text;
  bool interrupt;
  float rate;
  float pitch;
  float volume;
  std::string voice_id;
  std::string language;
};

struct SpielStopCommand {};
struct SpielPauseCommand {};
struct SpielResumeCommand {};
struct SpielRefreshVoicesCommand {};

using SpielCommand =
    std::variant<SpielSpeakCommand, SpielStopCommand, SpielPauseCommand,
                 SpielResumeCommand, SpielRefreshVoicesCommand>;

struct PrismSpielInstance {
  std::thread thread;
  GMainContext *ctx{nullptr};
  GMainLoop *loop{nullptr};
  GCancellable *cancellable{nullptr};
  SpielSpeaker *speaker{nullptr};
  GListModel *voices_model{nullptr};
  gulong speaking_handler{0};
  gulong paused_handler{0};
  gulong voices_handler{0};
  std::mutex ready_mtx;
  std::condition_variable ready_cv;
  std::optional<bool> ready;
  std::mutex queue_mtx;
  std::deque<SpielCommand> queue;
  bool wake_pending{false};
  std::atomic_flag speaking;
  std::atomic_flag paused;
  std::atomic<float> rate{0.5F};
  std::atomic<float> pitch{0.5F};
  std::atomic<float> volume{1.0F};
  std::mutex voices_mtx;
  std::vector<SpielVoiceEntry> voices;
  std::size_t voice_idx{0};
  std::atomic_flag voices_stale;

  PrismSpielInstance() = default;
  PrismSpielInstance(const PrismSpielInstance &) = delete;
  PrismSpielInstance &operator=(const PrismSpielInstance &) = delete;
  ~PrismSpielInstance() {
    if (thread.joinable()) {
      g_cancellable_cancel(cancellable);
      bool started = false;
      {
        std::unique_lock lock(ready_mtx);
        ready_cv.wait(lock, [this] { return ready.has_value(); });
        started = ready.value_or(false);
      }
      if (started)
        g_main_loop_quit(loop);
      thread.join();
    }
    if (loop != nullptr)
      g_main_loop_unref(loop);
    if (ctx != nullptr)
      g_main_context_unref(ctx);
    if (cancellable != nullptr)
      g_object_unref(cancellable);
  }
};

static constexpr std::string_view PROVIDER_SUFFIX = ".Speech.Provider";
static constexpr int DBUS_TIMEOUT_MS = 100;

static bool valid_normalized(float v) noexcept {
  return v >= 0.0F && v <= 1.0F &&
         (std::isnormal(v) || std::fpclassify(v) == FP_ZERO);
}

static PrismWinelibStatus
copy_out(std::initializer_list<std::string_view> parts, char *buf, uint32_t cap,
         uint32_t *needed) noexcept {
  std::size_t total = 0;
  for (const auto part : parts)
    total += part.size();
  if (total >= std::numeric_limits<uint32_t>::max())
    return PRISM_WINELIB_INTERNAL;
  *needed = static_cast<uint32_t>(total);
  if (buf == nullptr || cap <= total)
    return PRISM_WINELIB_BUFFER_TOO_SMALL;
  for (const auto part : parts)
    buf = std::ranges::copy(part, buf).out;
  *buf = '\0';
  return PRISM_WINELIB_OK;
}

static void rebuild_voices(PrismSpielInstance *h) noexcept try {
  if (h->voices_model == nullptr)
    return;
  std::vector<SpielVoiceEntry> fresh;
  const guint n = g_list_model_get_n_items(h->voices_model);
  for (guint i = 0; i < n; ++i) {
    std::unique_ptr<SpielVoice, decltype(&g_object_unref)> v(
        SPIEL_VOICE(g_list_model_get_object(h->voices_model, i)),
        g_object_unref);
    const char *id = spiel_voice_get_identifier(v.get());
    const char *name = spiel_voice_get_name(v.get());
    const char *const *langs = spiel_voice_get_languages(v.get());
    if (id == nullptr || name == nullptr || langs == nullptr)
      continue;
    for (const char *const *lp = langs; *lp != nullptr; ++lp)
      fresh.push_back({.id = id, .name = name, .language = *lp});
  }
  std::scoped_lock lock(h->voices_mtx);
  std::optional<std::size_t> preserved;
  if (h->voice_idx < h->voices.size()) {
    const auto &cur = h->voices[h->voice_idx];
    for (std::size_t i = 0; i < fresh.size(); ++i) {
      if (fresh[i].id == cur.id && fresh[i].language == cur.language) {
        preserved = i;
        break;
      }
    }
  }
  std::swap(h->voices, fresh);
  h->voice_idx = preserved.value_or(0);
  h->voices_stale.clear();
} catch (...) {
  h->voices_stale.test_and_set();
}

static void dispatch(PrismSpielInstance *h, const SpielCommand &cmd) {
  if (h->speaker == nullptr)
    return;
  if (const auto *speak = std::get_if<SpielSpeakCommand>(&cmd)) {
    if (speak->interrupt)
      spiel_speaker_cancel(h->speaker);
    std::unique_ptr<SpielUtterance, decltype(&g_object_unref)> u(
        spiel_utterance_new(speak->text.c_str()), g_object_unref);
    if (u == nullptr)
      return;
    spiel_utterance_set_rate(u.get(), speak->rate);
    spiel_utterance_set_pitch(u.get(), speak->pitch);
    spiel_utterance_set_volume(u.get(), speak->volume);
    if (!speak->language.empty())
      spiel_utterance_set_language(u.get(), speak->language.c_str());
    if (!speak->voice_id.empty() && h->voices_model != nullptr) {
      const guint n = g_list_model_get_n_items(h->voices_model);
      for (guint i = 0; i < n; ++i) {
        std::unique_ptr<SpielVoice, decltype(&g_object_unref)> v(
            SPIEL_VOICE(g_list_model_get_object(h->voices_model, i)),
            g_object_unref);
        const char *vid = spiel_voice_get_identifier(v.get());
        if (vid != nullptr && speak->voice_id == vid) {
          spiel_utterance_set_voice(u.get(), v.get());
          break;
        }
      }
    }
    spiel_speaker_speak(h->speaker, u.get());
  } else if (std::holds_alternative<SpielStopCommand>(cmd)) {
    spiel_speaker_cancel(h->speaker);
  } else if (std::holds_alternative<SpielPauseCommand>(cmd)) {
    spiel_speaker_pause(h->speaker);
  } else if (std::holds_alternative<SpielResumeCommand>(cmd)) {
    spiel_speaker_resume(h->speaker);
  } else if (std::holds_alternative<SpielRefreshVoicesCommand>(cmd)) {
    rebuild_voices(h);
  }
}

static gboolean on_drain_queue(gpointer ud) {
  auto *h = static_cast<PrismSpielInstance *>(ud);
  std::deque<SpielCommand> batch;
  {
    std::scoped_lock lock(h->queue_mtx);
    std::swap(batch, h->queue);
    h->wake_pending = false;
  }
  for (const auto &cmd : batch)
    dispatch(h, cmd);
  return G_SOURCE_REMOVE;
}

static PrismWinelibStatus post(PrismSpielInstance *h, SpielCommand cmd) noexcept
    try {
  bool wake = false;
  {
    std::scoped_lock lock(h->queue_mtx);
    h->queue.push_back(std::move(cmd));
    wake = !std::exchange(h->wake_pending, true);
  }
  if (wake)
    g_main_context_invoke(h->ctx, on_drain_queue, h);
  return PRISM_WINELIB_OK;
} catch (const std::bad_alloc &) {
  return PRISM_WINELIB_MEMORY;
} catch (...) {
  return PRISM_WINELIB_INTERNAL;
}

static void on_notify_speaking(GObject *obj, [[maybe_unused]] GParamSpec *spec,
                               gpointer ud) {
  gboolean v = FALSE;
  g_object_get(obj, "speaking", &v, nullptr);
  auto &flag = static_cast<PrismSpielInstance *>(ud)->speaking;
  if (v != FALSE)
    flag.test_and_set();
  else
    flag.clear();
}

static void on_notify_paused(GObject *obj, [[maybe_unused]] GParamSpec *spec,
                             gpointer ud) {
  gboolean v = FALSE;
  g_object_get(obj, "paused", &v, nullptr);
  auto &flag = static_cast<PrismSpielInstance *>(ud)->paused;
  if (v != FALSE)
    flag.test_and_set();
  else
    flag.clear();
}

static void on_voices_changed([[maybe_unused]] GListModel *model,
                              [[maybe_unused]] guint position,
                              [[maybe_unused]] guint removed,
                              [[maybe_unused]] guint added, gpointer ud) {
  rebuild_voices(static_cast<PrismSpielInstance *>(ud));
}

static void on_speaker_ready([[maybe_unused]] GObject *source,
                             GAsyncResult *result, gpointer ud) {
  auto *h = static_cast<PrismSpielInstance *>(ud);
  GError *err = nullptr;
  h->speaker = spiel_speaker_new_finish(result, &err);
  if (err != nullptr)
    g_error_free(err);
  if (h->speaker == nullptr) {
    {
      std::scoped_lock lock(h->ready_mtx);
      h->ready = false;
    }
    h->ready_cv.notify_all();
    g_main_loop_quit(h->loop);
    return;
  }
  h->speaking_handler = g_signal_connect(h->speaker, "notify::speaking",
                                         G_CALLBACK(on_notify_speaking), h);
  h->paused_handler = g_signal_connect(h->speaker, "notify::paused",
                                       G_CALLBACK(on_notify_paused), h);
  h->voices_model = spiel_speaker_get_voices(h->speaker);
  if (h->voices_model != nullptr)
    h->voices_handler = g_signal_connect(h->voices_model, "items-changed",
                                         G_CALLBACK(on_voices_changed), h);
  rebuild_voices(h);
  {
    std::scoped_lock lock(h->ready_mtx);
    h->ready = true;
  }
  h->ready_cv.notify_all();
}

static void worker(PrismSpielInstance *h) {
  g_main_context_push_thread_default(h->ctx);
  spiel_speaker_new(h->cancellable, on_speaker_ready, h);
  g_main_loop_run(h->loop);
  if (h->speaker != nullptr) {
    if (h->speaking_handler != 0)
      g_signal_handler_disconnect(h->speaker, h->speaking_handler);
    if (h->paused_handler != 0)
      g_signal_handler_disconnect(h->speaker, h->paused_handler);
    if (h->voices_model != nullptr && h->voices_handler != 0)
      g_signal_handler_disconnect(h->voices_model, h->voices_handler);
    h->voices_model = nullptr;
    g_object_unref(h->speaker);
    h->speaker = nullptr;
  }
  g_main_context_pop_thread_default(h->ctx);
}

extern "C" PRISM_WINELIB_ABI uint32_t prism_spiel_abi_version(void) noexcept {
  return PRISM_SPIEL_BRIDGE_ABI_VERSION;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_available(void) noexcept {
  GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
  if (bus == nullptr)
    return PRISM_WINELIB_NOT_AVAILABLE;
  bool found = false;
  for (const char *method : {"ListActivatableNames", "ListNames"}) {
    GError *err = nullptr;
    GVariant *reply = g_dbus_connection_call_sync(
        bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", method, nullptr, G_VARIANT_TYPE("(as)"),
        G_DBUS_CALL_FLAGS_NONE, DBUS_TIMEOUT_MS, nullptr, &err);
    if (err != nullptr)
      g_error_free(err);
    if (reply == nullptr)
      continue;
    GVariant *names = g_variant_get_child_value(reply, 0);
    GVariantIter it;
    const char *name = nullptr;
    g_variant_iter_init(&it, names);
    while (g_variant_iter_next(&it, "&s", &name) != FALSE) {
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
  return found ? PRISM_WINELIB_OK : PRISM_WINELIB_NOT_AVAILABLE;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_create(PrismSpielInstance **out) noexcept try {
  if (out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  *out = nullptr;
  auto h = std::make_unique<PrismSpielInstance>();
  h->cancellable = g_cancellable_new();
  h->ctx = g_main_context_new();
  h->loop = g_main_loop_new(h->ctx, FALSE);
  h->thread = std::thread(worker, h.get());
  bool ok = false;
  {
    std::unique_lock lock(h->ready_mtx);
    if (!h->ready_cv.wait_for(lock, std::chrono::seconds(5),
                              [&] { return h->ready.has_value(); }))
      return PRISM_WINELIB_INTERNAL;
    ok = h->ready.value_or(false);
  }
  if (!ok)
    return PRISM_WINELIB_NOT_AVAILABLE;
  *out = h.release();
  return PRISM_WINELIB_OK;
} catch (const std::bad_alloc &) {
  return PRISM_WINELIB_MEMORY;
} catch (...) {
  return PRISM_WINELIB_INTERNAL;
}

extern "C" PRISM_WINELIB_ABI void
prism_spiel_destroy(PrismSpielInstance *h) noexcept {
  delete h;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus prism_spiel_speak(
    PrismSpielInstance *h, const char *text, int32_t interrupt) noexcept try {
  if (h == nullptr || text == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  const float r = h->rate.load();
  const float p = h->pitch.load();
  SpielSpeakCommand cmd{
      .text = text,
      .interrupt = interrupt != 0,
      .rate = r <= 0.5F ? 0.1F + (r * 1.8F) : 1.0F + ((r - 0.5F) * 18.0F),
      .pitch = p * 2.0F,
      .volume = h->volume.load(),
      .voice_id = {},
      .language = {},
  };
  {
    std::scoped_lock lock(h->voices_mtx);
    if (h->voice_idx < h->voices.size()) {
      cmd.voice_id = h->voices[h->voice_idx].id;
      cmd.language = h->voices[h->voice_idx].language;
    }
  }
  return post(h, std::move(cmd));
} catch (const std::bad_alloc &) {
  return PRISM_WINELIB_MEMORY;
} catch (...) {
  return PRISM_WINELIB_INTERNAL;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_stop(PrismSpielInstance *h) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  return post(h, SpielStopCommand{});
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_pause(PrismSpielInstance *h) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (!h->speaking.test())
    return PRISM_WINELIB_NOT_SPEAKING;
  if (h->paused.test())
    return PRISM_WINELIB_ALREADY_PAUSED;
  return post(h, SpielPauseCommand{});
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_resume(PrismSpielInstance *h) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (!h->paused.test())
    return PRISM_WINELIB_NOT_PAUSED;
  return post(h, SpielResumeCommand{});
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_is_speaking(PrismSpielInstance *h, int32_t *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  *out = h->speaking.test() ? 1 : 0;
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_set_volume(PrismSpielInstance *h, float value) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (!valid_normalized(value))
    return PRISM_WINELIB_RANGE;
  h->volume.store(value);
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_volume(PrismSpielInstance *h, float *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  *out = h->volume.load();
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_set_rate(PrismSpielInstance *h, float value) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (!valid_normalized(value))
    return PRISM_WINELIB_RANGE;
  h->rate.store(value);
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_rate(PrismSpielInstance *h, float *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  *out = h->rate.load();
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_set_pitch(PrismSpielInstance *h, float value) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (!valid_normalized(value))
    return PRISM_WINELIB_RANGE;
  h->pitch.store(value);
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_pitch(PrismSpielInstance *h, float *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  *out = h->pitch.load();
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_refresh_voices(PrismSpielInstance *h) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  return post(h, SpielRefreshVoicesCommand{});
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_count_voices(PrismSpielInstance *h, uint32_t *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (h->voices_stale.test())
    return PRISM_WINELIB_MEMORY;
  std::scoped_lock lock(h->voices_mtx);
  if (h->voices.size() > std::numeric_limits<uint32_t>::max())
    return PRISM_WINELIB_INTERNAL;
  *out = static_cast<uint32_t>(h->voices.size());
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_voice_name(PrismSpielInstance *h, uint32_t index, char *buf,
                           uint32_t cap, uint32_t *needed) noexcept {
  if (h == nullptr || needed == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (h->voices_stale.test())
    return PRISM_WINELIB_MEMORY;
  std::scoped_lock lock(h->voices_mtx);
  if (index >= h->voices.size())
    return PRISM_WINELIB_RANGE;
  return copy_out({h->voices[index].name}, buf, cap, needed);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_voice_language(PrismSpielInstance *h, uint32_t index, char *buf,
                               uint32_t cap, uint32_t *needed) noexcept {
  if (h == nullptr || needed == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (h->voices_stale.test())
    return PRISM_WINELIB_MEMORY;
  std::scoped_lock lock(h->voices_mtx);
  if (index >= h->voices.size())
    return PRISM_WINELIB_RANGE;
  return copy_out({h->voices[index].language}, buf, cap, needed);
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_set_voice(PrismSpielInstance *h, uint32_t index) noexcept {
  if (h == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (h->voices_stale.test())
    return PRISM_WINELIB_MEMORY;
  std::scoped_lock lock(h->voices_mtx);
  if (index >= h->voices.size())
    return PRISM_WINELIB_RANGE;
  h->voice_idx = index;
  return PRISM_WINELIB_OK;
}

extern "C" PRISM_WINELIB_ABI PrismWinelibStatus
prism_spiel_get_voice(PrismSpielInstance *h, uint32_t *out) noexcept {
  if (h == nullptr || out == nullptr)
    return PRISM_WINELIB_INVALID_ARGUMENT;
  if (h->voices_stale.test())
    return PRISM_WINELIB_MEMORY;
  std::scoped_lock lock(h->voices_mtx);
  if (h->voice_idx >= h->voices.size())
    return PRISM_WINELIB_NO_VOICES;
  *out = static_cast<uint32_t>(h->voice_idx);
  return PRISM_WINELIB_OK;
}
