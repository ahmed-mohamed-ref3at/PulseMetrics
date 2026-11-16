import os
import sqlite3

import streamlit as st

from pulsemetrics import ipc
from pulsemetrics.ui.components import database
from pulsemetrics.ui import pages

st.set_page_config(page_title="PulseMetrics", page_icon=":material/monitoring:", layout="wide")
st.html("""<style>
.pm-eyebrow { color:#53D7E5; letter-spacing:.16em; font-size:.72rem; font-weight:700; margin-top:.3rem; }
.pm-brand { font-size:1.7rem; font-weight:750; letter-spacing:-.06em; color:#EAF0FF; }
.pm-brand span { color:#A58AFF; }
.pm-tagline { color:#9CAFCF; font-size:.78rem; margin:0 0 1.8rem; }
</style>""")
with st.sidebar:
    st.html('<div class="pm-brand">Pulse<span>Metrics</span></div><div class="pm-tagline">PERFORMANCE, IN PERSPECTIVE.</div>')


@st.dialog("Settings")
def settings():
    current = ipc.request("status")
    with st.form("settings_form"):
        fields = {}
        for key, label, high in [("sample_seconds", "Sampling interval · seconds", 60), ("window_seconds", "Aggregation window · seconds", 60), ("refresh_seconds", "Dashboard refresh · seconds", 60), ("retention_days", "History retention · days", 30)]:
            fields[key] = st.number_input(label, min_value=1, max_value=high, value=current["settings"][key], step=1)
        st.caption("Collection and retention changes apply on the next launch. Refresh changes apply immediately.")
        if st.form_submit_button("Save settings", type="primary"):
            try:
                ipc.request("save_settings", fields)
                st.session_state["refresh"] = fields["refresh_seconds"]
                st.rerun()
            except (RuntimeError, OSError, TimeoutError) as exc:
                st.error(str(exc))


@st.dialog("Collector & database health", width="large")
def health():
    current = ipc.request("status")
    st.caption("Accepted observations are in memory until a successful database commit. UDP loss before receipt cannot be measured.")
    st.json(current["collector"])
    st.write("Stored catalog and history", database().counts())
    if current["restart_required"]:
        st.info("Saved collection settings will apply after restart.")


@st.dialog("Exit PulseMetrics")
def exit_dialog():
    st.write("Stop native monitoring, commit the final window, and close the local application server.")
    st.caption("Closing a browser tab alone leaves collection running.")
    if st.button("Exit PulseMetrics", type="primary"):
        ipc.request("exit")
        st.rerun()


refresh = st.session_state.get("refresh", int(os.environ.get("PULSE_REFRESH", "5")))


def render(function):
    @st.fragment(run_every=refresh)
    def content():
        try:
            function()
        except (sqlite3.Error, RuntimeError, OSError, TimeoutError, ValueError) as exc:
            st.error(f"This view is unavailable: {exc}")
            st.caption("No substitute measurements are shown. Retry after checking collector health or restart the launcher.")
    content()


overview = st.Page(lambda: render(pages.overview), title="Overview", icon=":material/space_dashboard:", default=True, url_path="overview")
explorer = st.Page(lambda: render(pages.explorer), title="Process Explorer", icon=":material/memory:", url_path="processes")
navigation = st.navigation([overview, explorer])
with st.sidebar:
    st.divider()
    if st.button("Settings", icon=":material/settings:", width="stretch"):
        settings()
    if st.button("Collector Health", icon=":material/monitor_heart:", width="stretch"):
        health()
    if st.button("Exit PulseMetrics", icon=":material/power_settings_new:", width="stretch"):
        exit_dialog()
    st.caption("LOCAL BY DESIGN\n\nNative C collection · SQLite history\n\nPulseMetrics 2.0")
navigation.run()
