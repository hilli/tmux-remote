import Foundation

@Observable
class PatternEngine {
    private(set) var activeMatch: PatternMatch?
    private(set) var isOverlayHidden = false
    private let minimumVisibleDuration: TimeInterval
    private let resolveSuppressionDuration: TimeInterval
    private let now: () -> Date
    private var activeSince: Date?
    private var resolvedPromptSignature: String?
    private var resolvedAt: Date?

    var visibleMatch: PatternMatch? {
        if isOverlayHidden {
            return nil
        }
        return activeMatch
    }

    var canRestoreHiddenMatch: Bool {
        return isOverlayHidden && activeMatch != nil
    }

    init(minimumVisibleDuration: TimeInterval = 1.0,
         resolveSuppressionDuration: TimeInterval = 1.5,
         now: @escaping () -> Date = Date.init)
    {
        self.minimumVisibleDuration = minimumVisibleDuration
        self.resolveSuppressionDuration = resolveSuppressionDuration
        self.now = now
    }

    func applyServerPresent(_ match: PatternMatch) {
        if shouldSuppress(match) {
            return
        }
        clearResolveSuppression()
        activeMatch = match
        isOverlayHidden = false
        activeSince = now()
    }

    func applyServerUpdate(_ match: PatternMatch) {
        guard activeMatch?.id == match.id else {
            return
        }
        activeMatch = match
        if activeSince == nil {
            activeSince = now()
        }
    }

    func applyServerGone(instanceId: String) {
        guard activeMatch?.id == instanceId else {
            return
        }

        if let since = activeSince,
           now().timeIntervalSince(since) < minimumVisibleDuration {
            return
        }

        clearActiveState()
    }

    func dismissLocally(instanceId: String) {
        guard activeMatch?.id == instanceId else {
            return
        }
        isOverlayHidden = true
    }

    func restoreOverlay() {
        guard activeMatch != nil else {
            return
        }
        isOverlayHidden = false
    }

    func resolveLocally(instanceId: String) {
        guard let match = activeMatch, match.id == instanceId else {
            return
        }
        resolvedPromptSignature = promptSignature(match)
        resolvedAt = now()
        clearActiveState()
    }

    func reset() {
        clearResolveSuppression()
        clearActiveState()
    }

    private func clearActiveState() {
        activeMatch = nil
        isOverlayHidden = false
        activeSince = nil
    }

    private func clearResolveSuppression() {
        resolvedPromptSignature = nil
        resolvedAt = nil
    }

    private func shouldSuppress(_ match: PatternMatch) -> Bool {
        guard let signature = resolvedPromptSignature, let resolvedAt else {
            return false
        }
        if now().timeIntervalSince(resolvedAt) > resolveSuppressionDuration {
            clearResolveSuppression()
            return false
        }
        return signature == promptSignature(match)
    }

    private func promptSignature(_ match: PatternMatch) -> String {
        let prompt = (match.prompt ?? "")
            .trimmingCharacters(in: .whitespacesAndNewlines)
            .lowercased()
        return "\(match.patternType.rawValue)|\(prompt)"
    }
}
