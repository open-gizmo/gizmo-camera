# Give the firmware its version from the VERSION file of the repository (semantic versioning), so every build reports it in /api/device, released or flashed by hand
Import("env")  # noqa: F821 (PlatformIO runs this script with SCons)
import os

version_path = os.path.join(env.subst("$PROJECT_DIR"), "..", "VERSION")  # noqa: F821
with open(version_path) as version_file:
    version = "v" + version_file.read().strip()
env.Append(CPPDEFINES=[("PIO_FIRMWARE_VERSION", env.StringifyMacro(version))])  # noqa: F821
