import Foundation
import SwiftCBOR

enum CBORHelpers {

    // MARK: - Encoding

    static func encodeAttach(session: String, cols: Int, rows: Int) -> Data {
        let cbor: CBOR = .map([
            .utf8String("session"): .utf8String(session),
            .utf8String("cols"): .unsignedInt(UInt64(cols)),
            .utf8String("rows"): .unsignedInt(UInt64(rows))
        ])
        return Data(cbor.encode())
    }

    static func encodeCreate(session: String, cols: Int, rows: Int, command: String? = nil) -> Data {
        var map: [CBOR: CBOR] = [
            .utf8String("session"): .utf8String(session),
            .utf8String("cols"): .unsignedInt(UInt64(cols)),
            .utf8String("rows"): .unsignedInt(UInt64(rows))
        ]
        if let command = command {
            map[.utf8String("command")] = .utf8String(command)
        }
        let cbor: CBOR = .map(map)
        return Data(cbor.encode())
    }

    static func encodeResize(cols: Int, rows: Int) -> Data {
        let cbor: CBOR = .map([
            .utf8String("cols"): .unsignedInt(UInt64(cols)),
            .utf8String("rows"): .unsignedInt(UInt64(rows))
        ])
        return Data(cbor.encode())
    }

    // MARK: - Decoding

    /// Decode a control stream message: CBOR map with "sessions" key.
    /// Format: {"sessions": [{name, cols, rows, attached}, ...]}
    static func decodeControlMessage(from data: Data) -> [SessionInfo] {
        guard let decoded = try? CBOR.decode([UInt8](data)) else { return [] }
        guard case .map(let outerMap) = decoded else { return [] }
        guard case .array(let items) = outerMap[.utf8String("sessions")] else { return [] }
        return decodeSessionArray(items)
    }

    static func decodeSessions(from data: Data) -> [SessionInfo] {
        guard let decoded = try? CBOR.decode([UInt8](data)) else { return [] }
        guard case .array(let items) = decoded else { return [] }
        return decodeSessionArray(items)
    }

    private static func decodeSessionArray(_ items: [CBOR]) -> [SessionInfo] {
        var sessions: [SessionInfo] = []
        for item in items {
            guard case .map(let map) = item else { continue }

            let name: String
            if case .utf8String(let s) = map[.utf8String("name")] {
                name = s
            } else {
                continue
            }

            let cols: Int
            if case .unsignedInt(let v) = map[.utf8String("cols")] {
                cols = Int(v)
            } else {
                cols = 0
            }

            let rows: Int
            if case .unsignedInt(let v) = map[.utf8String("rows")] {
                rows = Int(v)
            } else {
                rows = 0
            }

            let attached: Int
            if case .unsignedInt(let v) = map[.utf8String("attached")] {
                attached = Int(v)
            } else {
                attached = 0
            }

            sessions.append(SessionInfo(name: name, cols: cols, rows: rows, attached: attached))
        }
        return sessions
    }
}
