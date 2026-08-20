"""Configuration shared by all pytest groups.

Every value can be overridden in CI without editing test code.
"""

from dataclasses import dataclass
from os import getenv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _positive_int(name: str, default: int) -> int:
    value = int(getenv(name, default))
    if value <= 0:
        raise ValueError(f"{name} must be positive")
    return value


def _default_driver() -> Path | None:
    executable = "clsync_test_driver.exe" if __import__("os").name == "nt" else "clsync_test_driver"
    candidates = (
        ROOT / "build" / executable,
        ROOT / "build" / "Debug" / executable,
        ROOT / "build" / "Release" / executable,
    )
    return next((path for path in candidates if path.is_file()), None)


@dataclass(frozen=True)
class TestSettings:
    __test__ = False
    driver: Path | None
    csharp_service_url: str | None
    timeout_seconds: int
    retries: int
    worker_count: int
    small_file_size: int
    medium_file_size: int
    large_file_size: int
    many_file_count: int

    @classmethod
    def from_environment(cls) -> "TestSettings":
        configured_driver = getenv("CLSYNC_TEST_DRIVER")
        driver = Path(configured_driver).expanduser() if configured_driver else _default_driver()
        if driver and not driver.is_file():
            driver = None
        return cls(
            driver=driver,
            csharp_service_url=getenv("CLSYNC_CSHARP_SERVICE_URL"),
            timeout_seconds=_positive_int("CLSYNC_TEST_TIMEOUT", 10),
            retries=_positive_int("CLSYNC_TEST_RETRIES", 1),
            worker_count=_positive_int("CLSYNC_TEST_WORKERS", 4),
            small_file_size=_positive_int("CLSYNC_TEST_SMALL_FILE_SIZE", 1024),
            medium_file_size=_positive_int("CLSYNC_TEST_MEDIUM_FILE_SIZE", 64 * 1024),
            large_file_size=_positive_int("CLSYNC_TEST_LARGE_FILE_SIZE", 1024 * 1024),
            many_file_count=_positive_int("CLSYNC_TEST_MANY_FILE_COUNT", 10),
        )
