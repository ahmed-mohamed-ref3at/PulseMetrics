import json
import os
import socket
import threading
import time
import uuid

import pytest
from streamlit.testing.v1 import AppTest

from pulsemetrics import ipc
from pulsemetrics.controller import Controller, ROOT, atomic_json
from pulsemetrics.windows import owns_listener
from test_collector import BIN, until


def config(directory):
    settings = json.loads((ROOT / "config/example.json").read_text())
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        settings["ui_port"] = s.getsockname()[1]
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("127.0.0.1", 0))
        settings["udp_port"] = s.getsockname()[1]
    settings.update(sample_seconds=1, window_seconds=1, refresh_seconds=1)
    atomic_json(directory / "config.json", settings)
    return settings


def test_controller_server_ready_without_browser_and_ui(tmp_path, monkeypatch):
    settings = config(tmp_path)
    controller = Controller(tmp_path, BIN)
    serving = None
    try:
        controller.native_start()
        url = controller.ui_start(open_browser=False)
        assert url.endswith(str(settings["ui_port"]))
        assert owns_listener(controller.ui.pid, settings["ui_port"])
        # No browser client has connected; server readiness has already succeeded.
        serving = threading.Thread(target=controller.serve)
        serving.start()
        health = ipc.request("status", name=controller.server.name)
        assert health["state"] == "running" and health["collector"]["state"] == "running"
        until(lambda: controller.snapshot and controller.snapshot["system"]["cpu_percent"] is not None)
        monkeypatch.setenv("PULSE_PIPE", controller.server.name)
        monkeypatch.setenv("PULSE_DATABASE", str(controller.database))
        app = AppTest.from_file(str(ROOT / "python/pulsemetrics/ui/app.py"), default_timeout=15).run()
        assert not app.exception, app.exception
        assert app.title[0].value == "Overview"
        assert len(app.metric) == 4
        explorer = AppTest.from_string("import streamlit as st\nfrom pulsemetrics.ui.pages import explorer\nst.set_page_config(layout='wide')\nexplorer()", default_timeout=15).run()
        assert not explorer.exception, explorer.exception
        explorer.text_input[0].input(str(os.getpid())).run()
        assert not explorer.exception, explorer.exception
        next(b for b in explorer.button if b.label == "Track process").click().run()
        until(lambda: any(r["pid"] == os.getpid() and r["tracked"] for r in controller.snapshot["processes"]))
        explorer.run()
        next(b for b in explorer.button if b.label == "Untrack process").click().run()
        until(lambda: any(r["pid"] == os.getpid() and not r["tracked"] for r in controller.snapshot["processes"]))
        rid = str(uuid.uuid4())
        first = ipc.request("save_settings", {"retention_days": 6}, name=controller.server.name, request_id=rid)
        assert ipc.request("save_settings", {"retention_days": 6}, name=controller.server.name, request_id=rid) == first
        with pytest.raises(RuntimeError, match="different content"):
            ipc.request("save_settings", {"retention_days": 5}, name=controller.server.name, request_id=rid)
        with pytest.raises(RuntimeError, match="at least"):
            ipc.request("save_settings", {"sample_seconds": 10, "window_seconds": 1}, name=controller.server.name)
        ipc.request("exit", name=controller.server.name)
        done = until(lambda: (s if (s := ipc.request("status", name=controller.server.name))["state"] == "stopped" else None), seconds=18)
        assert done["exit_result"]["clean"], done
        ipc.request("exit_result_seen", name=controller.server.name)
        serving.join(timeout=5)
        assert not serving.is_alive()
        assert json.loads((tmp_path / "last_shutdown.json").read_text())["clean"]
    finally:
        if controller.state == "running":
            controller.shutdown_native()
        controller.exit_seen = True
        if serving:
            serving.join(timeout=8)
        controller.close()


def test_ui_port_conflict_is_not_mistaken_for_readiness(tmp_path):
    settings = config(tmp_path)
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", settings["ui_port"]))
        listener.listen()
        controller = Controller(tmp_path, BIN)
        try:
            with pytest.raises(OSError):
                controller.ui_start(open_browser=False)
            assert controller.ui is None
        finally:
            controller.close()
