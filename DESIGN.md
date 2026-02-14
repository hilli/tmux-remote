# NabtoShell Design

## 1. System Overview

NabtoShell provides secure remote terminal access to tmux sessions using Nabto Edge peer-to-peer connectivity. It replaces SSH with a zero-configuration model: no port forwarding, firewall rules, or dynamic DNS. A lightweight agent runs on the host machine and relays PTY I/O over encrypted Nabto connections to authenticated clients.

```
 iOS App / CLI Client
       |
  [Nabto Edge P2P / DTLS]
       |
   NabtoShell Agent
       |
   tmux session <-> PTY
```

## 2. Components

| Component | Language | SDK | Purpose |
|-----------|----------|-----|---------|
| `agent/` | C | Nabto Embedded SDK | Serves terminal sessions, relays PTY data, detects patterns |
| `clients/cli/` | C | Nabto Client SDK | Command-line terminal client |
| `clients/ios/` | Swift | NabtoEdgeClientSwift | Mobile terminal with pattern overlay UI |

The agent is tool-agnostic. It knows nothing about what runs inside the terminal. Pattern definitions are loaded from JSON configuration files and evaluated generically.

## 3. Communication Model

### Nabto Streams

| Port | Name | Framing | Purpose |
|------|------|---------|---------|
| 1 | Data stream | None (raw bytes) | Bidirectional PTY relay, identical to SSH |
| 2 | Control stream | Length-prefixed CBOR | Session state, prompt lifecycle events, client resolve |

### CoAP Endpoints

All require IAM authorization. Payloads use CBOR (content format 60).

| Method | Path | Purpose |
|--------|------|---------|
| POST | `/terminal/attach` | Set target tmux session for next stream open |
| POST | `/terminal/resize` | Set PTY size, send SIGWINCH |
| POST | `/terminal/create` | Create a new tmux session |
| GET | `/terminal/sessions` | List tmux sessions |
| GET | `/terminal/status` | Agent health and uptime |

## 4. Security Model

NabtoShell grants remote shell access. A compromise means an attacker can execute arbitrary commands as the user running the agent. This is equivalent to SSH access, and the security posture must match or exceed SSH.

- **Single role: Owner.** Full access or no access. No "limited" roles. A multi-role model would be security theater for a remote shell.
- **Password Invite pairing only.** A one-time password is generated per invitation and closed after use. No other pairing mode is enabled.
- **Every endpoint checks IAM.** CoAP handlers and stream listeners call `nabtoshell_iam_check_access()`/`nabtoshell_iam_check_access_ref()` before processing. Unpaired connections can only access pairing endpoints.
- **DTLS with ECC.** All traffic is encrypted end-to-end by the Nabto platform. The basestation mediates connection setup but cannot decrypt traffic.

### Security Properties

- **End-to-end encryption**: DTLS with ECC key pairs. The Nabto basestation mediates connection setup (hole punching, relay fallback) but cannot decrypt traffic.
- **Mutual authentication**: After pairing, both client and device authenticate each other via public key fingerprints on every connection. No passwords are transmitted or stored after the initial pairing.
- **Zero exposed ports**: The device agent opens no listening TCP or UDP ports. All connectivity is outbound to the Nabto basestation. No attack surface for port scanners.
- **One-time pairing passwords**: Pairing passwords are single-use. After a client pairs, the password is invalidated on the device. A leaked pairing string that has already been used is worthless.
- **PTY isolation**: The PTY is spawned as the user running the agent. tmux session access is limited to that user's sessions. The agent does not run as root.

### Comparison with SSH

| Property | SSH | NabtoShell |
|----------|-----|------------|
| Key exchange | `ssh-keygen` + copy public key to server | Pairing string (one-time password for PAKE key exchange) |
| Authentication | Public key or password per connection | Public key (after initial pairing) |
| Encryption | AES/ChaCha20 over TCP | DTLS with ECC over UDP |
| Network requirements | Open port 22, firewall rules, possibly dynamic DNS | None. Outbound UDP only. |
| NAT traversal | Requires port forwarding or relay (ngrok, etc.) | Built-in P2P hole punching with relay fallback |

The trust model is identical: both rely on a one-time key exchange followed by public key authentication. NabtoShell's pairing is arguably more user-friendly than SSH's `ssh-copy-id` workflow, while providing the same cryptographic guarantees.

### Pairing Flow

