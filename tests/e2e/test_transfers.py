from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest

from tests.helpers.files import assert_same_file, payload


@pytest.mark.e2e
def test_large_file_workflow_preserves_checksum(run_driver, service, test_settings, tmp_path: Path) -> None:
    source = tmp_path / "large.bin"
    destination = tmp_path / "large-output.bin"
    source.write_bytes(payload(test_settings.large_file_size))

    run_driver("workflow", tmp_path / "clsync.sock", source, service.url, destination)

    assert_same_file(source, destination)


@pytest.mark.e2e
def test_parallel_uploads_and_downloads_keep_every_result(run_driver, service, test_settings, tmp_path: Path) -> None:
    def transfer(index: int) -> tuple[Path, Path]:
        source = tmp_path / f"source-{index}.bin"
        destination = tmp_path / f"destination-{index}.bin"
        source.write_bytes(payload(test_settings.small_file_size + index))
        run_driver("network", "upload", f"{service.url}/upload", source)
        run_driver("network", "download", f"{service.url}/files/{source.name}", destination)
        return source, destination

    with ThreadPoolExecutor(max_workers=test_settings.worker_count) as pool:
        results = list(pool.map(transfer, range(test_settings.many_file_count)))

    for source, destination in results:
        assert_same_file(source, destination)
