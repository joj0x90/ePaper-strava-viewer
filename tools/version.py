# PlatformIO pre-script: defines FIRMWARE_VERSION for all sources and libraries.
# CI sets the FIRMWARE_VERSION environment variable to the release tag; local builds use
# `git describe`, e.g. "v0.1.0" on a clean tag, "v0.1.0-3-gabc1234-dirty" otherwise.
import os
import subprocess

Import("env")

version = os.environ.get("FIRMWARE_VERSION")
if not version:
    try:
        version = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env["PROJECT_DIR"],
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        version = "dev"

print("Firmware version: " + version)
env.Append(CPPDEFINES=[("FIRMWARE_VERSION", env.StringifyMacro(version))])