The pairing flow is identical across all clients. The transport is the same (Nabto Client SDK); only the UI differs.

1. On `--init`, the agent pre-creates an initial owner invitation with a generated one-time password and SCT.
2. The invite pairing string is printed during `--init` and shown on startup while no client has paired yet.
3. The user copies it to their client (CLI command or mobile app paste).
4. The client parses the pairing string, generates a keypair if needed, connects, and completes the PAKE-based key exchange. Public keys are exchanged and stored.
5. The invitation is consumed. Pairing is now closed. No further clients can pair.
6. To add another device, run `nabtoshell-agent --add-user <name>` at the server terminal.

### Agent CLI

#### `nabtoshell-agent --init`

Interactive first-time setup:

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
#  No users paired yet. Pair your phone by copying
#  this string into the NabtoShell app:
#
#  p=pr-xxxxxxxx,d=de-yyyyyyyy,u=owner,pwd=CbAHaqpKKrhK,sct=TUfe3n3hhhM9
#
#  This is a one-time pairing password. After you pair,
#  it is invalidated and pairing is closed.
#
######## Waiting for pairing... ########
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

Removed. The agent enforces invite-only pairing. Use `--init` for first setup and `--add-user` for additional one-time invitations.

### Agent Configuration Directory

```
~/.nabtoshell/
  config/device.json          # Product ID, Device ID, server settings
  config/iam_config.json      # IAM policies, roles (static)
  state/iam_state.json        # Paired users and pending one-time invitations (mutable)
  keys/device.key             # Device private key
  patterns/*.json             # Pattern definitions
```

## 5. CLI Client

A command-line client for connecting to a NabtoShell agent from another computer. Uses the Nabto Client SDK (binary release). The CLI client is the simplest client and serves as the primary driver for developing and testing the agent. It is a transparent byte pipe: connects, opens a Nabto stream, and relays bytes between the stream and the local terminal's stdin/stdout.

### Usage

```
nabtoshell pair <pairing-string>                    # One-time pairing
nabtoshell attach <device-name> [session]            # Attach to existing tmux session
nabtoshell create <device-name> [session] [command]  # Create new session, optionally run command
nabtoshell sessions <device-name>                    # List tmux sessions
nabtoshell devices                                   # List saved devices
nabtoshell rename <device-name> <new-name>           # Rename a saved device
```

Aliases: `a` for `attach`, `c`/`n`/`new`/`new-session` for `create`.

### Attach/Create Flow

1. Look up device bookmark from `~/.nabtoshell-client/` by name.
2. Create a Nabto client connection with stored product ID, device ID, client private key, and SCT.
3. `nabto_client_connection_connect()`.
4. Send `POST /terminal/attach` or `POST /terminal/create` (CoAP) with session name and terminal dimensions from `ioctl(TIOCGWINSZ)`.
5. Open stream on port 1 via `nabto_client_stream_open()`.
6. Set local terminal to raw mode (`cfmakeraw`).
7. Enter relay loop: `select()` on stdin and the Nabto stream fd (or use the async API with futures).
8. Handle `SIGWINCH`: send `POST /terminal/resize` via CoAP when the local terminal is resized.
9. On stream close or EOF, restore terminal and exit.

### Client Configuration

```
~/.nabtoshell-client/
  client.key              # Client private key
  devices.json            # Saved device bookmarks
```

## 6. iOS App

### Pairing (First Connection)

1. User taps "Add Device" and pastes the pairing string from the agent's terminal.
2. App parses the string to extract product ID, device ID, pairing password, and SCT.
3. App generates a client keypair (if not already created) and connects to the device.
4. PAKE-based key exchange using the one-time password. Public keys are exchanged.
5. Device stores the client's public key fingerprint; app stores the device bookmark.
6. Device appears in the app's device list.

### Subsequent Connections

1. On the device list, the app connects to saved devices in the background.
2. The control stream (port 2) delivers session lists and prompt lifecycle events. The app displays available tmux sessions per device and renders prompt overlays from server events.
3. User taps a session to enter the terminal view.
4. App sends `POST /terminal/attach` with session name and current terminal dimensions.
5. App opens a stream via `connection.createStream()` and calls `stream.open()`.
6. Stream relay begins: `stream.readSome()` feeds into SwiftTerm, SwiftTerm key events go to `stream.write()`.

### Connection Lifecycle

