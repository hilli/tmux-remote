import Foundation
import NabtoEdgeClient

/// Receives connection lifecycle events from the Nabto SDK.
private class EventReceiver: NSObject, ConnectionEventReceiver {
    var onClosed: (() -> Void)?
    private let callbackQueue = DispatchQueue(label: "NabtoShell.ConnectionEventReceiver")

    func onEvent(event: NabtoEdgeClientConnectionEvent) {
        let onClosedLocal = onClosed
        callbackQueue.async {
            if event == .CLOSED {
                onClosedLocal?()
            }
        }
    }
}

@Observable
class ConnectionManager {
    /// Per-device connection state visible to UI.
    private(set) var deviceStates: [String: ConnectionState] = [:]

    private struct PendingConnect {
        let id: UUID
        let task: Task<Connection, Error>
    }

    private let client: Client
    private var connections: [String: Connection] = [:]
    private var pendingConnects: [String: PendingConnect] = [:]
    private var eventReceivers: [String: EventReceiver] = [:]

    init() {
        self.client = Client()
    }

    /// Probe terminal sessions for device-list status.
    ///
    /// Fast path: use a cached live connection (e.g. when returning from TerminalScreen).
    /// Fallback: use a short-lived isolated connection.
    func probeSessions(for bookmark: DeviceBookmark) async throws -> [SessionInfo] {
        let deviceId = bookmark.deviceId

        if let cached = connections[deviceId] {
            do {
                return try await listSessions(on: cached, timeoutNanoseconds: 2_000_000_000)
            } catch {
                // Drop stale cached connection before isolated fallback.
                disconnect(deviceId: deviceId)
            }
        }

        let probeClient = Client()
        let privateKey = try loadOrCreatePrivateKey(using: probeClient)
        let conn = try probeClient.createConnection()
        try conn.setPrivateKey(key: privateKey)
        try conn.setProductId(id: bookmark.productId)
        try conn.setDeviceId(id: bookmark.deviceId)
        try conn.setServerConnectToken(sct: bookmark.sct)

        defer {
            conn.stop()
        }

        try await connect(conn: conn, timeoutNanoseconds: 10_000_000_000)
        return try await listSessions(on: conn, timeoutNanoseconds: 30_000_000_000)
    }

    deinit {
        for (deviceId, _) in connections {
            disconnect(deviceId: deviceId)
        }
    }

    /// Returns a cached connected Connection, or creates one.
    /// Serialized per deviceId: concurrent callers for the same device
    /// await the same in-flight Task.
    func connection(for bookmark: DeviceBookmark) async throws -> Connection {
        let deviceId = bookmark.deviceId

        if let existing = connections[deviceId] {
            return existing
        }

        if let pending = pendingConnects[deviceId] {
            return try await pending.task.value
        }

        let pendingId = UUID()
        let task = Task<Connection, Error> { [weak self] in
            guard let self else {
                throw NabtoError.connectionFailed("ConnectionManager deallocated")
            }

            self.deviceStates[deviceId] = .connecting
            let privateKey = try self.loadOrCreatePrivateKey()
            let conn = try self.client.createConnection()
            try conn.setPrivateKey(key: privateKey)
            try conn.setProductId(id: bookmark.productId)
            try conn.setDeviceId(id: bookmark.deviceId)
            try conn.setServerConnectToken(sct: bookmark.sct)
            try await withTaskCancellationHandler {
                try await withThrowingTaskGroup(of: Void.self) { group in
                    group.addTask {
                        try await conn.connectAsync()
                    }
                    group.addTask {
                        try await Task.sleep(nanoseconds: 10_000_000_000)
                        conn.stop()
                        throw NabtoError.connectionFailed("Connection timed out")
                    }
                    // Wait for the first to complete; cancel the other
                    try await group.next()
                    group.cancelAll()
                }
            } onCancel: {
                conn.stop()
            }
            try Task.checkCancellation()

            guard self.pendingConnects[deviceId]?.id == pendingId else {
                conn.stop()
                throw CancellationError()
            }

            let receiver = EventReceiver()
            receiver.onClosed = { [weak self] in
                Task { @MainActor in
                    self?.handleConnectionClosed(deviceId: deviceId)
                }
            }
            try conn.addConnectionEventsReceiver(cb: receiver)

            guard self.pendingConnects[deviceId]?.id == pendingId else {
                conn.removeConnectionEventsReceiver(cb: receiver)
                conn.stop()
                throw CancellationError()
            }

            self.connections[deviceId] = conn
            self.eventReceivers[deviceId] = receiver
            self.pendingConnects.removeValue(forKey: deviceId)
            self.deviceStates[deviceId] = .connected
            return conn
        }

        pendingConnects[deviceId] = PendingConnect(id: pendingId, task: task)

        do {
            return try await task.value
        } catch {
            if pendingConnects[deviceId]?.id == pendingId {
                pendingConnects.removeValue(forKey: deviceId)
                if connections[deviceId] == nil {
                    deviceStates[deviceId] = .disconnected
                }
            }
            throw error
        }
    }

