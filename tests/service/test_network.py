from pathlib import Path

import pytest

from tests.helpers.files import assert_same_file, payload


@pytest.mark.service
@pytest.mark.integration
def test_http_get_uses_expected_user_agent(run_driver, service) -> None:
    result = run_driver("network", "get", f"{service.url}/health")
    assert result.stdout == "OK\nhealthy"
    assert service.requests[-1]["headers"]["User-Agent"] == "clsync/1.0"


@pytest.mark.service
@pytest.mark.integration
@pytest.mark.parametrize("method", ["post", "put"])
def test_http_methods_send_the_response_body(run_driver, service, method: str) -> None:
    result = run_driver("network", method, f"{service.url}/echo", "request-body")
    assert result.stdout == "OK\nrequest-body"
    assert service.requests[-1]["method"] == method.upper()


@pytest.mark.service
@pytest.mark.integration
def test_http_delete_removes_a_service_file(run_driver, service) -> None:
    service.files["obsolete"] = b"old"
    result = run_driver("network", "delete", f"{service.url}/files/obsolete")
    assert result.stdout == "OK\ndeleted"
    assert "obsolete" not in service.files


@pytest.mark.service
@pytest.mark.integration
def test_upload_and_download_preserve_checksum_and_user_agent(run_driver, service, test_settings, tmp_path: Path) -> None:
    source = tmp_path / "source.bin"
    destination = tmp_path / "destination.bin"
    source.write_bytes(payload(test_settings.medium_file_size))

    run_driver("network", "upload", f"{service.url}/upload", source)
    assert service.requests[-1]["headers"]["User-Agent"] == "clsync/1.0"
    run_driver("network", "download", f"{service.url}/files/uploaded", destination)

    assert_same_file(source, destination)
    assert service.requests[-1]["headers"]["User-Agent"] == "clsync/1.0"


@pytest.mark.service
def test_http_error_and_unavailable_service_are_failures(run_driver, service) -> None:
    run_driver("network", "get", f"{service.url}/files/missing", success=False)
    service.status = 503
    result = run_driver("network", "get", f"{service.url}/health", success=False)
    assert result.stdout.startswith("ERR\n")


@pytest.mark.service
def test_http_redirect_is_not_treated_as_a_successful_sync(run_driver, service) -> None:
    service.status = 302
    result = run_driver("network", "get", f"{service.url}/health", success=False)
    assert "Unexpected HTTP status: 302" in result.stdout


@pytest.mark.service
def test_timeout_is_reported(run_driver, service) -> None:
    service.delay_seconds = 0.2
    result = run_driver("network", "timeout-get", f"{service.url}/health", success=False)
    assert result.stdout.startswith("ERR\n")


@pytest.mark.service
@pytest.mark.e2e
def test_retry_recovers_after_one_temporary_failure(run_driver, service) -> None:
    service.planned_statuses = [503, 200]
    result = run_driver("network", "retry-get", f"{service.url}/health")
    assert result.stdout == "OK\nhealthy"
    assert len(service.requests) == 2


@pytest.mark.service
@pytest.mark.e2e
def test_partial_download_is_not_published(run_driver, service, tmp_path: Path) -> None:
    destination = tmp_path / "damaged.bin"
    run_driver("network", "download", f"{service.url}/partial", destination, success=False)
    assert not destination.exists()
    assert not destination.with_suffix(destination.suffix + ".part").exists()


@pytest.mark.service
def test_configured_csharp_service_is_reachable(run_driver, test_settings) -> None:
    if not test_settings.csharp_service_url:
        pytest.skip("set CLSYNC_CSHARP_SERVICE_URL to run against the real C# service")
    run_driver("network", "get", test_settings.csharp_service_url)