```
Client                              Device Agent
  |                                      |
  |---- Connect (Nabto) --------------->|
  |                                      |
  |---- Control Stream Open (port 2) -->|
  |<--- sessions list (CBOR, periodic) -|  (background, every 2s)
  |                                      |
  |  (user selects a session)            |
  |                                      |
  |---- POST /terminal/attach --------->|
  |     {session: "main", cols, rows}    |
  |<--- 2.01 Created -------------------|
  |                                      |
  |---- Data Stream Open (port 1) ----->|
  |                                      | <- forkpty() + tmux attach
  |<--- stream data (PTY output) -------|
  |---- stream data (keystrokes) ------>|
  |         ... interactive session ...  |
  |                                      |
  |<--- pattern_present (port 2, CBOR) -|  (prompt instance appears)
  |<--- pattern_update (port 2, CBOR) --|  (same instance redraw/update)
  |<--- pattern_gone (port 2, CBOR) ----|  (instance disappears)
  |---- pattern_resolve (port 2) ------>|  (user action or dismiss with instance_id)
  |                                      |
  |---- POST /terminal/resize --------->|  (on device rotation)
  |     {cols: 120, rows: 40}            |
  |<--- 2.04 Changed -------------------|
  |                                      |
  |---- Stream Close ------------------>|
  |---- Connection Close --------------->|
```

---

## 7. Prompt Detection System (V2)

### 7.1 Design Goals

1. Detect interactive prompts from terminal UIs using the visible screen state, not historical text tails.
2. Emit stable prompt lifecycle events with no match/dismiss oscillation during redraws.
3. Assign a stable prompt instance identity so redraws of the same prompt do not create new overlays.
4. Keep the iOS app as a renderer/action sender, not a detector.
5. Keep the agent tool-agnostic: behavior is driven by external JSON rules.
6. Guarantee deterministic replay behavior regardless of PTY chunk boundaries.

### 7.2 Architecture Overview

Prompt detection runs on the agent and is evaluated from canonical terminal snapshots.

```text
PTY output (raw bytes)
    |
    v
[VT Parser]
    |
    v
[TerminalState Snapshot]
  - visible lines
  - cursor position
  - viewport size
  - alt-screen flag
    |
    v
[Prompt Rule Evaluator]
    |
    v
[Prompt Lifecycle FSM]
  - instance_id
  - revision
  - present/update/gone
    |
    v
[Control Stream Protocol V2]
    |
    v
[iOS Overlay]
```

### 7.3 Canonical Terminal State

`TerminalState` is the only detector input. It is updated incrementally by parsing VT/ANSI control sequences from PTY bytes.

Each snapshot includes:
- normalized visible lines (UTF-8 safe)
- cursor row/column
- viewport dimensions
- active screen mode (main/alt)
- monotonic snapshot sequence

Normalization rules are deterministic and replay-safe:
- line endings normalized to LF
- invalid or incomplete UTF-8 never exposed to rules
- trailing cursor artifacts and purely decorative whitespace are normalized

This eliminates stale prompt copies from detection logic because only the current screen is matched.

### 7.4 Prompt Rule Configuration

Rules are loaded from `~/.nabtoshell/patterns/*.json` at startup.

Config schema is V3 only (single format, no compatibility branches):

```json
{
  "version": 3,
  "agents": {
    "claude-code": {
      "name": "Claude Code",
      "detect": ["claude code", "claude.ai"],
      "patterns": [
        {
          "id": "numbered_prompt",
          "type": "numbered_menu",
          "prompt_regex": "Do you want to .+\\?",
          "item_regex": "^(?:[>*\\-\\u276F]\\s*)?(\\d+)\\.\\s+(.+)$",
          "action_template": { "keys": "{number}" }
        }
      ]
    }
  }
}
```

Supported prompt types:

| Type | Action Source | Example |
|------|---------------|---------|
| `yes_no` | Static action list from config | `Continue? (y/n)` |
| `accept_reject` | Static action list from config | `Apply changes?` |
| `numbered_menu` | Extracted sequential menu items | `1. Yes / 2. No / 3. Cancel` |

### 7.5 Prompt Instance Identity

Each detected candidate prompt is assigned:

`instance_id = hash(pattern_id, normalized_prompt, normalized_actions, normalized_anchor)`

Where:
- `normalized_anchor` is a stable location fingerprint from the current snapshot region
- `normalized_actions` preserves order and key mappings

Properties:
- same prompt across redraws -> same `instance_id`
- semantically different prompt -> different `instance_id`

