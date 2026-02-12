#if DEBUG
import Foundation

/// Script that defines a sequence of pattern events for stub testing.
/// Each event is delivered to ConnectionManager.onPatternEvent, matching
/// what the agent control stream would push in production.
struct StubScript: Codable {
    struct PatternEvent: Codable {
        let type: String           // "pattern_match" or "pattern_dismiss"
        let delay: Double          // seconds before delivering this event
        let patternId: String?     // for pattern_match
        let patternType: String?   // "yes_no", "numbered_menu", "accept_reject"
        let prompt: String?        // optional prompt text
        let actions: [Action]?     // resolved actions

        struct Action: Codable {
            let label: String
            let keys: String
        }

        enum CodingKeys: String, CodingKey {
            case type, delay, prompt, actions
            case patternId = "pattern_id"
            case patternType = "pattern_type"
        }
    }

    let events: [PatternEvent]
}
#endif
