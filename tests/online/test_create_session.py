"""Test creating new tmux sessions via the CLI.

Runs against the persistent test server started with 'make run-test-server'.
Uses a pre-paired client from 'make setup-test-client'.
"""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from helpers import run_cli


def test_create_session_invalid_name(paired_env, cli_binary):
    """Creating a session with invalid characters should fail."""
    env = paired_env
    result = run_cli(
        cli_binary,
        ["new-session", "default", "bad;name"],
        env_override=env,
        timeout=10,
    )
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode != 0 or "fail" in output.lower() or \
        "invalid" in output.lower() or "error" in output.lower()
