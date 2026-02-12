import Foundation

enum PatternType: String, Codable {
    case yesNo = "yes_no"
    case numberedMenu = "numbered_menu"
    case acceptReject = "accept_reject"
}

struct PatternAction: Codable, Equatable {
    let label: String
    let keys: String
}

struct PatternActionTemplate: Codable, Equatable {
    let keys: String
}

struct PatternDefinition: Codable, Identifiable {
    let id: String
    let type: PatternType
    let regex: String
    let multiLine: Bool?
    let actions: [PatternAction]?
    let actionTemplate: PatternActionTemplate?

    enum CodingKeys: String, CodingKey {
        case id, type, regex, actions
        case multiLine = "multi_line"
        case actionTemplate = "action_template"
    }
}

struct AgentConfig: Codable {
    let name: String
    let patterns: [PatternDefinition]
}

struct PatternConfig: Codable {
    let version: Int
    let agents: [String: AgentConfig]
}
