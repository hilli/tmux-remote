import Foundation

@Observable
class AppState {
    let bookmarkStore: BookmarkStore
    let connectionManager: ConnectionManager
    let nabtoService: NabtoService

    init() {
        let store = BookmarkStore()
        let cm = ConnectionManager()
        self.bookmarkStore = store
        self.connectionManager = cm
        self.nabtoService = NabtoService(connectionManager: cm, bookmarkStore: store)
        injectTestConfigIfNeeded()
        if ProcessInfo.processInfo.environment["UI_TESTING"] == nil {
            cm.warmCache(bookmarks: store.devices)
        }
    }

    private func injectTestConfigIfNeeded() {
        let args = ProcessInfo.processInfo.arguments
        guard let idx = args.firstIndex(of: "--test-config"),
              idx + 1 < args.count else { return }
        let json = args[idx + 1]
        guard let data = json.data(using: .utf8) else { return }

        struct TestConfig: Codable {
            var productId: String
            var deviceId: String
            var sct: String
            var fingerprint: String
            var privateKey: String?
        }

        guard let config = try? JSONDecoder().decode(TestConfig.self, from: data) else { return }

        // Always inject the test private key (overrides any existing key)
        if let key = config.privateKey, !key.isEmpty {
            _ = KeychainService.savePrivateKey(key)
        }

        // Clear previous state so tests start from a clean device list
        for device in bookmarkStore.devices {
            bookmarkStore.removeDevice(id: device.deviceId)
        }

        let bookmark = DeviceBookmark(
            productId: config.productId,
            deviceId: config.deviceId,
            fingerprint: config.fingerprint,
            sct: config.sct,
            name: config.deviceId,
            lastSession: nil,
            lastConnected: nil
        )
        bookmarkStore.addDevice(bookmark)
    }
}
