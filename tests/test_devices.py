"""Test device bookmark listing."""

import os
import pytest
from helpers import (init_agent, start_agent, stop_agent, run_cli,
                     parse_pairing_string)


def test_devices_empty(cli_binary, client_home):
    """With no paired devices, 'devices' shows empty list."""
    env = {"NABTOSHELL_HOME": client_home}
    result = run_cli(cli_binary, ["devices"], env_override=env)
    output = result.stdout.decode("utf-8", errors="replace")

    assert result.returncode == 0, f"Devices failed: {output}"
    assert "no saved" in output.lower() or "no device" in output.lower()


def test_devices_after_pairing(agent_binary, cli_binary, agent_home,
                                client_home, product_id, device_id):
    """After pairing, 'devices' lists the paired device."""
    result = init_agent(agent_binary, agent_home, product_id, device_id)
    output = result.stdout.decode("utf-8", errors="replace")
    assert result.returncode == 0

    pairing_str = parse_pairing_string(output)
    assert pairing_str is not None

    proc = start_agent(agent_binary, agent_home)

    try:
        env = {"NABTOSHELL_HOME": client_home}
        pair_result = run_cli(cli_binary, ["pair", pairing_str],
                              env_override=env)
        assert pair_result.returncode == 0

        dev_result = run_cli(cli_binary, ["devices"], env_override=env)
        dev_output = dev_result.stdout.decode("utf-8", errors="replace")

        assert dev_result.returncode == 0
        assert device_id in dev_output or product_id in dev_output, \
            f"Device not found in output: {dev_output}"
    finally:
        stop_agent(proc)