    /// Disconnect and remove a cached connection.
    func disconnect(deviceId: String) {
        if let pending = pendingConnects.removeValue(forKey: deviceId) {
            pending.task.cancel()
        }

        if let conn = connections.removeValue(forKey: deviceId) {
            if let receiver = eventReceivers.removeValue(forKey: deviceId) {
                conn.removeConnectionEventsReceiver(cb: receiver)
            }
            conn.stop()
        }
        deviceStates[deviceId] = .disconnected
    }

    /// Cancel only the in-flight connect task for a device.
    /// Used when a view is dismissed while an initial resume connect is still running.
    func cancelPendingConnect(deviceId: String) {
        guard let pending = pendingConnects.removeValue(forKey: deviceId) else { return }
        pending.task.cancel()
        if connections[deviceId] == nil {
            deviceStates[deviceId] = .disconnected
        }
    }

    /// Update the connection state for a device (used by NabtoService for reconnect state).
    func setDeviceState(_ state: ConnectionState, for deviceId: String) {
        deviceStates[deviceId] = state
    }

    /// Warm the connection cache on app start.
    func warmCache(bookmarks: [DeviceBookmark]) {
        for bookmark in bookmarks {
            Task {
                _ = try? await connection(for: bookmark)
            }
        }
    }

    /// Create a connection for pairing. Not cached until adoptConnection() is called.
    func connectForPairing(info: PairingInfo) async throws -> Connection {
        deviceStates[info.deviceId] = .connecting
        let privateKey = try loadOrCreatePrivateKey()
        let conn = try client.createConnection()
        try conn.setPrivateKey(key: privateKey)
        try conn.setProductId(id: info.productId)
        try conn.setDeviceId(id: info.deviceId)
        try conn.setServerConnectToken(sct: info.sct)
        try await conn.connectAsync()
        deviceStates[info.deviceId] = .connected
        return conn
    }

    /// Adopt an already-open connection into the cache so it is reused for subsequent operations.
    func adoptConnection(_ conn: Connection, for deviceId: String) {
        let receiver = EventReceiver()
        receiver.onClosed = { [weak self] in
            Task { @MainActor in
                self?.handleConnectionClosed(deviceId: deviceId)
            }
        }
        try? conn.addConnectionEventsReceiver(cb: receiver)

        connections[deviceId] = conn
        eventReceivers[deviceId] = receiver
        deviceStates[deviceId] = .connected
    }

    private func handleConnectionClosed(deviceId: String) {
        connections.removeValue(forKey: deviceId)
        eventReceivers.removeValue(forKey: deviceId)
        deviceStates[deviceId] = .disconnected
    }

    private func connect(conn: Connection, timeoutNanoseconds: UInt64) async throws {
        try await withTaskCancellationHandler {
            try await withThrowingTaskGroup(of: Void.self) { group in
                group.addTask {
                    try await conn.connectAsync()
                }
                group.addTask {
                    try await Task.sleep(nanoseconds: timeoutNanoseconds)
                    conn.stop()
                    throw NabtoError.connectionFailed("Probe connection timed out")
                }
                try await group.next()
                group.cancelAll()
            }
        } onCancel: {
            conn.stop()
        }
    }

    private func listSessions(on conn: Connection, timeoutNanoseconds: UInt64) async throws -> [SessionInfo] {
        let coap = try conn.createCoapRequest(method: "GET", path: "/terminal/sessions")
        let response = try await executeCoap(coap: coap, timeoutNanoseconds: timeoutNanoseconds)

        guard response.status == 205 else {
            throw NabtoError.coapFailed("List sessions", response.status)
        }
        guard let payload = response.payload else {
            return []
        }
        return CBORHelpers.decodeSessions(from: payload)
    }

    private func executeCoap(coap: CoapRequest, timeoutNanoseconds: UInt64) async throws -> CoapResponse {
        return try await withTaskCancellationHandler {
            try await withThrowingTaskGroup(of: CoapResponse.self) { group in
                group.addTask {
                    try await coap.executeAsync()
                }
                group.addTask {
                    try await Task.sleep(nanoseconds: timeoutNanoseconds)
                    coap.stop()
                    throw NabtoError.connectionFailed("CoAP request timed out")
                }
                guard let first = try await group.next() else {
                    throw NabtoError.connectionFailed("CoAP request returned no result")
                }
                group.cancelAll()
                return first
            }
        } onCancel: {
            coap.stop()
        }
    }

    private func loadOrCreatePrivateKey(using keyClient: Client? = nil) throws -> String {
        if let existing = KeychainService.loadPrivateKey() {
            return existing
        }
        let key = try (keyClient ?? client).createPrivateKey()
        guard KeychainService.savePrivateKey(key) else {
            throw NabtoError.connectionFailed("Failed to save private key to Keychain")
        }
        return key
    }
}
