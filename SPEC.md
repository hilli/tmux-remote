# NabtoShell: Remote Terminal Access via Nabto Edge

## Product Specification v0.1

---

## 1. Overview

NabtoShell provides secure, zero-configuration remote terminal access to tmux sessions from any device. It uses Nabto Edge for P2P connectivity, eliminating the need for SSH port forwarding, dynamic DNS, or firewall configuration.

The system consists of a device agent and multiple clients:

- **NabtoShell Agent**: A daemon running on the developer's machine (Linux/macOS) that exposes tmux sessions over Nabto Edge streams.
- **NabtoShell CLI Client**: A command-line client for connecting from another computer. Pipes a Nabto stream to the local terminal. The simplest client and the primary driver for agent development.
- **NabtoShell iOS App**: A native iOS terminal client with optional smart UI overlays for interactive CLI tools (Claude Code, Codex, Aider, etc.).
- **NabtoShell Android App**: Deferred. Same architecture as iOS.

NabtoShell is a generic remote terminal tool. Support for specific CLI tools (AI coding agents, etc.) is handled entirely by an optional, pluggable pattern matching system on the client side. The agent knows nothing about what runs in the terminal.

---

## 2. Goals

1. Provide interactive remote terminal access to tmux sessions with no network configuration.
2. Support multiple client platforms: CLI (primary development driver), iOS, Android (deferred).
3. Optionally enhance mobile usability for interactive CLI tools (Claude Code, Codex, Aider) via pluggable pattern overlays.
4. Serve as a compelling Nabto Edge demo: "Secure remote terminal, no SSH config."

---

## 3. Architecture

```
┌─────────────────────┐              ┌──────────────────────┐
│   CLI Client         │   Nabto     │   Agent              │
│                      │   P2P      │                      │
│  stdin/stdout ──────►│◄──────────►│  ┌────────────────┐  │
│  (raw terminal mode) │  Stream    │  │  PTY (forkpty)  │  │
│                      │  (bidir)   │  │  tmux attach    │  │
└─────────────────────┘             │  └────────────────┘  │
                                     │                      │
┌─────────────────────┐             │  ┌────────────────┐  │
│   iOS App            │   Nabto    │  │  CoAP Endpoints │  │
│                      │   P2P     │  │  /terminal/*    │  │
│  ┌────────────────┐  │◄──────────►│  └────────────────┘  │
│  │  SwiftTerm     │  │  Stream    │                      │
│  │  TerminalView  │  │  (bidir)   │  ┌────────────────┐  │
│  └────────────────┘  │            │  │  IAM Module     │  │
│                      │            │  │  (auth/pairing) │  │
│  ┌────────────────┐  │  CoAP     │  └────────────────┘  │
│  │  Pattern       │  │  (req/res) │                      │
│  │  Overlay       │  │            │                      │
│  └────────────────┘  │            │                      │
└─────────────────────┘              └──────────────────────┘
```

All clients use the same agent, the same Nabto stream protocol, and the same CoAP endpoints. The CLI client is a transparent byte pipe to the local terminal. The iOS app adds terminal emulation (SwiftTerm) and optional pattern overlays. Both connect to the same tmux sessions.

### Communication Channels

| Channel | Nabto Primitive | Purpose |
|---------|----------------|---------|
| Terminal data | Stream | Bidirectional PTY byte relay (main data path) |
| Control | CoAP | Terminal resize, session management, status queries |

---

## 4. Device Agent

### 4.1 Overview

A C application using the Nabto Edge Embedded SDK. Runs as a daemon or foreground process on the developer's machine.

### 4.2 Startup

1. Load or generate device keypair (persist to `~/.nabtoshell/device.key`).
2. Load configuration: product ID, device ID, Nabto Cloud credentials.
3. Initialize Nabto device, attach to basestation.
4. Register CoAP endpoint handlers.
5. Begin listening for incoming streams.

### 4.3 Stream Handler (PTY Relay)

When a client opens a stream:

