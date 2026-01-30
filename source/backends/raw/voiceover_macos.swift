// SPDX-License-Identifier: MPL-2.0

#if canImport(AppKit) && os(macOS)
  import AppKit
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
  private final class VoiceOverMacOSController {
    private var initialized: Bool = false
    private var pendingText: String = ""
    private var debounceItem: DispatchWorkItem? = nil
    private let debounceDelay: TimeInterval = 0.015
    private weak var cachedWindow: NSWindow? = nil

    func initialize() -> VOError {
      if initialized { return .alreadyInitialized }
      guard NSWorkspace.shared.isVoiceOverEnabled else { return .backendNotAvailable }
      guard let w = pickBestWindow() else { return .backendNotAvailable }
      cachedWindow = w
      initialized = true
      pendingText = ""
      cancelDebounce()
      DispatchQueue.main.async {
        NSAccessibility.post(element: w, notification: .windowCreated)
        NSAccessibility.post(element: w, notification: .focusedWindowChanged)
      }
      return .ok
    }

    func shutdown() {
      cancelDebounce()
      pendingText = ""
      cachedWindow = nil
      initialized = false
    }

    func speak(cString: UnsafePointer<CChar>?, interrupt: Bool) -> VOError {
      guard initialized else { return .notInitialized }
      guard NSWorkspace.shared.isVoiceOverEnabled else { return .backendNotAvailable }
      guard let cString else { return .invalidParam }
      guard let text = String(validatingCString: cString) else { return .invalidUtf8 }
      if cachedWindow == nil || cachedWindow?.contentView == nil {
        cachedWindow = pickBestWindow()
      }
      guard cachedWindow != nil else { return .backendNotAvailable }
      if interrupt || pendingText.isEmpty {
        pendingText = text
      } else {
        pendingText += " . " + text
      }
      scheduleDebouncedAnnouncement()
      return .ok
    }

    func stop() -> VOError {
      guard initialized else { return .notInitialized }
      cancelDebounce()
      pendingText = ""
      return .ok
    }

    func isSpeaking(out: UnsafeMutablePointer<Bool>?) -> VOError {
      guard initialized else { return .notInitialized }
      guard let out else { return .invalidParam }
      out.pointee = !pendingText.isEmpty || debounceItem != nil
      return .ok
    }

    private func pickBestWindow() -> NSWindow? {
      let app = NSApplication.shared
      if let w = app.keyWindow, w.isVisible { return w }
      if let w = app.mainWindow, w.isVisible { return w }
      for w in app.orderedWindows where w.isVisible && !w.isMiniaturized && w.level == .normal {
        return w
      }
      for w in app.windows where w.contentView != nil {
        return w
      }
      return app.windows.first
    }

    private func cancelDebounce() {
      debounceItem?.cancel()
      debounceItem = nil
    }

    private func scheduleDebouncedAnnouncement() {
      cancelDebounce()
      let item = DispatchWorkItem { [weak self] in
        guard let self else { return }
        self.fireAnnouncementNow()
      }
      debounceItem = item
      DispatchQueue.main.asyncAfter(deadline: .now() + debounceDelay, execute: item)
    }

    private func fireAnnouncementNow() {
      debounceItem = nil
      guard initialized else { return }
      guard NSWorkspace.shared.isVoiceOverEnabled else { return }
      guard !pendingText.isEmpty else { return }
      let textToSpeak = pendingText
      pendingText = ""
      guard let w = cachedWindow ?? pickBestWindow() else { return }
      cachedWindow = w
      NSAccessibility.post(
        element: w,
        notification: .announcementRequested,
        userInfo: [
          .announcement: textToSpeak,
          .priority: NSAccessibilityPriorityLevel.high.rawValue,
        ]
      )
    }
  }

  @MainActor
  private var _voiceOverMac: VoiceOverMacOSController? = nil

  @MainActor
  private func vo() -> VoiceOverMacOSController {
    if let existing = _voiceOverMac { return existing }
    let created = VoiceOverMacOSController()
    _voiceOverMac = created
    return created
  }

  @_cdecl("voiceover_macos_initialize")
  public func voiceover_macos_initialize() -> UInt8 {
    onMainActorSync { vo().initialize().rawValue }
  }

  @_cdecl("voiceover_macos_speak")
  public func voiceover_macos_speak(_ text: UnsafePointer<CChar>?, _ interrupt: Bool) -> UInt8 {
    onMainActorSync { vo().speak(cString: text, interrupt: interrupt).rawValue }
  }

  @_cdecl("voiceover_macos_stop")
  public func voiceover_macos_stop() -> UInt8 {
    onMainActorSync { vo().stop().rawValue }
  }

  @_cdecl("voiceover_macos_is_speaking")
  public func voiceover_macos_is_speaking(_ out: UnsafeMutablePointer<Bool>?) -> UInt8 {
    onMainActorSync { vo().isSpeaking(out: out).rawValue }
  }

  @_cdecl("voiceover_macos_shutdown")
  public func voiceover_macos_shutdown() {
    onMainActorSync { vo().shutdown() }
  }
#endif
