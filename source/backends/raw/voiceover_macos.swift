// SPDX-License-Identifier: MPL-2.0
#if os(macOS)
import AppKit

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
    var pendingText: String = ""
    var windowPtr: Unmanaged<NSWindow>? = nil
    var initialized: Bool = false
}

private let stateQueue = DispatchQueue(label: "voiceover.macos", qos: .userInteractive)
private let debounceQueue = DispatchQueue(label: "voiceover.macos.debounce", qos: .userInteractive)
private nonisolated(unsafe) var state = VOState()
private nonisolated(unsafe) var debounceWork: DispatchWorkItem? = nil

@_cdecl("voiceover_macos_initialize")
public func voiceover_macos_initialize() -> UInt8 {
    stateQueue.sync {
        if state.initialized { return VOError.alreadyInitialized.rawValue }
        
        var window: NSWindow? = nil
        let work = { () -> Void in
            guard NSWorkspace.shared.isVoiceOverEnabled else { return }
            let app = NSApplication.shared
            if let w = app.keyWindow, w.isVisible { window = w; return }
            if let w = app.mainWindow, w.isVisible { window = w; return }
            for w in app.orderedWindows where w.isVisible && !w.isMiniaturized && w.level == .normal {
                window = w; return
            }
            for w in app.windows where w.contentView != nil {
                window = w; return
            }
            window = app.windows.first
        }
        
        if Thread.isMainThread { work() }
        else { DispatchQueue.main.sync(execute: work) }
        
        guard let w = window else { return VOError.backendNotAvailable.rawValue }
        
        state.windowPtr = Unmanaged.passRetained(w)
        state.initialized = true
        
        DispatchQueue.main.async {
            NSAccessibility.post(element: w, notification: .windowCreated)
            NSAccessibility.post(element: w, notification: .focusedWindowChanged)
        }
        return VOError.ok.rawValue
    }
}

@_cdecl("voiceover_macos_speak")
public func voiceover_macos_speak(_ text: UnsafePointer<CChar>, _ interrupt: Bool) -> UInt8 {
    stateQueue.sync {
        guard state.initialized, let windowRef = state.windowPtr else { return VOError.notInitialized.rawValue }
        
        var voRunning = false
        if Thread.isMainThread { voRunning = NSWorkspace.shared.isVoiceOverEnabled }
        else { DispatchQueue.main.sync { voRunning = NSWorkspace.shared.isVoiceOverEnabled } }
        guard voRunning else { return VOError.backendNotAvailable.rawValue }
        
        debounceWork?.cancel()
        
        let str = String(cString: text)
        if interrupt || state.pendingText.isEmpty {
            state.pendingText = str
        } else {
            state.pendingText += " . " + str
        }
        
        let textToSpeak = state.pendingText
        let work = DispatchWorkItem { [windowRef] in
            stateQueue.sync {
                guard state.initialized else { return }
                state.pendingText = ""
            }
            let window = windowRef.takeUnretainedValue()
            let announce = {
                NSAccessibility.post(
                    element: window,
                    notification: .announcementRequested,
                    userInfo: [
                        .announcement: textToSpeak,
                        .priority: NSAccessibilityPriorityLevel.high.rawValue
                    ]
                )
            }
            if Thread.isMainThread { announce() }
            else { DispatchQueue.main.sync(execute: announce) }
        }
        debounceWork = work
        debounceQueue.asyncAfter(deadline: .now() + 0.015, execute: work)
        return VOError.ok.rawValue
    }
}

@_cdecl("voiceover_macos_stop")
public func voiceover_macos_stop() -> UInt8 {
    stateQueue.sync {
        guard state.initialized else { return VOError.notInitialized.rawValue }
        debounceWork?.cancel()
        debounceWork = nil
        state.pendingText = ""
        return VOError.ok.rawValue
    }
}

@_cdecl("voiceover_macos_is_speaking")
public func voiceover_macos_is_speaking(_ out: UnsafeMutablePointer<Bool>) -> UInt8 {
    stateQueue.sync {
        guard state.initialized else { return VOError.notInitialized.rawValue }
        out.pointee = !state.pendingText.isEmpty
        return VOError.ok.rawValue
    }
}

@_cdecl("voiceover_macos_shutdown")
public func voiceover_macos_shutdown() {
    stateQueue.sync {
        debounceWork?.cancel()
        debounceWork = nil
        state.pendingText = ""
        if let ptr = state.windowPtr {
            ptr.release()
            state.windowPtr = nil
        }
        state.initialized = false
    }
}
#endif