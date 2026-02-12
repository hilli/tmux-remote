#if DEBUG
import Foundation

@Observable
class StubNabtoService: NabtoService {
    private let cm: ConnectionManager
    private(set) var sentData: [Data] = []
    var scriptedEvents: [StubScript.PatternEvent] = []

    override init(connectionManager: ConnectionManager, bookmarkStore: BookmarkStore) {
        self.cm = connectionManager
        super.init(connectionManager: connectionManager, bookmarkStore: bookmarkStore)
    }

    override func connect(bookmark: DeviceBookmark) async throws {
        currentDeviceId = bookmark.deviceId
        cm.setDeviceState(.connected, for: bookmark.deviceId)
    }

    override func attach(bookmark: DeviceBookmark, session: String, cols: Int, rows: Int) async throws {
        currentSession = session
    }

    override func openStream(bookmark: DeviceBookmark) async throws {
        let deviceId = bookmark.deviceId
        let events = scriptedEvents
        AppLog.log("StubNabtoService.openStream: events=%d", events.count)
        // Deliver pattern events via the control stream callback,
        // matching the production code path.
        Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: 300_000_000)
            guard self != nil else { return }
            for event in events {
                if event.delay > 0 {
                    try? await Task.sleep(nanoseconds: UInt64(event.delay * 1_000_000_000))
                }
                let cm = self?.cm
                switch event.type {
                case "pattern_match":
                    guard let patternId = event.patternId,
                          let patternTypeStr = event.patternType else { continue }
                    let patternType: PatternType
                    switch patternTypeStr {
                    case "yes_no": patternType = .yesNo
                    case "numbered_menu": patternType = .numberedMenu
                    case "accept_reject": patternType = .acceptReject
                    default: continue
                    }
                    let actions = (event.actions ?? []).map {
                        ResolvedAction(label: $0.label, keys: $0.keys)
                    }
                    let match = PatternMatch(
                        id: patternId,
                        patternType: patternType,
                        prompt: event.prompt,
                        matchedText: "",
                        actions: actions,
                        matchPosition: 0
                    )
                    AppLog.log("StubNabtoService: delivering pattern_match id=%@", patternId)
                    cm?.onPatternEvent?(deviceId, .patternMatch(match))
                case "pattern_dismiss":
                    AppLog.log("StubNabtoService: delivering pattern_dismiss")
                    cm?.onPatternEvent?(deviceId, .patternDismiss)
                default:
                    break
                }
            }
        }
    }

    override func writeToStream(_ data: Data) {
        sentData.append(data)
    }

    override func resize(bookmark: DeviceBookmark, cols: Int, rows: Int) async {}

    override func disconnect(keepConnection: Bool = false) {
        onStreamClosed = nil
        onStreamData = nil
        currentSession = nil
        currentDeviceId = nil
    }

    override func enableReconnectContext(deviceId: String, session: String) {}
    override func disableReconnectContext() {}
    override func canAutoReconnect(deviceId: String, session: String) -> Bool { false }
}
#endif
