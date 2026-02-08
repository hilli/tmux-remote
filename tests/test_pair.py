"""Test client pairing flow."""

import os
import pytest
from helpers import (init_agent, start_agent, stop_agent, run_cli,
                     parse_pairing_string)


@pytest.fixture
def running_agent(agent_binary, agent_home, product_id, device_id):
    """Initialize and start agent, yield pairing string, then stop."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result.stdout.decode("utf-8", errors="replace")
    assert result.returncode == 0, f"Init failed: {output}"

    pairing_str = parse_pairing_string(output)
    assert pairing_str is not None, f"No pairing string in: {output}"

    proc = start_agent(agent_binary, agent_home)
    yield proc, pairing_str
    stop_agent(proc)


def test_pair_success(running_agent, cli_binary, client_home):
    """Pairing with a valid string succeeds."""
    proc, pairing_str = running_agent
    env = {"NABTOSHELL_HOME": client_home}

    result = run_cli(cli_binary, ["pair", pairing_str], env_override=env)
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode == 0, f"Pair failed: {output}"
    assert "paired" in output.lower() or "saved" in output.lower()


def test_pair_invalid_string(running_agent, cli_binary, client_home):
    """Pairing with a malformed string fails."""
    proc, _ = running_agent
    env = {"NABTOSHELL_HOME": client_home}

    result = run_cli(cli_binary, ["pair", "invalid-string"], env_override=env)
    assert result.returncode != 0


def test_pair_creates_devices_file(running_agent, cli_binary, client_home):
    """After pairing, devices.json is created."""
    proc, pairing_str = running_agent
    env = {"NABTOSHELL_HOME": client_home}

    result = run_cli(cli_binary, ["pair", pairing_str], env_override=env)
    assert result.returncode == 0

    devices_file = os.path.join(client_home, "devices.json")
    assert os.path.exists(devices_file), "devices.json not created after pairing"
