from pathlib import Path

import pytest

from tests.helpers.files import assert_same_file, payload


@pytest.mark.integration
@pytest.mark.e2e
def test_ipc_file_network_workflow_preserves_content(run_driver, service, tmp_path: Path) -> None:
    source = tmp_path / "source.bin"
    destination = tmp_path / "destination.bin"
    source.write_bytes(payload(8192))

    result = run_driver("workflow", tmp_path / "clsync.sock", source, service.url, destination)

    assert result.stdout == "OK\nSuccess"
    assert_same_file(source, destination)
