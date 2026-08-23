"""Deterministic test-data and integrity helpers."""

from hashlib import sha256
from pathlib import Path


def payload(size: int) -> bytes:
    """Return repeatable binary data of exactly *size* bytes."""
    block = sha256(b"clsync-test-data").digest()
    return (block * (size // len(block) + 1))[:size]


def digest(value: bytes | Path) -> str:
    data = value.read_bytes() if isinstance(value, Path) else value
    return sha256(data).hexdigest()


def assert_same_file(expected: Path, actual: Path) -> None:
    assert actual.is_file(), f"missing output file: {actual}"
    assert actual.stat().st_size == expected.stat().st_size
    assert digest(actual) == digest(expected)
