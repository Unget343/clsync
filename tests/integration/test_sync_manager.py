from pathlib import Path

import pytest

from tests.helpers.files import assert_same_file, digest, payload


@pytest.mark.integration
@pytest.mark.service
def test_sync_manager_downloads_to_safe_path_and_validates_sha256(run_driver, service, tmp_path: Path) -> None:
    source = tmp_path / "source.bin"
    source.write_bytes(payload(8192))
    service.files["source.bin"] = source.read_bytes()
    root = tmp_path / "sync-root"
    destination = root / "nested" / "source.bin"

    result = run_driver(
        "sync", "download", root, f"{service.url}/health", f"{service.url}/files/source.bin", "nested/source.bin",
        source.stat().st_size, digest(source),
    )

    assert "task:attempts=1" in result.stdout
    assert_same_file(source, destination)


@pytest.mark.integration
@pytest.mark.service
def test_sync_manager_rejects_path_traversal_before_network_access(run_driver, service, tmp_path: Path) -> None:
    root = tmp_path / "sync-root"
    result = run_driver("sync", "reject-path", root, f"{service.url}/health", f"{service.url}/files/missing")
    assert "\nOK\n" in result.stdout
    assert not (tmp_path / "escape.bin").exists()
    assert all(request["path"] == "/health" for request in service.requests)


@pytest.mark.integration
@pytest.mark.service
def test_sync_manager_discards_checksum_mismatch_without_publishing_output(run_driver, service, tmp_path: Path) -> None:
    service.files["bad.bin"] = payload(1024)
    root = tmp_path / "sync-root"
    destination = root / "bad.bin"

    run_driver(
        "sync", "download", root, f"{service.url}/health", f"{service.url}/files/bad.bin", "bad.bin",
        1024, "0" * 64, success=False,
    )

    assert not destination.exists()
    assert not destination.with_suffix(".bin.part").exists()


@pytest.mark.integration
@pytest.mark.service
def test_sync_manager_retries_after_temporary_service_failure(run_driver, service, tmp_path: Path) -> None:
    source = tmp_path / "retry.bin"
    source.write_bytes(payload(256))
    service.files["retry.bin"] = source.read_bytes()
    service.planned_statuses = [200, 503, 200]

    result = run_driver(
        "sync", "download", tmp_path / "sync-root", f"{service.url}/health", f"{service.url}/files/retry.bin",
        "retry.bin", source.stat().st_size, digest(source),
    )

    assert "task:attempts=2" in result.stdout


@pytest.mark.integration
@pytest.mark.e2e
def test_sync_manager_dynamically_processes_multiple_files_in_parallel(run_driver, service, tmp_path: Path) -> None:
    service.delay_seconds = 0.1
    arguments: list[str] = []
    for index in range(4):
        name = f"file-{index}.bin"
        service.files[name] = payload(256 + index)
        arguments.extend((f"{service.url}/files/{name}", name))

    run_driver("sync", "batch-download", tmp_path / "sync-root", f"{service.url}/health", *arguments)

    assert service.max_active_requests >= 2
    for index in range(4):
        assert (tmp_path / "sync-root" / f"file-{index}.bin").read_bytes() == payload(256 + index)


@pytest.mark.integration
@pytest.mark.e2e
def test_sync_manager_upload_and_ipc_command_use_existing_network_and_reborn_contract(run_driver, service, tmp_path: Path) -> None:
    root = tmp_path / "sync-root"
    root.mkdir()
    upload = root / "upload.bin"
    upload.write_bytes(payload(512))
    run_driver("sync", "upload", root, f"{service.url}/health", f"{service.url}/upload", "upload.bin")
    assert service.files["upload.bin"] == upload.read_bytes()

    service.files["ipc.bin"] = payload(512)
    command = f"DOWNLOAD ipc {service.url}/files/ipc.bin ipc.bin 512 {digest(payload(512))}"
    run_driver("sync", "ipc", root, f"{service.url}/health", command)
    assert (root / "ipc.bin").read_bytes() == payload(512)