### 7.6 Lifecycle State Machine

Per data stream, prompt state is managed by a strict FSM.

```text
                 +-------------------------------+
                 | same instance redraw/update   |
                 v                               |
none --present--> active(instance_id, rev=1) ----+
                    |                |
                    | resolve        | absent for gone window
                    v                v
                resolved         gone(instance_id)
                    |                |
                    +------> none <--+
```

Rules:
- `present` is emitted once per new `instance_id`
- `update` is emitted only for same `instance_id` with changed prompt/actions
- `gone` is emitted once when instance is absent for configured window
- client resolve suppresses only that exact `instance_id`

### 7.7 Control Stream Protocol V2

**Port:** 2  
**Framing:** `[4-byte big-endian uint32 payload length][CBOR payload]`

#### Messages: Agent to Client

**Pattern Present:**
```cbor
{
  "type": "pattern_present",
  "instance_id": "8f0d63c1e4...",
  "revision": 1,
  "pattern_id": "numbered_prompt",
  "pattern_type": "numbered_menu",
  "prompt": "Do you want to create bar.txt?",
  "actions": [
    {"label": "Yes", "keys": "1"},
    {"label": "Yes, allow all", "keys": "2"},
    {"label": "No", "keys": "3"}
  ]
}
```

**Pattern Update:**
```cbor
{
  "type": "pattern_update",
  "instance_id": "8f0d63c1e4...",
  "revision": 2,
  "prompt": "Do you want to create bar.txt?",
  "actions": [
    {"label": "Yes", "keys": "1"},
    {"label": "Yes, allow all", "keys": "2"},
    {"label": "No", "keys": "3"},
    {"label": "Cancel", "keys": "4"}
  ]
}
```

**Pattern Gone:**
```cbor
{
  "type": "pattern_gone",
  "instance_id": "8f0d63c1e4..."
}
```

**Sessions** (unchanged):
```cbor
{
  "type": "sessions",
  "sessions": [
    {"name": "main", "cols": 80, "rows": 24, "attached": 1}
  ]
}
```

#### Messages: Client to Agent

**Pattern Resolve** (action tap or dismiss):
```cbor
{
  "type": "pattern_resolve",
  "instance_id": "8f0d63c1e4...",
  "decision": "action",
  "keys": "1"
}
```

`decision: "dismiss"` omits `keys`.

### 7.8 iOS Client Integration

Server event path:

```text
Control stream (port 2)
    |
    v
ConnectionManager.controlReadLoop()
    |
    v
decode V2 event (present/update/gone)
    |
    v
PatternEngine.applyServerEvent(instance_id,...)
    |
    v
PatternOverlayView state update
```

User action path:

```text
User taps action/dismiss
    |
    +--> write keys to data stream (port 1, action only)
    |
    +--> send pattern_resolve(instance_id, decision, keys?) on port 2
```

No client debounce is required. Overlay state follows server lifecycle events.

### 7.9 Determinism and Replay Guarantees

The detector must satisfy:

1. Same PTY byte stream -> same prompt event sequence.
2. Event sequence is invariant to PTY chunking.
3. Continuous redraw of one prompt does not emit `gone` then `present` cycles.
4. Resolving one prompt instance does not suppress a future distinct prompt.

Replay tests use `.ptyr` recordings and chunk-fuzz variants of the same recordings.

### 7.10 Tuning Parameters

| Parameter | Purpose |
|-----------|---------|
| `gone_window_snapshots` / `gone_window_ms` | Delay before emitting `pattern_gone` |
| `max_actions` | Cap for resolved action list size |
| `resolved_ttl_ms` | How long resolved instance suppression is retained |
| `snapshot_region_policy` | Anchor strategy for stable `instance_id` generation |

### 7.11 Threading Model

#### Agent Threads per Data Stream

| Thread | Function | Role |
|--------|----------|------|
| Setup | `stream_setup_thread` | Spawns PTY, tmux attach, initializes prompt detector |
| PTY Reader | `pty_reader_thread` | Reads PTY fd, feeds detector, writes PTY bytes to Nabto stream |
| Stream Reader | `stream_reader_thread` | Reads Nabto stream, writes to PTY fd |

Prompt detection executes in the PTY reader thread (`nabtoshell_prompt_detector_feed()`), and event callbacks send V2 CBOR messages on the control stream.

#### Agent Threads per Control Stream

