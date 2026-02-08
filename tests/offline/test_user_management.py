"""Test user management (--add-user, --remove-user).

These tests exercise agent CLI flags only (no network needed).
"""

import pytest
import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from helpers import init_agent, add_user, remove_user, parse_pairing_string


def test_add_user_success(agent_binary, agent_home, product_id, device_id):
    """Adding a user prints a pairing string."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    assert result.returncode == 0

    result = add_user(agent_binary, agent_home, "tablet")
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode == 0, f"Add user failed: {output}"

    pairing_str = parse_pairing_string(output)
    assert pairing_str is not None, f"No pairing string in: {output}"
    assert "u=tablet" in pairing_str


def test_add_user_duplicate(agent_binary, agent_home, product_id, device_id):
    """Adding the same user twice should fail."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    assert result.returncode == 0

    result1 = add_user(agent_binary, agent_home, "tablet")
    assert result1.returncode == 0

    result2 = add_user(agent_binary, agent_home, "tablet")
    output = result2.stdout.decode("utf-8", errors="replace")

    assert result2.returncode != 0 or "already exists" in output.lower()


def test_remove_user_success(agent_binary, agent_home, product_id, device_id):
    """Removing a user should succeed."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    assert result.returncode == 0

    add_result = add_user(agent_binary, agent_home, "tablet")
    assert add_result.returncode == 0

    rm_result = remove_user(agent_binary, agent_home, "tablet")
    output = rm_result.stdout.decode("utf-8", errors="replace")

    assert rm_result.returncode == 0, f"Remove user failed: {output}"
    assert "removed" in output.lower()


def test_remove_nonexistent_user(agent_binary, agent_home, product_id,
                                  device_id):
    """Removing a user that doesn't exist should fail."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    assert result.returncode == 0

    rm_result = remove_user(agent_binary, agent_home, "nonexistent")
    output = rm_result.stdout.decode("utf-8", errors="replace")

    assert rm_result.returncode != 0 or "not found" in output.lower()
