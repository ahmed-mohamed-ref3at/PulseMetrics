import threading
import time

import pytest

from pulsemetrics import ipc
from pulsemetrics.windows import lock_runtime


def test_named_pipe_round_trip_and_single_instance(tmp_path):
    server = ipc.Server(lambda c: {"id": c["id"], "ok": True, "result": c["args"]})
    stop = threading.Event()
    def serve():
        while not stop.is_set():
            server.once()
    thread = threading.Thread(target=serve)
    thread.start()
    try:
        for _ in range(3):
            assert ipc.request("status", {"text": "λ 🚀", "number": 12}, name=server.name) == {"text": "λ 🚀", "number": 12}
        with pytest.raises(OSError):
            ipc.create_pipe(server.name)
        lock = lock_runtime(tmp_path / "application.lock")
        try:
            with pytest.raises(RuntimeError):
                lock_runtime(tmp_path / "application.lock")
        finally:
            ipc.close(lock)
    finally:
        stop.set()
        thread.join(timeout=8)
        server.close()
    assert not thread.is_alive()
