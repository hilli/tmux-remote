import os
import signal
import subprocess
import time


def start_agent(agent_binary, home_dir, product_id=None, device_id=None,
                log_level="error", extra_args=None):
    """Start the agent process in the background."""
    cmd = [agent_binary, "--home-dir", home_dir, "--log-level", log_level,
           "--random-ports"]
    if extra_args:
        cmd.extend(extra_args)

    env = os.environ.copy()
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
    )
    # Give agent time to start and attach
    time.sleep(3)
    if proc.poll() is not None:
        out = proc.stdout.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"Agent exited early with code {proc.returncode}: {out}")
    return proc


def stop_agent(proc, timeout=5):
    """Stop the agent process gracefully."""
    if proc.poll() is not None:
        return proc.returncode
    proc.send_signal(signal.SIGINT)
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    return proc.returncode


def run_cli(cli_binary, args, timeout=30, env_override=None, input_data=None):
    """Run the CLI client with given arguments and return result."""
    cmd = [cli_binary] + args
    env = os.environ.copy()
    if env_override:
        env.update(env_override)

    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        env=env,
        input=input_data,
    )
    return result


def init_agent(agent_binary, home_dir, product_id, device_id):
    """Initialize agent configuration."""
    result = subprocess.run(
        [agent_binary, "--home-dir", home_dir, "--init",
         "--product-id", product_id, "--device-id", device_id],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=10,
    )
    return result


def add_user(agent_binary, home_dir, username):
    """Add a user to the agent."""
    result = subprocess.run(
        [agent_binary, "--home-dir", home_dir, "--add-user", username],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=10,
    )
    return result


def remove_user(agent_binary, home_dir, username):
    """Remove a user from the agent."""
    result = subprocess.run(
        [agent_binary, "--home-dir", home_dir, "--remove-user", username],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=10,
    )
    return result


def parse_pairing_string(output):
    """Extract a pairing string from agent output."""
    for line in output.split("\n"):
        line = line.strip()
        if line.startswith("p=") and "sct=" in line:
            return line
        # Also check for lines with leading comment/banner markers
        stripped = line.lstrip("#").strip()
        if stripped.startswith("p=") and "sct=" in stripped:
            return stripped
    return None


def wait_for_output(proc, pattern, timeout=10):
    """Wait for a pattern to appear in process output."""
    import select
    import re

    start = time.time()
    output = ""
    while time.time() - start < timeout:
        if proc.stdout.readable():
            line = proc.stdout.readline()
            if line:
                decoded = line.decode("utf-8", errors="replace")
                output += decoded
                if re.search(pattern, decoded):
                    return True, output
        time.sleep(0.1)
    return False, output
