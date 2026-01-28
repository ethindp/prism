// SPDX-License-Identifier: MPL-2.0

import AVFoundation
import Foundation

private enum SpeechError: Int32 {
  case ok = 0
  case notInitialized
  case invalidParam
  case notImplemented
  case noVoices
  case voiceNotFound
  case speakFailed
  case memoryFailed
  case unknown
}
@inline(__always)
private func onMainActor<T>(_ body: @MainActor () -> T) -> T {
  if Thread.isMainThread {
    return MainActor.assumeIsolated { body() }
  } else {
    return DispatchQueue.main.sync {
      MainActor.assumeIsolated { body() }
    }
  }
}
@MainActor
final class AVSpeechContextImpl: NSObject, AVSpeechSynthesizerDelegate {
  let synthesizer = AVSpeechSynthesizer()
  var volume: Float = 0.5
  var pitch: Float = 1.0
  var rate: Float = AVSpeechUtteranceDefaultSpeechRate
  var currentVoiceIndex: Int?
  var voices: [AVSpeechSynthesisVoice] = []
  var outputChannels: Int32 = 1
  var outputSampleRate: Int32 = 22050
  var outputBitDepth: Int32 = 32
  var audioCallback:
    (@convention(c) (UnsafeMutableRawPointer?, UnsafePointer<Float>?, Int, Int32, Int32) -> Void)?
  var audioUserdata: UnsafeMutableRawPointer?
  private var stringCache: [String: UnsafeMutablePointer<CChar>] = [:]

  override init() {
    super.init()
    synthesizer.delegate = self
    refreshVoices()
  }

  deinit {
    for ptr in stringCache.values {
      free(ptr)
    }
  }

  func refreshVoices() {
    voices = AVSpeechSynthesisVoice.speechVoices()
  }

  func cachedCString(_ string: String) -> UnsafePointer<CChar> {
    if let cached = stringCache[string] {
      return UnsafePointer(cached)
    }
    let ptr = strdup(string)!
    stringCache[string] = ptr
    return UnsafePointer(ptr)
  }

  func makeUtterance(for text: String) -> AVSpeechUtterance {
    let utterance = AVSpeechUtterance(string: text)
    utterance.volume = volume
    utterance.pitchMultiplier = pitch
    utterance.rate = rate
    if let index = currentVoiceIndex, voices.indices.contains(index) {
      utterance.voice = voices[index]
    }
    return utterance
  }

  func speechSynthesizer(_ synthesizer: AVSpeechSynthesizer, didFinish utterance: AVSpeechUtterance)
  {
    audioCallback = nil
    audioUserdata = nil
  }

