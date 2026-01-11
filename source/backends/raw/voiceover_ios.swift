// SPDX-License-Identifier: MPL-2.0
#if os(iOS) || os(tvOS) || os(watchOS) || os(visionOS)
  import UIKit

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

  private struct VOState: Sendable {
    var queue: [String] = []
    var speaking: Bool = false
    var initialized: Bool = false
  }

  private let stateQueue = DispatchQueue(label: "voiceover.ios", qos: .userInteractive)
  private nonisolated(unsafe) var state = VOState()
  private nonisolated(unsafe) var observer: (any NSObjectProtocol)? = nil

  private func processQueue() {
    var textToSpeak: String? = nil
    stateQueue.sync {
      guard state.initialized, !state.speaking, !state.queue.isEmpty else { return }
      textToSpeak = state.queue.removeFirst()
      state.speaking = true
    }
    guard let text = textToSpeak else { return }

    let post = { UIAccessibility.post(notification: .announcement, argument: text) }
    if Thread.isMainThread { post() } else { DispatchQueue.main.async(execute: post) }
  }

  @_cdecl("voiceover_ios_initialize")
  public func voiceover_ios_initialize() -> UInt8 {
    stateQueue.sync {
      if state.initialized { return VOError.alreadyInitialized.rawValue }

      var voRunning = false
      if Thread.isMainThread {
        voRunning = UIAccessibility.isVoiceOverRunning
      } else {
        DispatchQueue.main.sync { voRunning = UIAccessibility.isVoiceOverRunning }
      }
      guard voRunning else { return VOError.backendNotAvailable.rawValue }

      observer = NotificationCenter.default.addObserver(
        forName: UIAccessibility.announcementDidFinishNotification,
        object: nil,
        queue: nil
      ) { _ in
        stateQueue.async {
          state.speaking = false
          processQueue()
        }
      }

      state.initialized = true

      DispatchQueue.main.async {
        UIAccessibility.post(notification: .screenChanged, argument: nil)
      }
      return VOError.ok.rawValue
    }
  }

  @_cdecl("voiceover_ios_speak")
  public func voiceover_ios_speak(_ text: UnsafePointer<CChar>, _ interrupt: Bool) -> UInt8 {
    let result: UInt8 = stateQueue.sync {
      guard state.initialized else { return VOError.notInitialized.rawValue }
      var voRunning = false
      if Thread.isMainThread {
        voRunning = UIAccessibility.isVoiceOverRunning
      } else {
        DispatchQueue.main.sync { voRunning = UIAccessibility.isVoiceOverRunning }
      }
      guard voRunning else { return VOError.backendNotAvailable.rawValue }
      if interrupt {
        state.queue.removeAll()
        state.speaking = false
      }
      state.queue.append(String(cString: text))
      return VOError.ok.rawValue
    }
    if result != VOError.ok.rawValue { return result }
    processQueue()
    return VOError.ok.rawValue
  }

  @_cdecl("voiceover_ios_stop")
  public func voiceover_ios_stop() -> UInt8 {
    stateQueue.sync {
      guard state.initialized else { return VOError.notInitialized.rawValue }
      state.queue.removeAll()
      state.speaking = false
      return VOError.ok.rawValue
    }
  }

  @_cdecl("voiceover_ios_is_speaking")
  public func voiceover_ios_is_speaking(_ out: UnsafeMutablePointer<Bool>) -> UInt8 {
    stateQueue.sync {
      guard state.initialized else { return VOError.notInitialized.rawValue }
      out.pointee = state.speaking || !state.queue.isEmpty
      return VOError.ok.rawValue
    }
  }

  @_cdecl("voiceover_ios_shutdown")
  public func voiceover_ios_shutdown() {
    stateQueue.sync {
      if let obs = observer {
        NotificationCenter.default.removeObserver(obs)
        observer = nil
      }
      state.queue.removeAll()
      state.speaking = false
      state.initialized = false
    }
  }
#endif
