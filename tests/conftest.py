import json
import os
import pytest
import shutil

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
CONFIG_FILE = os.path.join(TESTS_DIR, "test_config.json")


def load_config():
    if not os.path.exists(CONFIG_FILE):
        pytest.skip("test_config.json not found. Run: make run-test-client-suite")
    with open(CONFIG_FILE) as f:
        return json.load(f)


@pytest.fixture(scope="session")
def config():
    return load_config()


@pytest.fixture(scope="session")
def agent_binary(config):
    path = config["agent_binary"]
    if not os.path.isabs(path):
        path = os.path.join(TESTS_DIR, path)
    path = os.path.abspath(path)
    if not os.path.exists(path):
        pytest.skip(f"Agent binary not found at {path}")
    return path


@pytest.fixture(scope="session")
def cli_binary(config):
    path = config["cli_binary"]
    if not os.path.isabs(path):
        path = os.path.join(TESTS_DIR, path)
    path = os.path.abspath(path)
    if not os.path.exists(path):
        pytest.skip(f"CLI binary not found at {path}")
    return path


@pytest.fixture(scope="session")
def product_id(config):
    return config["product_id"]


@pytest.fixture(scope="session")
def device_id(config):
    return config["device_id"]


@pytest.fixture
def agent_home(tmp_path):
    """A fresh temporary agent home directory for tests that need one."""
    home = str(tmp_path / "agent_home")
    os.makedirs(home, exist_ok=True)
    yield home
    shutil.rmtree(home, ignore_errors=True)
