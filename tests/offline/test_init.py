"""Test agent initialization (--init, file creation).

These tests exercise agent CLI flags only (no network needed).
"""

import os
import pytest
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from helpers import init_agent, parse_pairing_string


def test_init_creates_files(agent_binary, agent_home, product_id, device_id):
    """Agent --init creates the expected directory structure and files."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode == 0, f"Init failed: {output}"

    assert os.path.exists(os.path.join(agent_home, "config", "device.json"))
    assert os.path.exists(os.path.join(agent_home, "keys", "device.key"))
    assert os.path.exists(os.path.join(agent_home, "state", "iam_state.json"))


def test_init_prints_pairing_string(agent_binary, agent_home, product_id, device_id):
    """Agent --init prints a pairing string."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode == 0, f"Init failed: {output}"

    pairing_str = parse_pairing_string(output)
    assert pairing_str is not None, f"No pairing string found in: {output}"
    assert f"p={product_id}" in pairing_str
    assert f"d={device_id}" in pairing_str
    assert "pwd=" in pairing_str
    assert "sct=" in pairing_str


def test_init_refuses_reinit(agent_binary, agent_home, product_id, device_id):
    """Agent --init refuses to overwrite existing configuration."""
    result1 = init_agent(agent_binary, agent_home, product_id, device_id)
    assert result1.returncode == 0

    result2 = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result2.stdout.decode("utf-8", errors="replace")

    assert result2.returncode != 0 or "already exists" in output.lower()


def test_init_prints_fingerprint(agent_binary, agent_home, product_id, device_id):
    """Agent --init prints the device fingerprint."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode == 0
    assert "fingerprint" in output.lower(), f"No fingerprint in output: {output}"
