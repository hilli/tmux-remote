"""Test creating new tmux sessions via the CLI."""

import os
import subprocess
import time
import pytest
from helpers import (init_agent, start_agent, stop_agent, run_cli,
                     parse_pairing_string)


@pytest.fixture
def paired_setup(agent_binary, cli_binary, agent_home, client_home,
                 product_id, device_id):
    """Initialize, start agent, pair client."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result.stdout.decode("utf-8", errors="replace")
    assert result.returncode == 0

    pairing_str = parse_pairing_string(output)
    assert pairing_str is not None

    proc = start_agent(agent_binary, agent_home)

    env = {"NABTOSHELL_HOME": client_home}
    pair_result = run_cli(cli_binary, ["pair", pairing_str], env_override=env)
    assert pair_result.returncode == 0

    yield proc, env
    stop_agent(proc)


def test_create_session_invalid_name(paired_setup, cli_binary, client_home):
    """Creating a session with invalid characters should fail."""
    agent_proc, env = paired_setup

    # Session names with special characters should be rejected by the agent
    # The CLI should propagate the error
    result = run_cli(
        cli_binary,
        ["connect", "default", "-s", "bad;name", "--new", "bash"],
        env_override=env,
        timeout=10,
    )
    output = result.stdout.decode("utf-8", errors="replace")

    # Should fail due to invalid session name
    assert result.returncode != 0 or "fail" in output.lower() or \
        "invalid" in output.lower() or "error" in output.lower()