| Thread | Function | Role |
|--------|----------|------|
| Monitor | `monitor_thread_func` | Polls tmux sessions, broadcasts session snapshots, syncs active prompt instances to newly connected control streams |
| Reader | `reader_thread_func` | Reads `pattern_resolve` from client and forwards resolution to the owning data-stream detector |

#### Critical Rule: No Blocking in Nabto Callbacks

All Nabto SDK callbacks execute on the SDK core event loop thread. Blocking work is deferred to dedicated threads.

## 8. File Layout

```
agent/
  src/
    main.c                               # Entry point, argument parsing
    nabtoshell.h / .c                    # Main app struct, startup, shutdown
    nabtoshell_banner.h / .c             # Startup banner output
    nabtoshell_device.h / .c             # Nabto device setup
    nabtoshell_init.h / .c               # --init, --add-user, --remove-user logic
    nabtoshell_stream.h / .c             # Data stream (port 1), PTY relay
    nabtoshell_control_stream.h / .c     # Control stream (port 2), events
    nabtoshell_tmux.h / .c               # tmux session utilities
    nabtoshell_terminal_state.h / .c     # VT parsing and canonical screen snapshots
    nabtoshell_prompt_rules.h / .c       # Prompt rule evaluation on snapshots
    nabtoshell_prompt_lifecycle.h / .c   # Instance FSM: present/update/gone/resolved
    nabtoshell_prompt_detector.h / .c    # End-to-end detector orchestration
    nabtoshell_pattern_config.h / .c     # JSON config parsing
    nabtoshell_prompt_protocol.h / .c    # CBOR encode/decode for V2 prompt messages
    nabtoshell_coap_handler.h / .c       # CoAP endpoint scaffold
    nabtoshell_coap_attach.c             # POST /terminal/attach handler
    nabtoshell_coap_create.c             # POST /terminal/create handler
    nabtoshell_coap_resize.c             # POST /terminal/resize handler
    nabtoshell_coap_sessions.c           # GET /terminal/sessions handler
    nabtoshell_coap_status.c             # GET /terminal/status handler
    nabtoshell_iam.h / .c                # IAM integration
    nabtoshell_session.h / .c            # Session map
  tests/
    test_terminal_state.c                # VT parser and snapshot normalization tests
    test_prompt_rules.c                  # Rule matching and action extraction tests
    test_prompt_lifecycle.c              # Instance lifecycle FSM tests
    test_prompt_detector_replay.c        # PTY replay and chunk-fuzz determinism tests
    test_pattern_config.c                # Config parsing tests
    test_prompt_protocol.c               # Prompt protocol CBOR tests
    test_pattern_routing.c               # Pattern routing tests

clients/
  cli/
    src/                                 # CLI client (C)
  ios/
    NabtoShell/
      App/
        NabtoShellApp.swift              # App entry point
        RootView.swift                   # Root navigation
        AppState.swift                   # App-wide state
      Models/
        DeviceBookmark.swift             # Device bookmark model
        PairingInfo.swift                # Pairing string parser
        SessionInfo.swift                # Session data model
      Patterns/
        PatternEngine.swift              # V2 server event state (instance_id based)
        PatternMatch.swift               # Prompt instance model
        PatternConfig.swift              # Config structs
        PatternConfigLoader.swift        # Bundle JSON loader
      Services/
        ConnectionManager.swift          # Control stream read/write
        CBORHelpers.swift                # CBOR encoding/decoding
        NabtoService.swift               # Nabto connection management
        BookmarkStore.swift              # Device bookmark persistence
        KeychainService.swift            # Keychain for private keys
        ReconnectLogic.swift             # Auto-reconnect on disconnect
        ResumeLogic.swift                # Session resume on app foreground
        AppLog.swift                     # Debug logging
      Views/
        DeviceListView.swift             # Device list and session selection
        PairingView.swift                # Pairing string input
        WelcomeView.swift                # First-launch welcome
        TerminalScreen.swift             # Main terminal view, pattern integration
        PatternOverlayView.swift         # Action button overlay
        TerminalViewWrapper.swift        # SwiftTerm UIKit bridge
        KeyboardAccessoryView.swift      # Terminal keyboard accessory bar
      Resources/
        patterns.json                    # Bundled pattern definitions

~/.nabtoshell/
  config/device.json                     # Product/device IDs
  state/iam_state.json                   # Paired users
  keys/device.key                        # Device private key
  patterns/*.json                        # Pattern definitions
```
