"""Test that unauthorized access is rejected."""

import os
import pytest
from helpers import (init_agent, start_agent, stop_agent, run_cli,
                     parse_pairing_string)


def test_connect_without_pairing(agent_binary, cli_binary, agent_home,
                                  client_home, product_id, device_id):
    """Connecting without pairing should fail."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    assert result.returncode == 0

    proc = start_agent(agent_binary, agent_home)

    try:
        env = {"NABTOSHELL_HOME": client_home}
        # Try to connect without pairing first
        result = run_cli(cli_binary, ["connect", "nonexistent"],
                         env_override=env, timeout=10)
        output = result.stdout.decode("utf-8", errors="replace")

        # Should fail because device is not in bookmarks
        assert result.returncode != 0
    finally:
        stop_agent(proc)


def test_sessions_without_pairing(agent_binary, cli_binary, agent_home,
                                   client_home, product_id, device_id):
    """Listing sessions without pairing should fail."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    assert result.returncode == 0

    proc = start_agent(agent_binary, agent_home)

    try:
        env = {"NABTOSHELL_HOME": client_home}
        result = run_cli(cli_binary, ["sessions", "nonexistent"],
                         env_override=env, timeout=10)

        assert result.returncode != 0
    finally:
        stop_agent(proc)