1. Determine target tmux session (from a preceding CoAP request or default to a configured session name).
2. Call `forkpty()` to spawn a child process running `tmux attach-session -t <session>`.
3. Set initial terminal size via `ioctl(TIOCSWINSZ)` if the client has sent dimensions via CoAP.
4. Enter relay loop:
   - Read from PTY fd, write to Nabto stream.
   - Read from Nabto stream, write to PTY fd.
5. On stream close or PTY exit, clean up both ends.

The relay should use non-blocking I/O or an event loop (libevent is already a dependency of the embedded SDK). Buffer sizes should be tuned for interactive latency; 4096 bytes per read is a reasonable starting point.

### 4.4 CoAP Endpoints

#### `POST /terminal/resize`

Payload (CBOR):
```
{
  "cols": uint16,
  "rows": uint16
}
```

Sets terminal size on the active PTY via `ioctl(fd, TIOCSWINSZ, &ws)` and sends `SIGWINCH` to the child process group.

Response: 2.04 Changed

#### `GET /terminal/sessions`

Lists available tmux sessions.

Internally runs `tmux list-sessions -F "#{session_name}:#{session_width}:#{session_height}:#{session_attached}"` and returns:

```
[
  {
    "name": "main",
    "cols": 200,
    "rows": 50,
    "attached": 1
  },
  ...
]
```

Response: 2.05 Content (CBOR)

#### `POST /terminal/attach`

Payload (CBOR):
```
{
  "session": "main",
  "cols": 80,
  "rows": 24,
  "create": false
}
```

Sets the target session for the next stream connection. If `create` is true and the session does not exist, creates it first (`tmux new-session -d -s <name>`). The agent stores the target per-connection.

Response: 2.01 Created

#### `POST /terminal/create`

Payload (CBOR):
```
{
  "session": "cc-work",
  "cols": 80,
  "rows": 24,
  "command": "claude"
}
```

Creates a new tmux session and optionally starts a command in it (e.g., `claude`, `aider`, or just a shell). If `command` is omitted, starts the user's default shell.

Response: 2.01 Created

#### `GET /terminal/status`

Returns agent health and connection info:
```
{
  "version": "0.1.0",
  "active_sessions": 1,
  "uptime_seconds": 3600
}
```

Response: 2.05 Content (CBOR)

### 4.5 Authentication and Access Control

NabtoShell integrates the Nabto Edge IAM module (`nm_iam`) from the embedded SDK, following the same proven approach as the tcp_tunnel_device reference application. This is not optional or simplified for the demo: granting remote terminal access demands production-grade authentication.

#### IAM Integration

The agent uses the IAM module for:
- Managing pairing (Password Invite mode exclusively)
- Storing paired users and their public key fingerprints
- Access control on every CoAP and stream request
- Persisting IAM state to disk across restarts

Every CoAP endpoint and the stream handler call `nm_iam_check_access()` before processing. Unpaired or unauthorized connections cannot open a terminal.

#### Single Role Model

There is only one role: **Owner**. NabtoShell grants full terminal access or no access. There is no meaningful "limited" permission for a remote shell; a multi-role model would be security theater.

| Role | Policies | Permissions |
|------|----------|-------------|
| Unpaired | Pairing | `Pairing:Get`, `Pairing:Password` (pairing endpoints only) |
| Owner | Terminal | All terminal and session operations |

The `Terminal` policy grants: `Terminal:Connect`, `Terminal:ListSessions`, `Terminal:CreateSession`, `Terminal:Resize`.

#### Pairing: Invite Only, Closed by Default

NabtoShell uses **Password Invite Pairing** exclusively. No other pairing mode is ever enabled in normal operation.

The flow:
1. On `--init`, the system pre-creates an initial user with a generated one-time password and SCT.
2. The agent prints the pairing string to stdout on startup.
3. The user copies it to their phone and pairs.
4. The invitation is consumed. Pairing is now closed. No further clients can pair.
5. To add another device, the user runs `nabtoshell-agent --add-user <n>` at the server terminal. This creates a new one-time invitation and prints it. After the new client pairs, pairing closes again.

