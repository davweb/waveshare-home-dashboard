"""OTA firmware management for the dashboard server.

Firmware is stored in OTA_FIRMWARE_DIR (default /tmp/ota/).
Two files are kept in that directory:
  firmware.bin  — the raw ESP32 firmware image
  version.txt   — a single-line version string (e.g. "1.0.1")

Endpoints (wired in __main__.py):
  GET  /ota/version           → {"version": "1.0.1", "available": true, "size": 12345}
  GET  /ota/firmware          → raw binary stream (firmware.bin)
  POST /ota/firmware?version= → upload new firmware; version= is required
"""

import logging
import os
from pathlib import Path

logger = logging.getLogger(__name__)

_FIRMWARE_FILENAME = 'firmware.bin'
_VERSION_FILENAME  = 'version.txt'


def _firmware_dir() -> Path:
    path = Path(os.environ.get('OTA_FIRMWARE_DIR', '/tmp/ota'))
    path.mkdir(parents=True, exist_ok=True)
    return path


def get_firmware_path() -> Path | None:
    """Return the path to the stored firmware binary, or None if absent."""
    p = _firmware_dir() / _FIRMWARE_FILENAME
    return p if p.exists() else None


def get_version() -> str | None:
    """Return the stored firmware version string, or None if absent."""
    p = _firmware_dir() / _VERSION_FILENAME
    if not p.exists():
        return None
    return p.read_text(encoding='utf-8').strip()


def save_firmware(data: bytes, version: str) -> None:
    """Atomically replace the stored firmware binary and version."""
    d = _firmware_dir()
    # Write to temp files then rename so reads never see partial writes
    bin_tmp = d / (_FIRMWARE_FILENAME + '.tmp')
    ver_tmp = d / (_VERSION_FILENAME  + '.tmp')
    bin_tmp.write_bytes(data)
    ver_tmp.write_text(version, encoding='utf-8')
    bin_tmp.rename(d / _FIRMWARE_FILENAME)
    ver_tmp.rename(d / _VERSION_FILENAME)
    logger.info('Saved firmware version %s (%d bytes)', version, len(data))
