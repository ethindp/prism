// SPDX-License-Identifier: MPL-2.0

#if canImport(UIKit) && (os(iOS) || os(tvOS) || os(visionOS) || os(watchOS))
  import UIKit
  import Foundation

  private enum VOError: UInt8 {
    case ok = 0
    case notInitialized = 1
    case invalidParam = 2
    case notImplemented = 3
    case noVoices = 4
    case voiceNotFound = 5
    case speakFailure = 6
    case memoryFailure = 7
    case rangeOutOfBounds = 8
    case internalBackendError = 9
    case notSpeaking = 10
    case notPaused = 11
    case alreadyPaused = 12
    case invalidUtf8 = 13
    case invalidOperation = 14
    case alreadyInitialized = 15
    case backendNotAvailable = 16
    case unknown = 17
  }

  @inline(__always)
  private func onMainActorSync<T>(_ body: @MainActor () -> T) -> T {
    if Thread.isMainThread {
      return MainActor.assumeIsolated { body() }
    } else {
      return DispatchQueue.main.sync {
        MainActor.assumeIsolated { body() }
      }
    }
  }

  @MainActor
  private final class VoiceOverIOSController {
    private var initialized: Bool = false
    private var speaking: Bool = false
    private var queue: [String] = []
    private var observer: NSObjectProtocol? = nil

    func initialize() -> VOError {
      if initialized { return .alreadyInitialized }
      guard UIAccessibility.isVoiceOverRunning else { return .backendNotAvailable }
      observer = NotificationCenter.default.addObserver(
        forName: UIAccessibility.announcementDidFinishNotification,
        object: nil,
        queue: .main
      ) { [weak self] _ in
        guard let self else { return }
        MainActor.assumeIsolated {
          self.handleAnnouncementFinished()
        }
      }
      initialized = true
      speaking = false
      queue.removeAll(keepingCapacity: false)
      DispatchQueue.main.async {
        UIAccessibility.post(notification: .screenChanged, argument: nil)
      }
      return .ok
    }

    func shutdown() {
      if let obs = observer {
        NotificationCenter.default.removeObserver(obs)
        observer = nil
      }
      queue.removeAll(keepingCapacity: false)
      speaking = false
      initialized = false
    }

    func speak(cString: UnsafePointer<CChar>?, interrupt: Bool) -> VOError {
      guard initialized else { return .notInitialized }
      guard UIAccessibility.isVoiceOverRunning else { return .backendNotAvailable }
      guard let cString else { return .invalidParam }
      guard let text = String(validatingCString: cString) else { return .invalidUtf8 }
      if interrupt {
        queue.removeAll(keepingCapacity: true)
        speaking = false
      }
      queue.append(text)
      pumpIfNeeded()
      return .ok
    }

    func stop() -> VOError {
      guard initialized else { return .notInitialized }
      queue.removeAll(keepingCapacity: true)
      speaking = false
      return .ok
    }

    func isSpeaking(out: UnsafeMutablePointer<Bool>?) -> VOError {
      guard initialized else { return .notInitialized }
      guard let out else { return .invalidParam }
      out.pointee = speaking || !queue.isEmpty
      return .ok
    }

    private func pumpIfNeeded() {
      guard initialized else { return }
      guard !speaking else { return }
      guard !queue.isEmpty else { return }
      let next = queue.removeFirst()
      speaking = true
      DispatchQueue.main.async {
        UIAccessibility.post(notification: .announcement, argument: next)
      }
    }

    private func handleAnnouncementFinished() {
      guard initialized else { return }
      speaking = false
      pumpIfNeeded()
    }
  }

  @MainActor
  private var _voiceOverIOS: VoiceOverIOSController? = nil

  @MainActor
  private func vo() -> VoiceOverIOSController {
    if let existing = _voiceOverIOS { return existing }
    let created = VoiceOverIOSController()
    _voiceOverIOS = created
    return created
  }

  @_cdecl("voiceover_ios_initialize")
  public func voiceover_ios_initialize() -> UInt8 {
    onMainActorSync { vo().initialize().rawValue }
  }

  @_cdecl("voiceover_ios_speak")
  public func voiceover_ios_speak(_ text: UnsafePointer<CChar>?, _ interrupt: Bool) -> UInt8 {
    onMainActorSync { vo().speak(cString: text, interrupt: interrupt).rawValue }
  }

  @_cdecl("voiceover_ios_stop")
  public func voiceover_ios_stop() -> UInt8 {
    onMainActorSync { vo().stop().rawValue }
  }

  @_cdecl("voiceover_ios_is_speaking")
  public func voiceover_ios_is_speaking(_ out: UnsafeMutablePointer<Bool>?) -> UInt8 {
    onMainActorSync { vo().isSpeaking(out: out).rawValue }
  }

  @_cdecl("voiceover_ios_shutdown")
  public func voiceover_ios_shutdown() {
    onMainActorSync { vo().shutdown() }
  }
#endif