This design reflects the risk profile: low probability of attack, catastrophic impact if compromised. Every paired device required a deliberate action by someone with physical access to the machine running the agent. A leaked pairing string that has already been used is worthless. An unused leaked string can be revoked with `nabtoshell-agent --remove-user <n>`.

The `--demo-init` flag is the only way to enable Password Open Pairing, and it prints a prominent warning.

### 4.6 First Run Experience

The first-run UX is critical both for usability and as a marketing showcase for Nabto's security model. The agent startup output is the first thing a developer sees, and for many it will be their first exposure to Nabto. It must clearly communicate why this is secure without being verbose.

#### `nabtoshell-agent --init`

Interactive first-time setup, following the tcp_tunnel_device pattern:

```
$ nabtoshell-agent --init
No configuration found. Creating initial setup.

Product ID: pr-xxxxxxxx
Device ID:  de-yyyyyyyy

Generated device keypair.
Fingerprint: 08c955a5f7505f16f03bc3e3e0db89ff56ce571e0dd6be153c5bae9174d62ac6

Register this fingerprint in the Nabto Cloud Console before starting.

Configuration written to ~/.nabtoshell/
```

#### Normal startup (no users paired yet)

```
$ nabtoshell-agent

######## NabtoShell ########
# Product ID:     pr-xxxxxxxx
# Device ID:      de-yyyyyyyy
# Fingerprint:    08c955a5...
# Version:        0.1.0
#
# ── Security ──────────────────────────────────────────────
#
#  NabtoShell uses the same trust model as SSH: client and
#  device exchange public keys once during pairing. All
#  subsequent connections are authenticated with these keys.
#  No passwords are stored after pairing. All traffic is
#  end-to-end encrypted (DTLS with ECC).
#
#  Unlike SSH, no ports are opened on this machine and no
#  firewall or port forwarding is needed. The Nabto
#  basestation mediates P2P connection setup but never
#  sees your data.
#
# ── Pairing ───────────────────────────────────────────────
#
#  No users paired yet. Pair your phone by copying
#  this string into the NabtoShell app:
#
#  p=pr-xxxxxxxx,d=de-yyyyyyyy,u=owner,pwd=CbAHaqpKKrhK,sct=TUfe3n3hhhM9
#
#  This is a one-time pairing password. After you pair,
#  it is invalidated and pairing is closed.
#
# ── tmux sessions ─────────────────────────────────────────
#  main     200x50  (attached)
#
######## Waiting for pairing... ########
```

#### After owner has paired

```
######## NabtoShell ########
# Product ID:     pr-xxxxxxxx
# Device ID:      de-yyyyyyyy
# Version:        0.1.0
#
# Paired users:   owner (ulrik-iphone)
# Pairing:        closed
#
# To add another device, run:
#   nabtoshell-agent --add-user <name>
#
# ── tmux sessions ─────────────────────────────────────────
#  main     200x50  (attached)
#  cc-work  120x40
#
######## Attached to basestation ########
```

#### `nabtoshell-agent --add-user <name>`

Creates a one-time invitation for an additional client:

```
$ nabtoshell-agent --add-user tablet
Created invitation for user 'tablet'.

Copy this string into the NabtoShell app on the new device:

  p=pr-xxxxxxxx,d=de-yyyyyyyy,u=tablet,pwd=xK9mRtYwZp3n,sct=Hf7kL2nQwR4s

This invitation can only be used once. Pairing will close
again after this device pairs.
```

#### `nabtoshell-agent --remove-user <name>`

Revokes access for a paired user or cancels an unused invitation:

```
$ nabtoshell-agent --remove-user tablet
Removed user 'tablet'. Their public key has been deleted.
They will no longer be able to connect.
```

#### `nabtoshell-agent --demo-init`

Convenience mode for demos and testing. Enables Password Open Pairing (shared password, any number of clients). Prints a clear warning:

```
WARNING: Demo mode enables open pairing. Anyone with the pairing
password can gain terminal access. Do not use in production.
```

