from datetime import datetime, timezone
import html
import os
import time

import plotly.graph_objects as go
import streamlit as st

from pulsemetrics import ipc
from pulsemetrics.data import Data

PURPLE, CYAN = "#A58AFF", "#53D7E5"


def database():
    path = os.environ.get("PULSE_DATABASE")
    if not path:
        raise RuntimeError("Start the application with scripts/launch.py")
    return Data(path)


def utc(microseconds):
    return datetime.fromtimestamp(microseconds / 1e6, timezone.utc) if microseconds else None


def timestamp(microseconds):
    return utc(microseconds).strftime("%d %b %Y · %H:%M:%S UTC") if microseconds else "Not yet observed"


def title(eyebrow, heading, description):
    st.html(f'<div class="pm-eyebrow">{html.escape(eyebrow)}</div>')
    st.title(heading)
    st.caption(description)


def status():
    state = ipc.request("status")
    if state["state"] != "running":
        if state["state"] == "stopped":
            result = state.get("exit_result") or {}
            if result.get("clean"):
                st.success("PulseMetrics has stopped. Your committed history is preserved. You can close this tab.")
            else:
                st.error("Shutdown completed with uncommitted data or component errors. Inspect last_shutdown.json before restarting.")
                st.json(result)
            ipc.request("exit_result_seen")
        else:
            st.info("Stopping monitoring and committing the final window…")
        st.stop()
    live = state["live"]
    stale = live.get("age_seconds") is None or live["age_seconds"] > max(15, state["settings"]["sample_seconds"] * 3)
    if stale:
        st.warning("Live measurements are unavailable or stale. Historical records below remain real stored observations.")
    if state.get("monitor_error"):
        st.warning(state["monitor_error"])
    health = state["collector"]
    if health.get("clock_warning"):
        st.warning("The system clock changed. Sessions retain their original timestamps; compare within one session. Retention is paused for this session.")
    if health.get("write_healthy") is False or health.get("state") == "unavailable":
        st.error("Collector persistence is unavailable: " + health.get("last_error", "unknown failure"))
    return state, stale


def value(number, unit="", precision=1):
    return "Unavailable" if number is None else f"{number:,.{precision}f}{unit}"


def chart(rows, name, label, unit, scale=1, field="last_value", key=None):
    selected = [row for row in rows if row["name"] == name]
    if not selected:
        st.info(f"No stored {label.lower()} observations in this range.")
        return None
    figure = go.Figure()
    sessions = sorted({row["session_order"] for row in selected})
    for index, session in enumerate(sessions):
        ordered = sorted((r for r in selected if r["session_order"] == session), key=lambda r: r["sequence"])
        x, y, custom = [], [], []
        previous = None
        for row in ordered:
            if previous and (row["sequence"] != previous["sequence"] + 1 or row["start_utc_us"] - previous["end_utc_us"] > 100000):
                x.append(utc(row["start_utc_us"])); y.append(None); custom.append(None)
            x.append(utc(row["end_utc_us"])); y.append(row[field] / scale); custom.append(row["window_id"])
            previous = row
        figure.add_trace(go.Scatter(x=x, y=y, customdata=custom, mode="lines+markers", connectgaps=False,
                                   name=f"Session {session}", line=dict(color=PURPLE if index % 2 == 0 else CYAN, width=2),
                                   marker=dict(size=4), hovertemplate=f"%{{x|%d %b %H:%M:%S UTC}}<br>%{{y:.2f}} {unit}<br>Window %{{customdata}}<extra></extra>"))
    figure.update_layout(height=270, margin=dict(l=8, r=12, t=12, b=8), paper_bgcolor="rgba(0,0,0,0)",
                         plot_bgcolor="rgba(0,0,0,0)", font=dict(color="#D4DEF4", family="Segoe UI, sans-serif"),
                         showlegend=len(sessions) > 1, legend=dict(orientation="h"),
                         xaxis=dict(title="Window end · UTC", gridcolor="#202C45"),
                         yaxis=dict(title=unit, gridcolor="#202C45", rangemode="tozero"),
                         hovermode="x unified", uirevision=key or name)
    return st.plotly_chart(figure, key=key, config={"displaylogo": False}, on_select="rerun", selection_mode="points")


def alert_list(rows, maximum=4):
    if not rows:
        st.caption("No threshold events recorded. Add a rule from Alerts to evaluate future collected windows.")
    for row in rows[:maximum]:
        state = "Open · awaiting recovery" if row["closed_utc_us"] is None else "Closed · " + row["close_reason"].replace("_", " ")
        st.markdown(f"**{html.escape(row['label'])}**")
        st.caption(f"{state} · {row['display_name']} · {row['name']} · {row['last_value']:.2f} {row['unit']} · {timestamp(row['last_evaluated_utc_us'])}")


def recent_range(seconds=3600):
    end = int(time.time() * 1e6)
    return end - seconds * 1000000, end
