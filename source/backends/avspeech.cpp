// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#include "utils.h"
#ifdef __OBJC__
#ifdef __APPLE__
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#include <atomic>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

@interface AVSpeechSyncDelegate : NSObject <AVSpeechSynthesizerDelegate>
@property(nonatomic, strong) dispatch_semaphore_t sema;
@property(atomic, assign) bool isDone;
@end

@implementation AVSpeechSyncDelegate
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer
    didFinishSpeechUtterance:(AVSpeechUtterance *)utterance {
  self.isDone = true;
  if (self.sema) {
    dispatch_semaphore_signal(self.sema);
  }
}
- (void)speechSynthesizer:(AVSpeechSynthesizer *)synthesizer
    didCancelSpeechUtterance:(AVSpeechUtterance *)utterance {
  self.isDone = true;
  if (self.sema) {
    dispatch_semaphore_signal(self.sema);
  }
}
@end

class AVSpeechBackend final : public TextToSpeechBackend {
private:
  AVSpeechSynthesizer *synthesizer{nullptr};
  std::atomic_flag initialized;
  std::atomic<float> volume{0.5f};
  std::atomic<float> pitch{0.5f};
  std::atomic<float> rate{0.5f};

  struct VoiceInfo {
    std::string identifier;
    std::string name;
    std::string language;
  };

  std::vector<VoiceInfo> voices;
  mutable std::shared_mutex voices_lock;
  std::atomic_uint64_t voice_idx{0};

  std::atomic<std::size_t> audio_channels{1};
  std::atomic<std::size_t> audio_sample_rate{22050};
  std::atomic<std::size_t> audio_bit_depth{32};

  static inline void sync_on_main(dispatch_block_t block) {
    if ([NSThread isMainThread]) {
      block();
    } else {
      dispatch_sync(dispatch_get_main_queue(), block);
    }
  }

