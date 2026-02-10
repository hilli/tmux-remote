import SwiftUI
import NabtoEdgeClient

struct DeviceListView: View {
    let nabtoService: NabtoService
    let connectionManager: ConnectionManager
    let bookmarkStore: BookmarkStore
    @State private var showPairing = false
    @State private var deviceStatuses: [String: DeviceStatus] = [:]
    @State private var selectedDevice: DeviceBookmark?

    enum DeviceStatus {
        case unknown
        case probing
        case online([SessionInfo])
        case offline
    }

    var body: some View {
        Group {
            if bookmarkStore.devices.isEmpty {
                WelcomeView(showPairing: $showPairing)
            } else {
                deviceList
            }
        }
        .navigationTitle("Devices")
        .toolbar {
            ToolbarItem(placement: .navigationBarTrailing) {
                Button {
                    showPairing = true
                } label: {
                    Image(systemName: "plus")
                }
            }
        }
        .sheet(isPresented: $showPairing) {
            PairingView(nabtoService: nabtoService, bookmarkStore: bookmarkStore)
        }
        .navigationDestination(item: $selectedDevice) { device in
            SessionListView(
                bookmark: device,
                nabtoService: nabtoService,
                connectionManager: connectionManager,
                bookmarkStore: bookmarkStore,
                onDismissToDevices: { selectedDevice = nil }
            )
        }
        .task { await probeAllDevices() }
    }

    @ViewBuilder
    private var deviceList: some View {
        List {
            ForEach(bookmarkStore.devices) { device in
                Button {
                    selectedDevice = device
                } label: {
                    deviceRow(device)
                }
                .accessibilityIdentifier("device-row-\(device.deviceId)")
                .tint(.primary)
            }
            .onDelete(perform: deleteDevices)
        }
        .refreshable { await probeAllDevices() }
    }

    @ViewBuilder
    private func deviceRow(_ device: DeviceBookmark) -> some View {
        HStack {
            Circle()
                .fill(statusColor(for: device.deviceId))
                .frame(width: 10, height: 10)

            VStack(alignment: .leading, spacing: 2) {
                Text(device.name)
                    .font(.body)
                    .fontWeight(.medium)

                if case .online(let sessions) = deviceStatuses[device.deviceId] {
                    let names = sessions.map(\.name).joined(separator: ", ")
                    Text(names.isEmpty ? "No sessions" : names)
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .accessibilityIdentifier("device-status-\(device.deviceId)")
                } else if case .offline = deviceStatuses[device.deviceId] {
                    Text("Offline")
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .accessibilityIdentifier("device-status-\(device.deviceId)")
                } else {
                    Text("Checking...")
                        .font(.caption)
                        .foregroundColor(.secondary)
                        .accessibilityIdentifier("device-status-\(device.deviceId)")
                }
            }

            Spacer()

            if let lastConnected = device.lastConnected {
                Text(lastConnected, style: .relative)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
        .padding(.vertical, 4)
    }

    private func statusColor(for deviceId: String) -> Color {
        switch deviceStatuses[deviceId] {
        case .online: return .green
        case .offline: return .red
        case .probing, .unknown, .none: return .gray
        }
    }

    private func deleteDevices(at offsets: IndexSet) {
        for index in offsets {
            let deviceId = bookmarkStore.devices[index].deviceId
            connectionManager.disconnect(deviceId: deviceId)
            bookmarkStore.removeDevice(id: deviceId)
        }
    }

    private func probeAllDevices() async {
        for device in bookmarkStore.devices {
            if let lastSession = device.lastSession, !lastSession.isEmpty {
                // Optimistic UI on resume/back: show the last known session immediately
                // while the live probe is in flight.
                deviceStatuses[device.deviceId] = .online([
                    SessionInfo(name: lastSession, cols: 0, rows: 0, attached: 1)
                ])
            } else {
                deviceStatuses[device.deviceId] = .probing
            }
        }

        for device in bookmarkStore.devices {
            do {
                let sessions = try await connectionManager.probeSessions(for: device)
                deviceStatuses[device.deviceId] = .online(sessions)
            } catch {
                deviceStatuses[device.deviceId] = .offline
            }
        }
    }
}

extension DeviceBookmark: Hashable {
    static func == (lhs: DeviceBookmark, rhs: DeviceBookmark) -> Bool {
        lhs.deviceId == rhs.deviceId
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(deviceId)
    }
}
