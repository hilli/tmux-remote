import XCTest
@testable import NabtoShell

final class PatternConfigTests: XCTestCase {

    func testDecodeBundledFormat() {
        let json = """
        {
          "version": 2,
          "agents": {
            "test-agent": {
              "name": "Test Agent",
              "patterns": [
                {
                  "id": "yes_prompt",
                  "type": "yes_no",
                  "regex": "Continue\\\\?.*\\\\(y\\\\/n\\\\)",
                  "actions": [
                    { "label": "Yes", "keys": "y" },
                    { "label": "No", "keys": "n" }
                  ]
                }
              ]
            }
          }
        }
        """.data(using: .utf8)!

        let config = PatternConfigLoader.load(from: json)
        XCTAssertNotNil(config)
        XCTAssertEqual(config?.version, 2)
        XCTAssertEqual(config?.agents.count, 1)

        let agent = config?.agents["test-agent"]
        XCTAssertEqual(agent?.name, "Test Agent")
        XCTAssertEqual(agent?.patterns.count, 1)

        let pattern = agent?.patterns[0]
        XCTAssertEqual(pattern?.id, "yes_prompt")
        XCTAssertEqual(pattern?.type, .yesNo)
        XCTAssertEqual(pattern?.actions?.count, 2)
    }

    func testDecodeNumberedMenu() {
        let json = """
        {
          "version": 1,
          "agents": {
            "a": {
              "name": "A",
              "patterns": [
                {
                  "id": "menu",
                  "type": "numbered_menu",
                  "regex": "\\\\[\\\\d+\\\\]\\\\s+.+",
                  "multi_line": true,
                  "action_template": { "keys": "{number}\\n" }
                }
              ]
            }
          }
        }
        """.data(using: .utf8)!

        let config = PatternConfigLoader.load(from: json)
        XCTAssertNotNil(config)

        let pattern = config?.agents["a"]?.patterns[0]
        XCTAssertEqual(pattern?.type, .numberedMenu)
        XCTAssertEqual(pattern?.multiLine, true)
        XCTAssertEqual(pattern?.actionTemplate?.keys, "{number}\n")
        XCTAssertNil(pattern?.actions)
    }

    func testDecodeEmptyPatterns() {
        let json = """
        {
          "version": 1,
          "agents": {
            "empty": {
              "name": "Empty",
              "patterns": []
            }
          }
        }
        """.data(using: .utf8)!

        let config = PatternConfigLoader.load(from: json)
        XCTAssertNotNil(config)
        XCTAssertEqual(config?.agents["empty"]?.patterns.count, 0)
    }

    func testInvalidJSON() {
        let json = "not json".data(using: .utf8)!
        let config = PatternConfigLoader.load(from: json)
        XCTAssertNil(config)
    }

    func testMissingRequiredFields() {
        // Missing "version"
        let json = """
        {
          "agents": {}
        }
        """.data(using: .utf8)!
        let config = PatternConfigLoader.load(from: json)
        XCTAssertNil(config)
    }

    func testOptionalMultiLine() {
        let json = """
        {
          "version": 1,
          "agents": {
            "a": {
              "name": "A",
              "patterns": [
                {
                  "id": "p",
                  "type": "yes_no",
                  "regex": "test",
                  "actions": [
                    { "label": "Y", "keys": "y" },
                    { "label": "N", "keys": "n" }
                  ]
                }
              ]
            }
          }
        }
        """.data(using: .utf8)!

        let config = PatternConfigLoader.load(from: json)
        let pattern = config?.agents["a"]?.patterns[0]
        XCTAssertNil(pattern?.multiLine)
    }
}
