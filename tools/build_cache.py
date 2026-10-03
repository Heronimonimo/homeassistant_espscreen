#!/usr/bin/env python3
"""Make one board's compile cache the way ESP Screens builds (app 0.4.49, .github/workflows/build-cache.yml).

    python3 tools/build_cache.py BOARD OUT_DIR

Runs in the app's own image (ghcr.io/esphome/esphome at the version of screen_manager/Dockerfile) with the app's own
paths, because a cache only answers a compile whose paths and compiler match: the profile goes in
/homeassistant/esphome and everything built in /data, exactly as the app's Firmware class does it for a new screen.
It is that class that writes the profile and runs the build, so the YAML, the environment and the ccache options
cannot drift from what users get. The build starts from an empty cache and leaves
OUT_DIR/ccache-<board>-esphome-<version>.tar.gz (build_cache.asset_name), which screen_manager/app/build_cache.py
fetches before a screen's build.
"""
import asyncio
import os
from pathlib import Path
import sys
import tarfile
import time

APP = Path(__file__).resolve().parent.parent / 'screen_manager' / 'app'
sys.path.insert(0, str(APP))
os.environ['ESP_SCREENS_BUILD_CACHE'] = 'off'  # start empty: never fetch the cache this job is making

import build_cache  # noqa: E402
from core import BOARD_KEYS  # noqa: E402
from firmware import Firmware  # noqa: E402

ROOT = Path(os.environ.get('ESPHOME_CONFIG', '/homeassistant/esphome'))
DATA = Path(os.environ.get('SCREEN_DATA', '/data'))


async def main(board, out):
    if board not in BOARD_KEYS:
        sys.exit(f'No such board: {board} (boards.json has {", ".join(BOARD_KEYS)})')
    esphome = build_cache.esphome_version()
    if not esphome:
        sys.exit('ESPHome is not installed here')
    firmware = Firmware(ROOT, DATA)
    folder = Path(firmware.build_env(firmware.root / 'x.yaml').get('CCACHE_DIR') or DATA / 'idf' / 'ccache')
    if folder.exists() and any(folder.iterdir()):
        sys.exit(f'{folder} is not empty; a cache is made from a clean start')
    name = f'cache-{board}'
    # Placeholders of the usual length: the build needs Wi-Fi, and nothing here leaves the job but the ccache.
    profile = firmware.create({'board': board, 'name': name, 'friendly_name': f'Cache {board}',
                               'wifi_ssid': 'build-cache-network', 'wifi_password': 'build-cache-password'})
    started = time.monotonic()
    firmware.start({'file': profile['file'], 'action': 'build'})
    await firmware.task
    for line in firmware.logs:
        print(line)
    if firmware.job.get('state') != 'success':
        sys.exit(f'The build of {board} failed')
    out.mkdir(parents=True, exist_ok=True)
    archive = out / build_cache.asset_name(board, esphome)
    with tarfile.open(archive, 'w:gz', compresslevel=6) as tar:
        tar.add(folder, arcname='.')
    print(f'{board}: built in {time.monotonic() - started:.0f} s, {archive.name} {archive.stat().st_size // 1_000_000} MB')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    asyncio.run(main(sys.argv[1], Path(sys.argv[2])))