  func speechSynthesizer(_ synthesizer: AVSpeechSynthesizer, didCancel utterance: AVSpeechUtterance)
  {
    audioCallback = nil
    audioUserdata = nil
  }
}
private func withContext(
  _ ctx: UnsafeMutableRawPointer?, _ body: @MainActor (AVSpeechContextImpl) -> SpeechError
) -> Int32 {
  guard let ctx else { return SpeechError.notInitialized.rawValue }
  return onMainActor {
    body(Unmanaged<AVSpeechContextImpl>.fromOpaque(ctx).takeUnretainedValue())
  }.rawValue
}
@_cdecl("avspeech_initialize")
func avspeech_initialize(_ ctx: UnsafeMutablePointer<UnsafeMutableRawPointer?>) -> Int32 {
  onMainActor {
    ctx.pointee = Unmanaged.passRetained(AVSpeechContextImpl()).toOpaque()
  }
  return SpeechError.ok.rawValue
}
@_cdecl("avspeech_cleanup")
func avspeech_cleanup(_ ctx: UnsafeMutableRawPointer?) -> Int32 {
  guard let ctx else { return SpeechError.notInitialized.rawValue }
  onMainActor {
    Unmanaged<AVSpeechContextImpl>.fromOpaque(ctx).release()
  }
  return SpeechError.ok.rawValue
}
@_cdecl("avspeech_speak")
func avspeech_speak(_ ctx: UnsafeMutableRawPointer?, _ text: UnsafePointer<CChar>?) -> Int32 {
  withContext(ctx) { impl in
    guard let text else { return .invalidParam }
    impl.synthesizer.speak(impl.makeUtterance(for: String(cString: text)))
    return .ok
  }
}
@_cdecl("avspeech_speak_to_memory")
func avspeech_speak_to_memory(
  _ ctx: UnsafeMutableRawPointer?, _ text: UnsafePointer<CChar>?,
  _ callback: (
    @convention(c) (UnsafeMutableRawPointer?, UnsafePointer<Float>?, Int, Int32, Int32) ->
      Void
  )?, _ userdata: UnsafeMutableRawPointer?
) -> Int32 {
  withContext(ctx) { impl in
    guard let text, let callback else { return .invalidParam }
    impl.audioCallback = callback
    impl.audioUserdata = userdata
    impl.synthesizer.write(impl.makeUtterance(for: String(cString: text))) { buffer in
      onMainActor {
        guard let callback = impl.audioCallback,
          let pcmBuffer = buffer as? AVAudioPCMBuffer,
          let floatData = pcmBuffer.floatChannelData
        else { return }
        let frameCount = Int(pcmBuffer.frameLength)
        let channelCount = Int32(pcmBuffer.format.channelCount)
        let sampleRate = Int32(pcmBuffer.format.sampleRate)
        impl.outputChannels = channelCount
        impl.outputSampleRate = sampleRate
        if channelCount == 1 {
          callback(impl.audioUserdata, floatData[0], frameCount, channelCount, sampleRate)
        } else {
          var interleaved = [Float](repeating: 0, count: frameCount * Int(channelCount))
          for frame in 0..<frameCount {
            for ch in 0..<Int(channelCount) {
              interleaved[frame * Int(channelCount) + ch] = floatData[ch][frame]
            }
          }
          interleaved.withUnsafeBufferPointer { ptr in
            callback(impl.audioUserdata, ptr.baseAddress, frameCount, channelCount, sampleRate)
          }
        }
      }
    }
    return .ok
  }
}
@_cdecl("avspeech_stop")
func avspeech_stop(_ ctx: UnsafeMutableRawPointer?) -> Int32 {
  withContext(ctx) { impl in
    impl.synthesizer.stopSpeaking(at: .immediate)
    return .ok
  }
}
@_cdecl("avspeech_pause")
func avspeech_pause(_ ctx: UnsafeMutableRawPointer?) -> Int32 {
  withContext(ctx) { impl in
    impl.synthesizer.pauseSpeaking(at: .immediate)
    return .ok
  }
}
@_cdecl("avspeech_resume")
func avspeech_resume(_ ctx: UnsafeMutableRawPointer?) -> Int32 {
  withContext(ctx) { impl in
    impl.synthesizer.continueSpeaking()
    return .ok
  }
}
@_cdecl("avspeech_is_speaking")
func avspeech_is_speaking(_ ctx: UnsafeMutableRawPointer?) -> Bool {
  guard let ctx else { return false }
  return onMainActor {
    Unmanaged<AVSpeechContextImpl>.fromOpaque(ctx).takeUnretainedValue().synthesizer.isSpeaking
  }
}
@_cdecl("avspeech_set_volume")
func avspeech_set_volume(_ ctx: UnsafeMutableRawPointer?, _ volume: Float) -> Int32 {
  withContext(ctx) { impl in
    impl.volume = volume
    return .ok
  }
}
@_cdecl("avspeech_get_volume")
func avspeech_get_volume(_ ctx: UnsafeMutableRawPointer?, _ volume: UnsafeMutablePointer<Float>?)
  -> Int32
{
  withContext(ctx) { impl in
    guard let volume else { return .invalidParam }
    volume.pointee = impl.volume
    return .ok
  }
}
@_cdecl("avspeech_set_pitch")
func avspeech_set_pitch(_ ctx: UnsafeMutableRawPointer?, _ pitch: Float) -> Int32 {
  withContext(ctx) { impl in
    impl.pitch = pitch
    return .ok
  }
}
@_cdecl("avspeech_get_pitch")
func avspeech_get_pitch(_ ctx: UnsafeMutableRawPointer?, _ pitch: UnsafeMutablePointer<Float>?)
  -> Int32
{
  withContext(ctx) { impl in
    guard let pitch else { return .invalidParam }
    pitch.pointee = impl.pitch
    return .ok
  }
}
@_cdecl("avspeech_set_rate")
func avspeech_set_rate(_ ctx: UnsafeMutableRawPointer?, _ rate: Float) -> Int32 {
  withContext(ctx) { impl in
    impl.rate = rate
    return .ok
  }
}
@_cdecl("avspeech_get_rate")
func avspeech_get_rate(_ ctx: UnsafeMutableRawPointer?, _ rate: UnsafeMutablePointer<Float>?)
  -> Int32
{
  withContext(ctx) { impl in
    guard let rate else { return .invalidParam }
    rate.pointee = impl.rate
    return .ok
  }
}
@_cdecl("avspeech_get_rate_min")
func avspeech_get_rate_min() -> Float {
  AVSpeechUtteranceMinimumSpeechRate
}
@_cdecl("avspeech_get_rate_max")
func avspeech_get_rate_max() -> Float {
  AVSpeechUtteranceMaximumSpeechRate
}
@_cdecl("avspeech_get_rate_default")
func avspeech_get_rate_default() -> Float {
  AVSpeechUtteranceDefaultSpeechRate
}
@_cdecl("avspeech_refresh_voices")
func avspeech_refresh_voices(_ ctx: UnsafeMutableRawPointer?) -> Int32 {
  withContext(ctx) { impl in
    impl.refreshVoices()
    return .ok
  }
}
@_cdecl("avspeech_count_voices")
func avspeech_count_voices(_ ctx: UnsafeMutableRawPointer?, _ count: UnsafeMutablePointer<Int32>?)
  -> Int32
{
  withContext(ctx) { impl in
    guard let count else { return .invalidParam }
    count.pointee = Int32(impl.voices.count)
    return .ok
  }
}
@_cdecl("avspeech_get_voice_name")
func avspeech_get_voice_name(
  _ ctx: UnsafeMutableRawPointer?, _ voiceId: Int32,
  _ name: UnsafeMutablePointer<UnsafePointer<CChar>?>?
) -> Int32 {
  withContext(ctx) { impl in
    guard let name else { return .invalidParam }
    guard impl.voices.indices.contains(Int(voiceId)) else { return .voiceNotFound }
    name.pointee = impl.cachedCString(impl.voices[Int(voiceId)].name)
    return .ok
  }
}
@_cdecl("avspeech_get_voice_language")
func avspeech_get_voice_language(
  _ ctx: UnsafeMutableRawPointer?, _ voiceId: Int32,
  _ language: UnsafeMutablePointer<UnsafePointer<CChar>?>?
) -> Int32 {
  withContext(ctx) { impl in
    guard let language else { return .invalidParam }
    guard impl.voices.indices.contains(Int(voiceId)) else { return .voiceNotFound }
    language.pointee = impl.cachedCString(impl.voices[Int(voiceId)].language)
    return .ok
  }
}
@_cdecl("avspeech_set_voice")
func avspeech_set_voice(_ ctx: UnsafeMutableRawPointer?, _ voiceId: Int32) -> Int32 {
  withContext(ctx) { impl in
    guard impl.voices.indices.contains(Int(voiceId)) else { return .voiceNotFound }
    impl.currentVoiceIndex = Int(voiceId)
    return .ok
  }
}
@_cdecl("avspeech_get_voice")
func avspeech_get_voice(_ ctx: UnsafeMutableRawPointer?, _ voiceId: UnsafeMutablePointer<Int32>?)
  -> Int32
{
  withContext(ctx) { impl in
    guard let voiceId else { return .invalidParam }
    if let index = impl.currentVoiceIndex {
      voiceId.pointee = Int32(index)
    } else {
      let lang = AVSpeechSynthesisVoice.currentLanguageCode()
      let defaultIndex = impl.voices.firstIndex { $0.language.hasPrefix(lang) } ?? 0
      voiceId.pointee = Int32(defaultIndex)
    }
    return .ok
  }
}
@_cdecl("avspeech_get_channels")
func avspeech_get_channels(
  _ ctx: UnsafeMutableRawPointer?, _ channels: UnsafeMutablePointer<Int32>?
) -> Int32 {
  withContext(ctx) { impl in
    guard let channels else { return .invalidParam }
    channels.pointee = impl.outputChannels
    return .ok
  }
}
@_cdecl("avspeech_get_sample_rate")
func avspeech_get_sample_rate(
  _ ctx: UnsafeMutableRawPointer?, _ sampleRate: UnsafeMutablePointer<Int32>?
) -> Int32 {
  withContext(ctx) { impl in
    guard let sampleRate else { return .invalidParam }
    sampleRate.pointee = impl.outputSampleRate
    return .ok
  }
}
@_cdecl("avspeech_get_bit_depth")
func avspeech_get_bit_depth(
  _ ctx: UnsafeMutableRawPointer?, _ bitDepth: UnsafeMutablePointer<Int32>?
) -> Int32 {
  withContext(ctx) { impl in
    guard let bitDepth else { return .invalidParam }
    bitDepth.pointee = impl.outputBitDepth
    return .ok
  }
}
