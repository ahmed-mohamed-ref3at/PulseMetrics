# Phase 0 setup and blocker

No functional build environment is currently available to this execution session. The earlier official MSYS2 acquisition request failed with WinError10013 (socket access forbidden). Current local checks found no UCRT64 compiler/CMake/CTest/Ninja or complete official offline package set. The same blocked download was not retried.

## Minimal manual setup

Use one official MSYS2 x86_64 UCRT64 installation. Do not mix MSVC, MSYS, MINGW64 and UCRT64 libraries. V2 requires only these direct native packages:

```text
mingw-w64-ucrt-x86_64-gcc
mingw-w64-ucrt-x86_64-cmake
mingw-w64-ucrt-x86_64-ninja
```

GCC supplies its transitive binutils/windres/headers/runtime dependencies; CMake supplies CTest. GTK, Cairo and pkgconf are not needed by the planned V2 backend build. SQLite/cJSON will be vendored from the received source with their notices; do not install a separate database server.

1. From a manually network-enabled environment, obtain a versioned x86_64 MSYS2 **base archive**, its official checksum/signature evidence, and official package metadata. Official distribution location: `https://repo.msys2.org/distrib/x86_64/`. Verify the chosen archive before executing it. No release filename or current version was verified by this session.
2. Extract the isolated root beneath the already approved location:

   `C:\PulseMetricsWorkspace\AUDIT_REPORTS\FOLLOW_UP\toolchain\msys64`

   Do not overwrite existing audit evidence or modify the host `E:/` installation. This approved parent contains spaces; if MSYS2 bootstrap rejects that path, stop and obtain approval for a shorter isolated location. Do not silently install elsewhere, create persistent drive mappings, alter system PATH or elevate.
3. Open **that root's** UCRT64 shell (`ucrt64.exe`). Confirm it is the intended root and `MSYSTEM=UCRT64`. In that disposable shell:

```bash
printf '%s\n' "$MSYSTEM"
cygpath -w /
pacman -Syu
# If requested, close/reopen this disposable shell and finish pacman -Syu.
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
pacman -Q > resolved-packages.txt
gcc -dumpmachine
gcc --version
windres --version
cmake --version
ctest --version
ninja --version
command -v gcc cmake ctest ninja windres
```

Keep package signature validation enabled and use official MSYS2 sources only. Store the resolved manifest, downloaded official packages and their hashes in the isolated toolchain area, not the source repository. Expected compiler target is `x86_64-w64-mingw32`; also confirm the paths point into this root's `ucrt64/bin` because that target string alone does not identify the runtime variant.

If internet acquisition is unavailable, supply the verified base archive plus the official signed **complete transitive package closure** for those three packages. A compiler executable alone or the old project's runtime DLLs is insufficient. Once those artifacts are available, installation can use the approved root and official metadata; do not disable signature checks to force cached packages through.

## Process-only build environment

After installation is actually verified, a PowerShell session may use:

```powershell
$PulseToolchain = 'C:\PulseMetricsWorkspace\AUDIT_REPORTS\FOLLOW_UP\toolchain\msys64'
$env:PATH = "$PulseToolchain\ucrt64\bin;$env:PATH"
gcc --version
cmake --version
ctest --version
ninja --version
```

This changes only that session, not persistent PATH. Native source preparation is gated until these tools work. The future build directory should be a sibling such as `../PulseMetrics_V2_BUILD/debug`, outside the distributable source repository. After the backend CMake files have been created and reviewed, the intended commands are:

```powershell
cmake -S . -B ../PulseMetrics_V2_BUILD/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ../PulseMetrics_V2_BUILD/debug --parallel 2
ctest --test-dir ../PulseMetrics_V2_BUILD/debug --output-on-failure
```

These are **future commands, not currently runnable project instructions**: this blocked foundation contains no CMake graph or production code yet. Do not reuse a received cache or executable. Debug assertions must remain enabled. Later Release smoke tests are additional evidence, not replacements for Debug tests.

Python environment and pinned Streamlit/Plotly/pytest dependencies follow the native prerequisite; no packages have been installed here. Application runtime data will live under `%LOCALAPPDATA%/PulseMetricsV2/`, separate from source, release staging and build products. Phase 0 remains blocked until fresh toolchain/build evidence exists.
