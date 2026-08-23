from pathlib import Path

import pytest

from tests.config.settings import TestSettings
from tests.helpers.files import assert_same_file, digest, payload


@pytest.mark.unit
@pytest.mark.parametrize("size", [1, 32, 4097])
def test_payload_is_deterministic_and_exact_size(size: int) -> None:
    data = payload(size)
    assert len(data) == size
    assert data == payload(size)
    assert digest(data) == digest(data)


@pytest.mark.unit
def test_file_integrity_compares_size_and_sha256(tmp_path: Path) -> None:
    source = tmp_path / "source.bin"
    target = tmp_path / "target.bin"
    source.write_bytes(payload(256))
    target.write_bytes(source.read_bytes())
    assert_same_file(source, target)


@pytest.mark.unit
def test_settings_are_read_from_environment(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    driver = tmp_path / "driver"
    driver.write_text("")
    monkeypatch.setenv("CLSYNC_TEST_DRIVER", str(driver))
    monkeypatch.setenv("CLSYNC_TEST_LARGE_FILE_SIZE", "2048")
    settings = TestSettings.from_environment()
    assert settings.driver == driver
    assert settings.large_file_size == 2048


@pytest.mark.unit
def test_file_roundtrip_uses_the_cpp_file_implementation(run_driver, tmp_path: Path) -> None:
    result = run_driver("file", "roundtrip", tmp_path / "roundtrip.bin", "test-payload")
    assert result.stdout == "OK\ntest-payload"


@pytest.mark.unit
def test_log_output_contains_component_status_and_message(run_driver) -> None:
    result = run_driver("log", "sync", "0", "operation started")
    assert " sync 0 operation started" in result.stdout