### 4.7 Implementation Language and Dependencies

- Language: C (consistent with Nabto Embedded SDK ecosystem)
- Dependencies: nabto-embedded-sdk (includes libevent and IAM module), CBOR library (tinycbor or equivalent)
- Build: CMake
- Target platforms: anywhere the embedded SDK builds (Linux, macOS, Windows)

---

## 5. CLI Client

### 5.1 Overview

A command-line client for connecting to a NabtoShell agent from another computer. It uses the Nabto Client SDK (binary release) and runs on Linux and macOS. The CLI client is the simplest client and serves as the primary driver for developing and testing the agent.

The client is a transparent byte pipe: it connects to the agent, opens a Nabto stream, and relays bytes between the stream and the local terminal's stdin/stdout. The local terminal handles all rendering.

### 5.2 Usage

```
nabtoshell pair <pairing-string>          # One-time pairing
nabtoshell connect <device-name>          # Connect to default session
nabtoshell connect <device-name> -s main  # Connect to specific session
nabtoshell connect <device-name> --new claude  # Create session with command
nabtoshell sessions <device-name>         # List tmux sessions
nabtoshell devices                        # List saved devices
```

### 5.3 Connect Flow

1. Look up device bookmark from `~/.nabtoshell-client/` by name.
2. Create a Nabto client connection with stored product ID, device ID, client private key, and SCT.
3. `nabto_client_connection_connect()`.
4. Send `POST /terminal/attach` (CoAP) with session name and terminal dimensions from `ioctl(TIOCGWINSZ)`.
5. Open stream on port 1 via `nabto_client_stream_open()`.
6. Set local terminal to raw mode (`cfmakeraw`).
7. Enter relay loop: `select()` on stdin and the Nabto stream fd (or use the async API with futures).
8. Handle `SIGWINCH`: send `POST /terminal/resize` via CoAP when the local terminal is resized.
9. On stream close or EOF, restore terminal and exit.

### 5.4 Pairing Flow

1. Parse the pairing string to extract product ID, device ID, username, password, and SCT.
2. Generate a client keypair (if not already created).
3. Connect to the device and complete Password Invite pairing via the IAM CoAP endpoints.
4. Store the device bookmark (name, product ID, device ID, device fingerprint, SCT) in `~/.nabtoshell-client/devices.json`.
5. The user can optionally provide a friendly name: `nabtoshell pair <string> --name work-machine`.

### 5.5 Configuration

Client state is stored in `~/.nabtoshell-client/`:

```
~/.nabtoshell-client/
  client.key              # Client private key
  devices.json            # Saved device bookmarks
```

### 5.6 Dependencies

- Nabto Client SDK (binary release, vendored in repo)
- Build: CMake, following the same pattern as nabto-client-sdk-examples
- Target platforms: Linux, macOS

---

## 6. iOS App

### 6.1 Overview

A native Swift/UIKit (or SwiftUI) iOS application using the NabtoEdgeClientSwift SDK.

### 6.2 Connection Flow

**First connection (pairing):**

1. User taps "Add Device" and pastes the pairing string from the agent's terminal.
2. App parses the string to extract product ID, device ID, pairing password, and SCT.
3. App generates a client keypair (if not already created) and connects to the device.
4. PAKE-based key exchange using the one-time password. Public keys are exchanged.
5. Device stores the client's public key fingerprint; app stores the device bookmark (IDs + device fingerprint + SCT).
6. Device appears in the app's device list.

**Subsequent connections:**

1. User selects a device from the saved list.
2. App creates a `Connection`, sets options (product ID, device ID, client private key, SCT).
3. `connection.connect()` establishes the Nabto P2P connection, authenticated via stored public keys.
4. App sends `GET /terminal/sessions` (CoAP) to discover available tmux sessions.
5. User picks a session (or app auto-selects if only one exists).
6. App sends `POST /terminal/attach` with session name and current terminal dimensions.
7. App opens a stream via `connection.createStream()` and calls `stream.open()`.
8. Stream relay begins: `stream.readSome()` feeds into SwiftTerm, SwiftTerm key events go to `stream.write()`.

