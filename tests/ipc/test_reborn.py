from pathlib import Path

import pytest


@pytest.mark.ipc
@pytest.mark.integration
def test_ipc_message_is_queued_and_processed(run_driver, tmp_path: Path) -> None:
    result = run_driver("ipc", "message", tmp_path / "clsync.sock", "SYNC")
    assert result.stdout == "OK\nOK"


@pytest.mark.ipc
@pytest.mark.unit
def test_ipc_rejects_an_empty_command(run_driver, tmp_path: Path) -> None:
    result = run_driver("ipc", "invalid", tmp_path / "clsync.sock")
    assert result.stdout == "OK\n"


@pytest.mark.ipc
@pytest.mark.integration
def test_ipc_queue_handles_more_than_the_legacy_ten_messages(run_driver, tmp_path: Path) -> None:
    result = run_driver("ipc", "many", tmp_path / "clsync.sock", "32")
    assert result.stdout == "OK\n"
