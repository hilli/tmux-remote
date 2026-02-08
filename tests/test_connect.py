"""Test interactive terminal connection."""

import os
import pty
import select
import subprocess
import time
import pytest
from helpers import (init_agent, start_agent, stop_agent, run_cli,
                     parse_pairing_string)


@pytest.fixture
def paired_setup(agent_binary, cli_binary, agent_home, client_home,
                 product_id, device_id):
    """Initialize, start agent, pair client, yield components."""
    # Init agent
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result.stdout.decode("utf-8", errors="replace")
    assert result.returncode == 0, f"Init failed: {output}"

    pairing_str = parse_pairing_string(output)
    assert pairing_str is not None

    # Start agent
    proc = start_agent(agent_binary, agent_home)

    # Pair client
    env = {"NABTOSHELL_HOME": client_home}
    pair_result = run_cli(cli_binary, ["pair", pairing_str], env_override=env)
    assert pair_result.returncode == 0, \
        f"Pair failed: {pair_result.stdout.decode('utf-8', errors='replace')}"

    yield proc, env
    stop_agent(proc)


def test_connect_interactive(paired_setup, cli_binary, client_home):
    """Connect and send a command, verify output appears."""
    agent_proc, env = paired_setup

    # Use a PTY to simulate interactive terminal
    master_fd, slave_fd = pty.openpty()

    proc = subprocess.Popen(
        [cli_binary, "connect", "default"],
        stdin=slave_fd,
        stdout=slave_fd,
        stderr=slave_fd,
        env={**os.environ, **env},
    )

    # Give time to connect
    time.sleep(3)

    # Send a command
    os.write(master_fd, b"echo nabtoshell_test_marker\n")
    time.sleep(2)

    # Read output
    output = b""
    while True:
        ready, _, _ = select.select([master_fd], [], [], 0.5)
        if not ready:
            break
        chunk = os.read(master_fd, 4096)
        if not chunk:
            break
        output += chunk

    output_str = output.decode("utf-8", errors="replace")

    # Clean up
    os.write(master_fd, b"exit\n")
    time.sleep(1)

    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()

    os.close(master_fd)
    os.close(slave_fd)

    assert "nabtoshell_test_marker" in output_str, \
        f"Expected marker not found in output: {output_str}"
