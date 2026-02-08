# NabtoShell

Secure remote terminal access over Nabto Edge P2P connectivity. Exposes tmux sessions to authenticated clients without SSH, port forwarding, or firewall configuration.

## Prerequisites

- CMake 3.14+, Ninja (or Make)
- tmux installed on the agent machine
- Python 3 for integration tests

## Building the Agent

The Nabto Embedded SDK is fetched automatically via CMake FetchContent.

```bash
cd agent
cmake -B _build -G Ninja
cmake --build _build
```

## Building the CLI Client

The CLI client requires the Nabto Client SDK binary. You must download it before building.

### 1. Download the Nabto Client SDK

Download release v5.12.0 from https://github.com/nabto/nabto-client-sdk-releases/releases/tag/v5.12.0

Extract the platform libraries into the vendored directory:

```bash
# macOS (universal binary)
mkdir -p clients/cli/nabto_client_sdk_library/lib/macos-universal
cp <extracted>/lib/macos-universal/libnabto_client.dylib \
   clients/cli/nabto_client_sdk_library/lib/macos-universal/

# Linux x86_64
mkdir -p clients/cli/nabto_client_sdk_library/lib/linux-x86_64
cp <extracted>/lib/linux-x86_64/libnabto_client.so \
   clients/cli/nabto_client_sdk_library/lib/linux-x86_64/
```

The SDK headers are already checked in at `clients/cli/nabto_client_sdk_library/include/`.

### 2. Build

```bash
cd clients/cli
cmake -B _build -G Ninja
cmake --build _build
```

## Running Tests

```bash
cp tests/test_config.json.example tests/test_config.json
# Edit test_config.json with your Nabto Cloud Console device credentials
python3 -m pytest tests/ -v
```

## Architecture

See SPEC.md for full product specification and CLAUDE.md for development instructions.
