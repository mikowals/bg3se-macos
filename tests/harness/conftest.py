"""Shared fixtures for BG3SE harness pytest suite (Tier H).

All fixtures run without BG3 — they mock subprocess, socket, and filesystem.
"""

import socket
import threading
import time
from pathlib import Path
from types import SimpleNamespace

import pytest


@pytest.fixture
def fake_process():
    """Factory for a SimpleNamespace mimicking subprocess.Popen.

    Usage:
        proc = fake_process(returncode=42)
        assert proc.poll() == 42
    """
    def _make(returncode=None, pid=123):
        proc = SimpleNamespace(pid=pid, returncode=returncode)
        proc.poll = lambda: returncode
        return proc
    return _make


@pytest.fixture
def fake_socket_server(tmp_path):
    """Thread-based Unix domain socket server that sends scripted lines.

    Usage:
        sock_path, server = fake_socket_server(["v67\n"])
        # connect to sock_path, read lines
        server.join()
    """
    def _make(responses, sock_name="bg3se.sock"):
        sock_path = str(tmp_path / sock_name)

        def _serve():
            srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            srv.bind(sock_path)
            srv.listen(1)
            srv.settimeout(5)
            try:
                conn, _ = srv.accept()
                for line in responses:
                    conn.sendall(line.encode() if isinstance(line, str) else line)
                    time.sleep(0.05)
                conn.close()
            except socket.timeout:
                pass
            finally:
                srv.close()

        t = threading.Thread(target=_serve, daemon=True)
        t.start()
        time.sleep(0.1)
        return sock_path, t

    return _make
