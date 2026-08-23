from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import pytest

from tests.helpers.files import assert_same_file, payload


@pytest.mark.ntfs
def test_test_volume_handles_create_rename_move_and_delete(tmp_path: Path) -> None:
    source_dir = tmp_path / "source"
    target_dir = tmp_path / "target"
    source_dir.mkdir()
    target_dir.mkdir()
    source = source_dir / "initial.bin"
    source.write_bytes(payload(128))

    renamed = source.with_name("renamed.bin")
    source.rename(renamed)
    moved = target_dir / renamed.name
    renamed.replace(moved)

    assert moved.read_bytes() == payload(128)
    moved.unlink()
    target_dir.rmdir()
    source_dir.rmdir()


@pytest.mark.ntfs
@pytest.mark.unit
@pytest.mark.parametrize("name", [".mounted", "ordinary.mounted:stream"])
def test_mounted_control_files_are_rejected_by_file_layer(run_driver, tmp_path: Path, name: str) -> None:
    result = run_driver("file", "reject-mounted", tmp_path / name)
    assert result.stdout == "OK\n"


@pytest.mark.ntfs
@pytest.mark.unit
def test_ntfs_file_rejects_mounted_control_file(run_driver, tmp_path: Path) -> None:
    result = run_driver("ntfs", "reject-mounted", tmp_path / ".mounted")
    assert result.stdout == "OK\n"


@pytest.mark.ntfs
@pytest.mark.integration
def test_parallel_file_operations_keep_every_file_intact(run_driver, test_settings, tmp_path: Path) -> None:
    def roundtrip(index: int) -> Path:
        target = tmp_path / f"parallel-{index}.bin"
        run_driver("file", "roundtrip", target, f"payload-{index}")
        return target

    with ThreadPoolExecutor(max_workers=test_settings.worker_count) as pool:
        files = list(pool.map(roundtrip, range(test_settings.many_file_count)))

    for index, item in enumerate(files):
        expected = tmp_path / f"expected-{index}.bin"
        expected.write_bytes(f"payload-{index}".encode())
        assert_same_file(expected, item)
