#!/bin/sh
# Renders every screen of the UI on the host (Linux container, software renderer) into BMP files.
# Usage: tools/host_preview.sh <fake_console_dir> <out_dir>
set -eu
cd "$(dirname "$0")/.."
FAKE="$1"; OUT="$2"
mkdir -p "$OUT"
docker run --rm -v "$PWD":/project -v "$FAKE":/fake -v "$OUT":/out ubuntu:24.04 bash -c '
  set -e
  apt-get update -qq >/dev/null
  DEBIAN_FRONTEND=noninteractive apt-get install -y -qq g++ libsdl2-dev libsdl2-ttf-dev libsdl2-gfx-dev fonts-dejavu-core >/dev/null
  cd /project
  g++ -std=c++20 -O1 -Wall -Wextra -I/usr/include/SDL2 -Isource $(ls source/*.cpp | grep -v platform_wiiu) host/platform_host.cpp \
      -o /tmp/hometidy_host $(sdl2-config --libs) -lSDL2_ttf -lSDL2_gfx
  rm -rf /tmp/fake && cp -r /fake /tmp/fake
  SDL_VIDEODRIVER=dummy HOMETIDY_ROOT=/tmp/fake HOMETIDY_SHOTS=/out /tmp/hometidy_host
  cp /tmp/fake/sd/wiiu/apps/HomeTidy/hometidy.log /out/ 2>/dev/null || true
'
