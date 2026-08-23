from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from subprocess import CompletedProcess, run

import pytest

from tests.config.settings import TestSettings
from tests.helpers.service import LocalService


@pytest.fixture(scope="session")
def test_settings() -> TestSettings:
    return TestSettings.from_environment()


@pytest.fixture(scope="session")
def test_driver(test_settings: TestSettings) -> Path:
    if test_settings.driver is None:
        pytest.skip(
            "C++ test driver not found; build with 'cmake -S . -B build -DBUILD_TESTING=ON' "
            "or set CLSYNC_TEST_DRIVER"
        )
    return test_settings.driver


@pytest.fixture
def run_driver(test_driver: Path, test_settings: TestSettings) -> Callable[..., CompletedProcess[str]]:
    def _run(*args: str, success: bool = True, timeout: int | None = None) -> CompletedProcess[str]:
        result = run(
            [str(test_driver), *map(str, args)],
            capture_output=True,
            text=True,
            timeout=timeout or test_settings.timeout_seconds,
            check=False,
        )
        if (result.returncode == 0) != success:
            pytest.fail(
                f"driver {' '.join(map(str, args))} exited {result.returncode}\n"
                f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
            )
        return result

    return _run


@pytest.fixture
def service() -> LocalService:
    instance = LocalService()
    instance.start()
    yield instance
    instance.close()