### 6.3 Terminal Rendering

Use **SwiftTerm** (github.com/migueldeicaza/SwiftTerm) for terminal emulation.

- `TerminalView` handles all ANSI/xterm escape sequence rendering.
- Feed raw bytes from the Nabto stream into the terminal view.
- Capture key/input events from the terminal view and write to the Nabto stream.
- On terminal resize (device rotation, split view), send `POST /terminal/resize` via CoAP and update SwiftTerm dimensions.

### 6.4 Pattern Overlay System

This is the key differentiator for mobile usability. The app runs in a hybrid mode where the raw terminal is always visible, but native UI controls overlay it when interactive prompts from supported CLI tools are detected.

#### 6.4.1 Pattern Detection

The app maintains a pattern matcher that operates on the stream data after ANSI stripping. Pattern definitions are loaded from a JSON configuration file fetched from a remote URL on app launch (with a bundled fallback).

Pattern config format:
```json
{
  "version": 2,
  "agents": {
    "claude-code": {
      "name": "Claude Code",
      "patterns": [
        {
          "id": "permission_prompt",
          "type": "yes_no",
          "regex": "Do you want to (allow|proceed|run).*\\?.*\\(y\\/n\\)",
          "actions": [
            { "label": "Yes", "keys": "y" },
            { "label": "No", "keys": "n" }
          ]
        },
        {
          "id": "menu_selection",
          "type": "numbered_menu",
          "regex": "\\[\\d+\\]\\s+.+",
          "multi_line": true,
          "action_template": { "keys": "{number}\n" }
        },
        {
          "id": "diff_review",
          "type": "accept_reject",
          "regex": "Do you want to apply these changes",
          "actions": [
            { "label": "Accept", "keys": "y" },
            { "label": "Reject", "keys": "n" }
          ]
        }
      ]
    },
    "codex": {
      "name": "OpenAI Codex CLI",
      "patterns": []
    },
    "aider": {
      "name": "Aider",
      "patterns": []
    }
  }
}
```

The app auto-detects which agent is running by matching on initial terminal output (e.g., the CC welcome banner), or the user selects it manually. Only the active agent's patterns are evaluated.

#### 6.4.2 Overlay Rendering

When a pattern matches:

1. An overlay panel slides up from the bottom of the screen, partially covering the terminal.
2. The overlay shows the detected prompt context and action buttons.
3. Tapping a button sends the corresponding keystroke sequence via `stream.write()`.
4. The overlay dismisses automatically when the terminal output moves past the prompt.

The terminal view remains interactive underneath. The user can always dismiss the overlay and type manually.

#### 6.4.3 Pattern Config Updates

- On app launch, fetch `https://<config-host>/nabtoshell/cc-patterns.json`.
- Compare version number with cached config.
- If newer, update local cache.
- If fetch fails, use bundled/cached version.
- No App Store review needed for pattern updates.

### 6.5 Additional Mobile UX Considerations

- **Keyboard**: Use a terminal-optimized keyboard accessory bar with common keys: Tab, Escape, Ctrl, arrow keys, pipe, slash. This is critical for usability.
- **Quick actions**: Swipe gestures for common operations (e.g., swipe down to scroll back, pinch to adjust font size).
- **Connection status**: Persistent indicator showing P2P vs relay, latency.
- **Background handling**: When the app moves to background, keep the Nabto connection alive briefly (iOS permits ~30s). On return, reconnect and reattach to the same tmux session. tmux handles the session persistence, so no data is lost.
- **Multiple sessions**: Tab bar or swipe-between interface for managing multiple tmux sessions.

### 6.6 Dependencies

- NabtoEdgeClientSwift (CocoaPods)
- SwiftTerm (SPM or CocoaPods)
- No other significant dependencies

---

## 7. Protocol Details

### 7.1 Stream Port

The device agent listens on Nabto stream port **1** (the default). If multiple services are needed in the future, additional ports can be used.

### 7.2 Stream Data Format

