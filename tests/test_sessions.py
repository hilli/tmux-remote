"""Test listing tmux sessions on a device."""

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


def test_sessions_lists_known_session(paired_setup, cli_binary, client_home):
    """After creating a tmux session, 'sessions' should list it."""
    agent_proc, env = paired_setup

    session_name = "nabtoshell_test_sess"

    # Create a tmux session
    subprocess.run(
        ["tmux", "new-session", "-d", "-s", session_name],
        check=False,
    )
    time.sleep(1)

    try:
        result = run_cli(cli_binary, ["sessions", "default"],
                         env_override=env)
        output = result.stdout.decode("utf-8", errors="replace")

        assert result.returncode == 0, f"Sessions failed: {output}"
        assert session_name in output, \
            f"Session '{session_name}' not found in: {output}"
    finally:
        subprocess.run(["tmux", "kill-session", "-t", session_name],
                       check=False)


def test_sessions_empty_when_no_tmux(paired_setup, cli_binary, client_home):
    """When no tmux sessions exist, the output should indicate none found."""
    agent_proc, env = paired_setup

    # Kill all tmux sessions (be careful in CI)
    subprocess.run(["tmux", "kill-server"], check=False)
    time.sleep(1)

    result = run_cli(cli_binary, ["sessions", "default"], env_override=env)
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode == 0, f"Sessions failed: {output}"
    assert "no" in output.lower() or output.strip() == ""