  static inline void wait_for_semaphore_pumping_main(dispatch_semaphore_t sema,
                                                     double timeout_sec) {
    if ([NSThread isMainThread]) {
      NSDate *start = [NSDate date];
      while (dispatch_semaphore_wait(
                 sema, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_MSEC)) !=
             0) {
        if ([[NSDate date] timeIntervalSinceDate:start] > timeout_sec)
          break;
        [[NSRunLoop currentRunLoop]
               runMode:NSDefaultRunLoopMode
            beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
      }
    } else {
      dispatch_semaphore_wait(
          sema, dispatch_time(DISPATCH_TIME_NOW,
                              (int64_t)(timeout_sec * NSEC_PER_SEC)));
    }
  }

public:
  ~AVSpeechBackend() override {
    if (initialized.test()) {
      sync_on_main(^{
        if (synthesizer) {
          [synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
          synthesizer = nil;
        }
      });
    }
  }

  [[nodiscard]] std::string_view get_name() const override {
    return "AVSpeech";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    if (NSClassFromString(@"AVSpeechSynthesizer") != nil) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    features |= SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING |
                SUPPORTS_STOP | SUPPORTS_PAUSE | SUPPORTS_RESUME |
                SUPPORTS_SET_VOLUME | SUPPORTS_GET_VOLUME | SUPPORTS_SET_RATE |
                SUPPORTS_GET_RATE | SUPPORTS_SET_PITCH | SUPPORTS_GET_PITCH |
                SUPPORTS_REFRESH_VOICES | SUPPORTS_COUNT_VOICES |
                SUPPORTS_GET_VOICE_NAME | SUPPORTS_GET_VOICE_LANGUAGE |
                SUPPORTS_GET_VOICE | SUPPORTS_SET_VOICE |
                SUPPORTS_GET_CHANNELS | SUPPORTS_GET_SAMPLE_RATE |
                SUPPORTS_GET_BIT_DEPTH;
    if (@available(macOS 10.15, iOS 13.0, *)) {
      features |= SUPPORTS_SPEAK_TO_MEMORY |
                  PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY;
    }
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    __block bool success = false;
    sync_on_main(^{
      synthesizer = [[AVSpeechSynthesizer alloc] init];
      success = (synthesizer != nil);
    });
    if (!success) {
      return std::unexpected(BackendError::BackendNotAvailable);
    }
    if (auto const res = refresh_voices(); !res) {
      return res;
    }
    if (@available(macOS 10.15, iOS 13.0, *)) {
      __block AVAudioFormat *probedFormat = nil;
      __block AVSpeechSynthesizer *probeSynth = nil;
      AVSpeechSyncDelegate *probeDelegate = [[AVSpeechSyncDelegate alloc] init];
      dispatch_semaphore_t probe_sema = dispatch_semaphore_create(0);
      probeDelegate.sema = probe_sema;
      sync_on_main(^{
        probeSynth = [[AVSpeechSynthesizer alloc] init];
        probeSynth.delegate = probeDelegate;
        AVSpeechUtterance *probeUtt =
            [AVSpeechUtterance speechUtteranceWithString:@" "];
        [probeSynth writeUtterance:probeUtt
                  toBufferCallback:^(AVAudioBuffer *buffer) {
                    if (probedFormat == nil &&
                        [buffer isKindOfClass:[AVAudioPCMBuffer class]]) {
                      probedFormat = [(AVAudioPCMBuffer *)buffer format];
                    }
                  }];
      });
      wait_for_semaphore_pumping_main(probe_sema, 1.0);
      if (probedFormat) {
        audio_channels.store(probedFormat.channelCount,
                             std::memory_order_release);
        audio_sample_rate.store(
            static_cast<std::size_t>(probedFormat.sampleRate),
            std::memory_order_release);
        audio_bit_depth.store(
            32, std::memory_order_release); // floatChannelData is inherently
                                            // 32-bit floating point.
      }
      sync_on_main(^{
        [probeSynth stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
      });
    }
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size())) {
      return std::unexpected(BackendError::InvalidUtf8);
    }
    if (interrupt) {
      if (auto const res = stop(); !res)
        return res;
    }
    NSString *nsText = [[NSString alloc] initWithBytes:text.data()
                                                length:text.size()
                                              encoding:NSUTF8StringEncoding];
    if (!nsText)
      return std::unexpected(BackendError::InvalidUtf8);
    std::size_t v_idx = voice_idx.load(std::memory_order_acquire);
    float current_vol = volume.load(std::memory_order_acquire);
    float current_pitch = pitch.load(std::memory_order_acquire);
    float current_rate = rate.load(std::memory_order_acquire);
    std::string target_voice_id;
    {
      std::shared_lock sl(voices_lock);
      if (v_idx < voices.size()) {
        target_voice_id = voices[v_idx].identifier;
      }
    }
    sync_on_main(^{
      AVSpeechUtterance *utterance =
          [AVSpeechUtterance speechUtteranceWithString:nsText];
      utterance.volume = current_vol;
      float pitchMultiplier = (current_pitch < 0.5f)
                                  ? (0.5f + current_pitch)
                                  : (1.0f + (current_pitch - 0.5f) * 2.0f);
      utterance.pitchMultiplier = pitchMultiplier;
      utterance.rate = AVSpeechUtteranceMinimumSpeechRate +
                       (current_rate * (AVSpeechUtteranceMaximumSpeechRate -
                                        AVSpeechUtteranceMinimumSpeechRate));
      if (!target_voice_id.empty()) {
        NSString *nsId =
            [NSString stringWithUTF8String:target_voice_id.c_str()];
        AVSpeechSynthesisVoice *v =
            [AVSpeechSynthesisVoice voiceWithIdentifier:nsId];
        if (v) {
          utterance.voice = v;
        }
      }
      [synthesizer speakUtterance:utterance];
    });
    return {};
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size()))
      return std::unexpected(BackendError::InvalidUtf8);
    if (!callback)
      return std::unexpected(BackendError::InvalidParam);
    NSString *nsText = [[NSString alloc] initWithBytes:text.data()
                                                length:text.size()
                                              encoding:NSUTF8StringEncoding];
    if (!nsText)
      return std::unexpected(BackendError::InvalidUtf8);
    std::size_t v_idx = voice_idx.load(std::memory_order_acquire);
    float current_vol = volume.load(std::memory_order_acquire);
    float current_pitch = pitch.load(std::memory_order_acquire);
    float current_rate = rate.load(std::memory_order_acquire);
    std::string target_voice_id;
    {
      std::shared_lock sl(voices_lock);
      if (v_idx < voices.size()) {
        target_voice_id = voices[v_idx].identifier;
      }
    }
    __block BackendResult<> result = {};
    __block std::vector<float> audio_data;
    __block std::mutex audio_mtx;
    __block std::size_t out_channels = 1;
    __block std::size_t out_sample_rate = 22050;
    if (@available(macOS 10.15, iOS 13.0, *)) {
      __block AVSpeechSynthesizer *memSynth = nil;
      AVSpeechSyncDelegate *memDelegate = [[AVSpeechSyncDelegate alloc] init];
      dispatch_semaphore_t sema = dispatch_semaphore_create(0);
      memDelegate.sema = sema;
      sync_on_main(^{
        memSynth = [[AVSpeechSynthesizer alloc] init];
        memSynth.delegate = memDelegate;
        AVSpeechUtterance *utterance =
            [AVSpeechUtterance speechUtteranceWithString:nsText];
        utterance.volume = current_vol;
        float pitchMultiplier = (current_pitch < 0.5f)
                                    ? (0.5f + current_pitch)
                                    : (1.0f + (current_pitch - 0.5f) * 2.0f);
        utterance.pitchMultiplier = pitchMultiplier;
        utterance.rate = AVSpeechUtteranceMinimumSpeechRate +
                         (current_rate * (AVSpeechUtteranceMaximumSpeechRate -
                                          AVSpeechUtteranceMinimumSpeechRate));
        if (!target_voice_id.empty()) {
          NSString *nsId =
              [NSString stringWithUTF8String:target_voice_id.c_str()];
          AVSpeechSynthesisVoice *v =
              [AVSpeechSynthesisVoice voiceWithIdentifier:nsId];
          if (v)
            utterance.voice = v;
        }
        [memSynth writeUtterance:utterance
                toBufferCallback:^(AVAudioBuffer *_Nonnull buffer) {
                  if (![buffer isKindOfClass:[AVAudioPCMBuffer class]])
                    return;
                  AVAudioPCMBuffer *pcmBuffer = (AVAudioPCMBuffer *)buffer;
                  float *const *floatData = pcmBuffer.floatChannelData;
                  if (!floatData)
                    return;
                  std::size_t frames = pcmBuffer.frameLength;
                  std::size_t channels = pcmBuffer.format.channelCount;
                  std::size_t sampleRate =
                      static_cast<std::size_t>(pcmBuffer.format.sampleRate);
                  std::lock_guard<std::mutex> lock(audio_mtx);
                  out_channels = channels;
                  out_sample_rate = sampleRate;
                  std::size_t current_size = audio_data.size();
                  audio_data.resize(current_size + frames * channels);
                  if (channels == 1) {
                    std::copy(floatData[0], floatData[0] + frames,
                              audio_data.begin() + current_size);
                  } else {
                    for (std::size_t f = 0; f < frames; f++) {
                      for (std::size_t ch = 0; ch < channels; ch++) {
                        audio_data[current_size + f * channels + ch] =
                            floatData[ch][f];
                      }
                    }
                  }
                }];
      });
      wait_for_semaphore_pumping_main(
          sema, 60.0 * 5.0 /* robust 5-min internal timeout */);
      if (!audio_data.empty()) {
        auto const tv = trim_silence_rms_gate_inplace(
            std::span<float>(audio_data), out_channels, out_sample_rate);
        callback(userdata, tv.view.data(), tv.view.size() / out_channels,
                 out_channels, out_sample_rate);
      }
    } else {
      result = std::unexpected(BackendError::NotImplemented);
    }
    return result;
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    __block bool speaking = false;
    sync_on_main(^{
      speaking = synthesizer.isSpeaking;
    });
    return speaking;
  }

  BackendResult<> stop() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    sync_on_main(^{
      [synthesizer stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    });
    return {};
  }

  BackendResult<> pause() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    sync_on_main(^{
      [synthesizer pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    });
    return {};
  }

  BackendResult<> resume() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    sync_on_main(^{
      [synthesizer continueSpeaking];
    });
    return {};
  }

  BackendResult<> set_volume(float vol) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (vol < 0.0f || vol > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    volume.store(vol, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_volume() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return volume.load(std::memory_order_acquire);
  }

  BackendResult<> set_rate(float r) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (r < 0.0f || r > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    rate.store(r, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_rate() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return rate.load(std::memory_order_acquire);
  }

  BackendResult<> set_pitch(float p) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (p < 0.0f || p > 1.0f)
      return std::unexpected(BackendError::RangeOutOfBounds);
    pitch.store(p, std::memory_order_release);
    return {};
  }

  BackendResult<float> get_pitch() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return pitch.load(std::memory_order_acquire);
  }

  BackendResult<> refresh_voices() override {
    __block NSArray<AVSpeechSynthesisVoice *> *apple_voices = nil;
    sync_on_main(^{
      apple_voices = [AVSpeechSynthesisVoice speechVoices];
    });
    std::vector<VoiceInfo> new_voices;
    new_voices.reserve(apple_voices.count);
    for (AVSpeechSynthesisVoice *v in apple_voices) {
      NSString *vName = v.name ?: @"Unknown";
      NSString *vLang = v.language ?: @"en-US";
      std::string cId(v.identifier ? v.identifier.UTF8String : "");
      std::string cName(vName.UTF8String);
      std::string cLang(vLang.UTF8String);
      new_voices.emplace_back(VoiceInfo{.identifier = std::move(cId),
                                        .name = std::move(cName),
                                        .language = std::move(cLang)});
    }
    {
      std::unique_lock ul(voices_lock);
      voices = std::move(new_voices);
    }
    __block NSString *current_lang = nil;
    sync_on_main(^{
      current_lang = [AVSpeechSynthesisVoice currentLanguageCode];
    });
    std::string default_lang = current_lang ? current_lang.UTF8String : "en-US";
    std::size_t default_idx = 0;
    {
      std::shared_lock sl(voices_lock);
      for (std::size_t i = 0; i < voices.size(); ++i) {
        if (voices[i].language == default_lang) {
          default_idx = i;
          break;
        }
      }
    }
    voice_idx.store(default_idx, std::memory_order_release);
    return {};
  }

  BackendResult<std::size_t> count_voices() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    return voices.size();
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].name;
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    return voices[id].language;
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    std::shared_lock sl(voices_lock);
    if (id >= voices.size())
      return std::unexpected(BackendError::RangeOutOfBounds);
    voice_idx.store(id, std::memory_order_release);
    return {};
  }

  BackendResult<std::size_t> get_voice() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return voice_idx.load(std::memory_order_acquire);
  }

  BackendResult<std::size_t> get_channels() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_channels.load(std::memory_order_acquire);
  }

  BackendResult<std::size_t> get_sample_rate() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_sample_rate.load(std::memory_order_acquire);
  }

  BackendResult<std::size_t> get_bit_depth() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    return audio_bit_depth.load(std::memory_order_acquire);
  }
};

REGISTER_BACKEND_WITH_ID(AVSpeechBackend, Backends::AVSpeech, "AVSpeech", 97);
#endif
#endif