Raw bytes. No framing, no length prefixing. The stream is a transparent byte pipe between the client and the PTY, identical to what SSH does. Terminal escape sequences are handled entirely by SwiftTerm on the client side.

### 7.3 CoAP Content Format

All CoAP payloads use CBOR (content format 60). CBOR is compact, well-supported in both C and Swift, and already used in Nabto Edge examples.

### 7.4 Connection Lifecycle

```
Client                              Device Agent
  │                                      │
  │──── Connect (Nabto) ───────────────►│
  │                                      │
  │──── GET /terminal/sessions ────────►│
  │◄─── 2.05 [session list] ───────────│
  │                                      │
  │  (optional: create new session)      │
  │──── POST /terminal/create ─────────►│
  │     {session: "cc", command: "claude"}
  │◄─── 2.01 Created ──────────────────│
  │                                      │
  │──── POST /terminal/attach ─────────►│
  │     {session: "cc", cols, rows}      │
  │◄─── 2.01 Created ──────────────────│
  │                                      │
  │──── Stream Open (port 1) ──────────►│
  │                                      │ ← forkpty() + tmux attach
  │◄─── stream data (PTY output) ──────│
  │──── stream data (keystrokes) ──────►│
  │         ... interactive session ...  │
  │                                      │
  │──── POST /terminal/resize ─────────►│  (on device rotation)
  │     {cols: 120, rows: 40}            │
  │◄─── 2.04 Changed ──────────────────│
  │                                      │
  │──── Stream Close ──────────────────►│
  │──── Connection Close ──────────────►│
```

---

## 8. Configuration and Setup

### 8.1 Device Agent Setup

See section 4.6 for the `--init` first-run flow and section 5.4 for the CLI pairing flow. Configuration is stored in `~/.nabtoshell/`:

```
~/.nabtoshell/
  config/device.json          # Product ID, Device ID, server settings
  config/iam_config.json      # IAM policies, roles (static)
  state/iam_state.json        # Paired users, pairing mode state (mutable)
  keys/device.key             # Device private key
```

Config file (`config/device.json`):
```json
{
  "ProductId": "pr-xxxxxxxx",
  "DeviceId": "de-yyyyyyyy"
}
```

The IAM config and initial state are generated by `--init` with sensible defaults. The state file is updated automatically as users pair and is persisted on every change via the `nm_iam_set_state_changed_callback` mechanism.

### 8.2 Client Pairing Flow

Pairing is identical across all clients. The transport is the same (Nabto Client SDK), only the UI differs.

**CLI client:**

```
$ nabtoshell pair p=pr-xxxxxxxx,d=de-yyyyyyyy,u=owner,pwd=CbAHaqpKKrhK,sct=TUfe3n3hhhM9
Paired with device 'de-yyyyyyyy'. Saved as 'de-yyyyyyyy'.
Use --name to set a friendly name, or rename later with: nabtoshell rename de-yyyyyyyy <n>
```

**iOS app:**

1. Open the NabtoShell app, tap "Add Device".
2. Paste the pairing string into the text field (QR scan as secondary option).
3. Device appears in the app's device list.

**In both cases:** the client parses the pairing string, generates a keypair if needed, connects to the device, and completes the PAKE-based key exchange. Public keys are exchanged and stored. No password is retained after pairing.

---

## 9. Security

### 9.1 Threat Model

NabtoShell grants remote terminal access to a developer's machine. A compromise means an attacker can execute arbitrary commands as the user running the agent. This is equivalent to SSH access, and the security posture must match or exceed SSH.

### 9.2 Security Properties

