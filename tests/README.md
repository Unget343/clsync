# clsync tests

The suite uses `pytest` and a C++ test driver built from the actual `File`, `NtfsFile`, `Reborn`, and `Network` classes. The HTTP service is an isolated local compatibility stand; no C# service, network share, or permanent files are needed for the regular suite.

```powershell
py -m pip install -r requirements-dev.txt
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Debug
py -m pytest
```

Run a group with `py -m pytest -m unit`, `ipc`, `ntfs`, `service`, `integration`, or `e2e`. Generate a CI report with `py -m pytest --junitxml=test-results.xml`.

## Configuration

`CLSYNC_TEST_DRIVER` points to a manually built test driver. The default locations are `build/clsync_test_driver` and the usual Visual Studio `Debug`/`Release` folders. `CLSYNC_CSHARP_SERVICE_URL` enables the optional real-service reachability check. File sizes, file count, worker count, and timeouts are configurable with the `CLSYNC_TEST_*` variables declared in `tests/config/settings.py`.

`tests/integration/test_sync_manager.py` covers the production sync manager: safe paths, SHA-256 validation, retries, queue parallelism, upload, and the IPC command format documented in [`net/SYNC_PROTOCOL.md`](../net/SYNC_PROTOCOL.md).
