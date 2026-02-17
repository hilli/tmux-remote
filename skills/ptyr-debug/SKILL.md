---
name: ptyr-debug
description: Write replay-based regression tests and implement fixes for prompt-overlay bugs from PTY recordings captured with --record-pty. Use when requests ask to debug, analyze, or fix recording N using ./pty-log-N.ptyr with ./client-log.txt and ./device-log.txt, and the expected flow is fail-first reproduction test, debugging, code fix, and passing verification.
---

# Ptyr Debug

## Overview

Turn a PTY recording plus logs into an end-to-end fix. Add a replay regression test first, reproduce the failure, debug with the provided logs, implement the fix, and verify the new test passes.

## Required Inputs

- Recording number `N` from the user request.
- PTY recording file at `./pty-log-N.ptyr`.
- `./client-log.txt` and `./device-log.txt`.
- Scenario text from the user prompt.

If any required file is missing, stop and ask for it.

## Core Workflow

### 1. Normalize the scenario text

Read `references/prompt-normalization.md`.

- Keep the wording as close as possible to the user prompt.
- Correct spelling and obvious typos only.
- Preserve intent exactly.

### 2. Prepare the replay fixture

- Confirm `./pty-log-N.ptyr` exists.
- Copy it into `agent/tests/fixtures/pty-log-N.ptyr` if not already present.
- Reuse `agent/tests/test_prompt_detector_replay.c` as the primary harness.

### 3. Add a reproduction test

- Add a recording macro near the existing `RECORDING_PATH_*` constants.
- Add a new `START_TEST(...)` case that calls `load_recording` and `replay_frames`.
- Insert the normalized scenario as a comment in the test case.
- Add assertions that encode the bug report behavior.
- Register the test in `replay_suite()`.

Prefer test names of the form:

- `test_replay_recording_N_short_issue_name`

### 4. Enforce fail-first validation

Run the replay test suite and confirm the new test fails before patching code.

```bash
cd agent
cmake -B _build -G Ninja
cmake --build _build --target test_prompt_detector_replay
ctest --test-dir _build --output-on-failure -R prompt_detector_replay
```

If the test passes unexpectedly, tighten assertions until it fails for the reported scenario.

### 5. Debug using logs and replay signals

- Inspect `client-log.txt` and `device-log.txt` with focused searches.
- Compare log evidence with replay events and current assertions.
- Isolate the concrete mismatch, such as missing prompt event emission, malformed actions, or overlay state behavior.

### 6. Implement the fix

- Apply the smallest fix that addresses the identified root cause.
- Keep the new regression test unchanged unless the assertion is objectively wrong.
- Remove temporary debug instrumentation before final verification.

### 7. Verify success

- Re-run `prompt_detector_replay` until the new test passes.
- Run additional nearby tests when behavior overlaps.
- Report both states: initial failing assertion and final passing run.

## Output Requirements

- Include the normalized user scenario comment in the regression test.
- State where the bug lived, what changed, and why it fixes the issue.
- Confirm the new replay test reproduces the issue and passes after the fix.

## Guardrails

- Do not skip the fail-first step.
- Do not silently change scenario intent while correcting typos.
- Do not replace the replay harness with unrelated test infrastructure.
- Do not remove existing fixtures or tests unless explicitly requested.
