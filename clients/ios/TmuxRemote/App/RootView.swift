import SwiftUI
import UIKit

struct RootView: View {
    let appState: AppState
    @State private var destination: LaunchDestination
    @Environment(\.scenePhase) private var scenePhase

    init(appState: AppState) {
        self.appState = appState
        let dest = resolveLaunchDestination(
            devices: appState.bookmarkStore.devices,
            lastDeviceId: appState.bookmarkStore.lastDeviceId
        )
        self._destination = State(initialValue: dest)
        switch dest {
        case .resumeSession(let bookmark, let session):
            AppLog.log("RootView: launch destination = resumeSession(device=%@, session=%@)", bookmark.deviceId, session)
        case .deviceList:
            AppLog.log("RootView: launch destination = deviceList (lastDeviceId=%@, deviceCount=%d)",
                       appState.bookmarkStore.lastDeviceId ?? "nil",
                       appState.bookmarkStore.devices.count)
        }
    }

    var body: some View {
        NavigationStack {
            switch destination {
            case .resumeSession(let bookmark, let session):
                TerminalScreen(
                    bookmark: bookmark,
                    sessionName: session,
                    nabtoService: appState.nabtoService,
                    connectionManager: appState.connectionManager,
                    bookmarkStore: appState.bookmarkStore,
                    onDismiss: { destination = .deviceList }
                )
            case .deviceList:
                DeviceListView(
                    nabtoService: appState.nabtoService,
                    connectionManager: appState.connectionManager,
                    bookmarkStore: appState.bookmarkStore
                )
            }
        }
        .task {
            appState.connectionManager.warmCache(bookmarks: appState.bookmarkStore.devices)
        }
        .onChange(of: scenePhase) { _, newPhase in
            switch newPhase {
            case .background:
                let taskId = UIApplication.shared.beginBackgroundTask(expirationHandler: nil)
                appState.connectionManager.disconnectAll()
                if taskId != .invalid {
                    UIApplication.shared.endBackgroundTask(taskId)
                }
            case .active:
                appState.connectionManager.warmCache(bookmarks: appState.bookmarkStore.devices)
            default:
                break
            }
        }
    }
}
