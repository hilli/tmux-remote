"""Test terminal resize functionality.

Runs against the persistent test server started with 'make run-test-server'.
Uses a pre-paired client from 'make setup-test-client'.
"""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from helpers import run_cli


def test_sessions_reachable_after_pairing(paired_env, cli_binary):
    """Verify the sessions endpoint is reachable (resize infra smoke test)."""
    env = paired_env
    result = run_cli(cli_binary, ["sessions", "default"], env_override=env)
    assert result.returncode == 0, \
        f"Sessions failed: {result.stdout.decode('utf-8', errors='replace')}"
