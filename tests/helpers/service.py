"""A local HTTP service that mirrors the exercised C# service contract."""

from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock, Thread
from time import sleep
from typing import Any


class LocalService:
    def __init__(self) -> None:
        self.files: dict[str, bytes] = {}
        self.requests: list[dict[str, Any]] = []
        self.status = 200
        self.planned_statuses: list[int] = []
        self.delay_seconds = 0.0
        self.active_requests = 0
        self.max_active_requests = 0
        self._lock = Lock()
        self._server = ThreadingHTTPServer(("127.0.0.1", 0), self._handler())
        self._thread = Thread(target=self._server.serve_forever, daemon=True)

    @property
    def url(self) -> str:
        host, port = self._server.server_address
        return f"http://{host}:{port}"

    def start(self) -> None:
        self._thread.start()

    def close(self) -> None:
        self._server.shutdown()
        self._server.server_close()
        self._thread.join(timeout=2)

    def _next_status(self) -> int:
        with self._lock:
            if self.planned_statuses:
                return self.planned_statuses.pop(0)
            return self.status

    def _record(self, method: str, path: str, headers: Any, body: bytes) -> None:
        with self._lock:
            self.active_requests += 1
            self.max_active_requests = max(self.max_active_requests, self.active_requests)
            self.requests.append(
                {"method": method, "path": path, "headers": dict(headers), "body": body}
            )

    def _finish_request(self) -> None:
        with self._lock:
            self.active_requests -= 1

    def _handler(self) -> type[BaseHTTPRequestHandler]:
        service = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.0"

            def log_message(self, _format: str, *_args: object) -> None:
                pass

            def _body(self) -> bytes:
                return self.rfile.read(int(self.headers.get("Content-Length", 0)))

            def _respond(self, status: int, body: bytes = b"") -> None:
                self.send_response(status)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                try:
                    self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError):
                    pass

            def _request_status(self, body: bytes) -> int | None:
                service._record(self.command, self.path, self.headers, body)
                if service.delay_seconds:
                    sleep(service.delay_seconds)
                status = service._next_status()
                service._finish_request()
                if status != 200:
                    self._respond(status, f"status {status}".encode())
                    return status
                return None

            def do_GET(self) -> None:
                if self._request_status(b"") is not None:
                    return
                if self.path == "/health":
                    self._respond(200, b"healthy")
                elif self.path == "/partial":
                    body = b"incomplete"
                    self.send_response(200)
                    self.send_header("Content-Length", str(len(body) + 5))
                    self.end_headers()
                    self.wfile.write(body)
                elif self.path.startswith("/files/"):
                    name = self.path.removeprefix("/files/")
                    with service._lock:
                        data = service.files.get(name)
                    self._respond(200 if data is not None else 404, data or b"missing")
                else:
                    self._respond(404, b"missing")

            def do_POST(self) -> None:
                body = self._body()
                if self._request_status(body) is not None:
                    return
                if self.path == "/echo":
                    self._respond(200, body)
                elif self.path == "/upload":
                    data, filename = _multipart_file(self.headers.get("Content-Type", ""), body)
                    with service._lock:
                        service.files[filename or "uploaded"] = data
                        service.files["uploaded"] = data
                    self._respond(200, b"uploaded")
                else:
                    self._respond(404, b"missing")

            def do_PUT(self) -> None:
                body = self._body()
                if self._request_status(body) is None:
                    self._respond(200 if self.path == "/echo" else 404, body if self.path == "/echo" else b"missing")

            def do_DELETE(self) -> None:
                if self._request_status(b"") is not None:
                    return
                if self.path.startswith("/files/"):
                    with service._lock:
                        service.files.pop(self.path.removeprefix("/files/"), None)
                    self._respond(200, b"deleted")
                else:
                    self._respond(404, b"missing")

        return Handler


def _multipart_file(content_type: str, body: bytes) -> tuple[bytes, str | None]:
    """Extract the single multipart file emitted by libcurl without extra packages."""
    marker = "boundary="
    if marker not in content_type:
        return body, None
    boundary = content_type.split(marker, 1)[1].strip('"').encode()
    for part in body.split(b"--" + boundary):
        if b"\r\n\r\n" not in part:
            continue
        headers, value = part.split(b"\r\n\r\n", 1)
        if b'name="file"' not in headers:
            continue
        filename = None
        if b'filename="' in headers:
            filename = headers.split(b'filename="', 1)[1].split(b'"', 1)[0].decode(errors="replace")
        return value.rsplit(b"\r\n", 1)[0], filename
    return b"", None
