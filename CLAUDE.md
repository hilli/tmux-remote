# CLAUDE.md

Read SPEC.md before starting any work. It is the source of truth for architecture, protocol, security model, and design decisions.

## Project overview

NabtoShell is a secure remote terminal tool using Nabto Edge P2P connectivity. It exposes tmux sessions on a machine to authenticated clients without SSH, port forwarding, or firewall configuration.

The repo contains three components that share no source code:

- `agent/` -- C, Nabto Embedded SDK (device side, serves terminal sessions)
- `clients/cli/` -- C, Nabto Client SDK binary (client side, connects to agent)
- `clients/ios/` -- Swift, NabtoEdgeClientSwift (client side, mobile terminal)
- `patterns/` -- JSON pattern definitions for CLI tool overlays (client-side only)

## Build

### Agent

```bash
cd agent
cmake -B _build -G Ninja
cmake --build _build
```

The Nabto Embedded SDK is fetched automatically via CMake FetchContent. No submodule init needed.

### CLI client

```bash
cd clients/cli
cmake -B _build -G Ninja
cmake --build _build
```

The Nabto Client SDK is vendored in `clients/cli/nabto_client_sdk_library/`. No external downloads needed.

### iOS app

Open `clients/ios/NabtoShell.xcodeproj` in Xcode. NabtoEdgeClientSwift is managed via CocoaPods.

## Architecture rules

- The agent uses the **Nabto Embedded SDK** (open source, C). The clients use the **Nabto Client SDK** (binary release, C API). These are different SDKs. Do not mix them.
- The agent is a generic PTY relay. It knows nothing about what runs in the terminal. Never add agent-side logic that depends on Claude Code, Codex, Aider, or any specific CLI tool.
- Pattern detection for CLI tools is handled entirely on the client side via JSON pattern configs in `patterns/`. This is by design: see SPEC.md section 13 item 1 and 7.
- Communication: Nabto Streams for terminal data (raw bidirectional byte pipe), CoAP for control messages (resize, session management). See SPEC.md sections 4.4 and 7.

## Security model

This is critical. NabtoShell grants remote shell access. A compromise means arbitrary command execution.

- Single IAM role: Owner. Full access or no access. Do not add "limited" roles.
- Password Invite Pairing only. Pairing is closed after each invitation is consumed. No open pairing modes in normal operation. `--demo-init` is the only exception and must print a warning.
- Every CoAP endpoint and stream handler must call `nm_iam_check_access()` before processing. No exceptions.
- See SPEC.md sections 4.5 and 9 for full details.

## Code style

- Agent and CLI client: C (not C++). Match the style of the Nabto Embedded SDK examples.
- iOS app: Swift, UIKit or SwiftUI.
- Use CBOR (content format 60) for all CoAP payloads. Use tinycbor on the C side.
- No em dashes in comments, docs, or output strings. Use colons, semicolons, commas, or separate sentences.

## CoAP endpoints (agent)

All endpoints require IAM authorization except pairing.

| Method | Path | Payload | Response | Purpose |
|--------|------|---------|----------|---------|
| POST | /terminal/resize | `{cols, rows}` | 2.04 | Set PTY size, send SIGWINCH |
| GET | /terminal/sessions | -- | 2.05 `[{name, cols, rows, attached}]` | List tmux sessions |
| POST | /terminal/attach | `{session, cols, rows, create?}` | 2.01 | Set target session for next stream |
| POST | /terminal/create | `{session, cols?, rows?, command?}` | 2.01 | Create new tmux session |
| GET | /terminal/status | -- | 2.05 `{version, active_sessions, uptime}` | Agent health |

## Stream protocol

Port 1. Raw bytes, no framing. Transparent PTY pipe. Terminal escape sequences are handled by the client (local terminal for CLI, SwiftTerm for iOS).

## File layout

```
agent/
  CMakeLists.txt
  src/

clients/
  cli/
    CMakeLists.txt
    src/
    nabto_client_sdk_library/    # Vendored: headers + platform binaries

  ios/
    NabtoShell.xcodeproj/
    NabtoShell/

patterns/
  schema.json                    # JSON schema for pattern format
  claude-code.json
  codex.json
  aider.json

docs/
SPEC.md
CLAUDE.md
```

## Development phase

We are in **Phase 1: Agent + CLI Client**. Focus is:

1. Agent with stream relay, CoAP endpoints, IAM integration
2. CLI client with pair, connect, sessions, devices commands
3. Goal: interactive tmux session between two machines over Nabto with proper authentication

Do not work on iOS, pattern overlays, or multi-session tabs until Phase 1 is complete and tested.

## Key references

- SPEC.md (this repo): full product specification
- Nabto Embedded SDK: https://github.com/nabto/nabto-embedded-sdk
- Nabto Client SDK examples: https://github.com/nabto/nabto-client-sdk-examples
- Nabto tcp_tunnel_device (IAM reference): in the embedded SDK repo under `examples/tcp_tunnel_device/`
- SwiftTerm: https://github.com/migueldeicaza/SwiftTerm
- NabtoEdgeClientSwift: available via CocoaPods

## Common mistakes to avoid

- Do not use git submodules. The Embedded SDK is fetched via FetchContent.
- Do not add CC-specific logic to the agent. The agent is tool-agnostic.
- Do not enable open pairing modes by default. Security posture: low probability, catastrophic impact.
- Do not skip IAM checks on any endpoint. Even GET /terminal/status requires authentication.
- Do not frame the stream data. It is a raw byte pipe, same as SSH.