- **End-to-end encryption**: All traffic is encrypted with DTLS using ECC key pairs. The Nabto basestation mediates connection setup (hole punching, relay fallback) but cannot decrypt traffic.
- **Mutual authentication**: After pairing, both client and device authenticate each other via public key fingerprints on every connection. No passwords are transmitted or stored after the initial pairing.
- **Zero exposed ports**: The device agent opens no listening TCP or UDP ports. All connectivity is outbound to the Nabto basestation. There is no attack surface for port scanners.
- **One-time pairing passwords**: Pairing passwords (for Password Invite mode) are single-use. After a client pairs, the password is invalidated on the device.
- **Role-based access control**: Every CoAP request and stream open is gated by `nm_iam_check_access()`. Unpaired connections can only access pairing endpoints.
- **PTY isolation**: The PTY is spawned as the user running the agent. tmux session access is limited to that user's sessions. The agent does not run as root.

### 9.3 Comparison with SSH

| Property | SSH | NabtoShell |
|----------|-----|------------|
| Key exchange | `ssh-keygen` + copy public key to server | Pairing string (one-time password for PAKE key exchange) |
| Authentication | Public key or password per connection | Public key (after initial pairing) |
| Encryption | AES/ChaCha20 over TCP | DTLS with ECC over UDP |
| Network requirements | Open port 22, firewall rules, possibly dynamic DNS | None. Outbound UDP only. |
| NAT traversal | Requires port forwarding or relay (ngrok, etc.) | Built-in P2P hole punching with relay fallback |

The trust model is identical: both rely on a one-time key exchange followed by public key authentication. NabtoShell's pairing is arguably more user-friendly than SSH's `ssh-copy-id` workflow, while providing the same cryptographic guarantees.

---

## 10. Marketing and Demo Strategy

### 10.1 Key Message

"Control Claude Code from your iPhone. No SSH, no port forwarding, no VPN. Same security, zero config."

### 10.2 Why Security Presentation Matters for Marketing

The target audience (developers running AI coding agents) is security-conscious. They will immediately ask "how is this secure?" when they see remote terminal access without SSH. If NabtoShell cannot answer this convincingly and immediately, developers will dismiss it regardless of how good the UX is.

This is why the agent's first-run output includes the security explanation directly in the terminal. It is the first thing a developer reads, before they even pair. The explanation is framed as a comparison to SSH because that is the mental model the audience already has. The goal is instant recognition: "ah, same trust model as SSH, just without the firewall hassle."

This security-first presentation also serves Nabto's broader marketing goals. Many potential Nabto customers are technical decision-makers evaluating connectivity solutions for IoT products. Seeing a concrete, security-critical application (remote terminal access) built on Nabto, with a clear explanation of the trust model, is more convincing than abstract claims about "enterprise-grade encryption." NabtoShell demonstrates that Nabto's security is strong enough to trust with shell access to a developer's machine.

### 10.3 Blog Post Outline

1. **The problem**: "I saw this Reddit post about running Claude Code remotely via tmux + SSH + firewall rules. It works, but the setup is painful."
2. **The SSH tax**: port forwarding, dynamic DNS, firewall holes, key management across devices, NAT traversal headaches.
3. **NabtoShell**: same security model, zero network config. Show the first-run output with the security explanation.
4. **How it works**: brief Nabto Edge architecture (P2P, basestation mediation, DTLS). Keep it concrete, not abstract.
5. **The mobile twist**: pattern overlays make AI coding agents usable from a phone. Show screenshots.
6. **Try it yourself**: link to open source repos.

### 10.4 Demo Scenario

1. Show the Reddit post about tmux + SSH + firewall configuration.
2. Show NabtoShell: `nabtoshell-agent --init`, start the agent, copy pairing string, paste in the app. Connected in under 30 seconds.
3. Walk through a Claude Code session on the phone: approve a file edit by tapping a button, watch code being written in real-time.
4. Show the P2P connection indicator: direct connection, low latency, no relay.
5. Highlight: "No ports were opened. No firewall rules were changed. No SSH keys were copied. The security is the same."

### 10.5 Publishable Artifacts

- Blog post on nabto.com
- Open source device agent and iOS app on GitHub
- Short video demo (60-90 seconds)
- README with the security comparison table from section 9.3

---

## 11. Repository Structure

