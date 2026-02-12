import Foundation

struct ResolvedAction: Identifiable, Equatable {
    let id = UUID()
    let label: String
    let keys: String
}

struct PatternMatch: Identifiable, Equatable {
    let id: String
    let patternType: PatternType
    let prompt: String?
    let matchedText: String
    let actions: [ResolvedAction]
    let matchPosition: Int

    static func == (lhs: PatternMatch, rhs: PatternMatch) -> Bool {
        lhs.id == rhs.id && lhs.matchPosition == rhs.matchPosition
    }
}