```
nabtoshell/
  agent/                              # C, Nabto Embedded SDK
    CMakeLists.txt
    src/
  clients/
    cli/                              # C, Nabto Client SDK (binary)
      CMakeLists.txt
      src/
      nabto_client_sdk_library/       # Vendored binary release
    ios/                              # Swift, NabtoEdgeClientSwift
      NabtoShell.xcodeproj/
      NabtoShell/
    android/                          # Deferred
  patterns/
    schema.json                       # JSON schema for pattern format
    claude-code.json
    codex.json
    aider.json
  docs/
  CMakeLists.txt                      # Top-level: adds agent/ and clients/cli/
```

### Dependency Management

| Component | SDK | Source | Strategy |
|-----------|-----|--------|----------|
| Agent | Nabto Embedded SDK | github.com/nabto/nabto-embedded-sdk | CMake `FetchContent`, pinned to tag/commit |
| CLI client | Nabto Client SDK | github.com/nabto/nabto-client-sdk-releases | Vendored binary in `nabto_client_sdk_library/` |
| iOS app | NabtoEdgeClientSwift | CocoaPods | Pod dependency |
| Android app | NabtoEdgeClientAndroid | Maven | Deferred |

The Embedded SDK uses `FetchContent` so that `git clone` + `cmake --build` just works with no extra steps. The Client SDK binary is vendored because it is not built from source and is small (shared lib + headers).

---

## 12. Development Phases

### Phase 1: Agent + CLI Client
- Agent with stream relay, basic CoAP (resize, attach, sessions), and IAM (Password Invite pairing)
- CLI client with pair, connect, sessions, and devices commands
- Goal: interactive tmux session from one machine to another over Nabto, with proper authentication
- This phase proves the core protocol and agent without any UI framework dependencies

### Phase 2: iOS App
- SwiftTerm + Nabto stream
- Pairing via paste
- Session discovery and selection
- Keyboard accessory bar with terminal keys
- Connection status and reconnection logic

### Phase 3: Pattern Overlays
- Pattern schema and initial pattern sets (Claude Code, Codex, Aider)
- Agent auto-detection from terminal output
- iOS overlay rendering
- Remote pattern config fetch with local cache and bundled fallback

### Phase 4: Polish and Publish
- Multiple session tabs (iOS)
- Blog post and demo video
- Open source release

## 13. Resolved Design Decisions

1. **Generic terminal tool, not CC-specific**: NabtoShell is a generic secure remote terminal. Support for Claude Code and other AI coding agents is handled entirely by optional, pluggable pattern overlays on the client side. The agent knows nothing about what runs in the terminal.
2. **CLI client first**: The CLI client is developed before the iOS app. It is simpler (no UI framework), exercises the full agent protocol, and serves as a test harness. Any bug found is definitively agent-side or protocol-side.
3. **Single IAM role**: There is only one role (Owner). NabtoShell grants full terminal access or no access. A multi-role model would be security theater for a remote shell.
4. **Invite-only pairing, closed by default**: Password Invite Pairing is the only mode in normal operation. Pairing closes after each invitation is consumed. Every paired device required a deliberate action at the server terminal. Risk profile: low probability, catastrophic impact.
5. **Platform support**: The Embedded SDK is open source ANSI C and builds on Linux, macOS, and Windows via CMake. No platform-specific work needed.
6. **Android client**: Deferred. The Nabto Android SDK has equivalent stream/CoAP APIs. Straightforward to add later.
7. **Claude Agent SDK integration (considered and rejected)**: Would eliminate terminal scraping but turns NabtoShell into a full CC client (scope creep), only works for Claude Code (not agent-agnostic), and the pattern config JSON already supports multiple tools. Rejected in favor of raw PTY relay with pluggable client-side patterns.
8. **FetchContent over git submodules**: The Embedded SDK is fetched via CMake FetchContent with a pinned tag. Avoids the "forgot to init submodules" problem. The Client SDK binary is vendored because it is not built from source.
9. **Session creation**: The agent supports both attaching to existing tmux sessions and creating new ones, including optionally starting a command (e.g., `claude`, `aider`).
10. **File viewing**: Deferred. Terminal-only for now.