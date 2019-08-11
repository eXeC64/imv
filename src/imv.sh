#!/usr/bin/sh

if [ -n "${WAYLAND_DISPLAY}" ]; then
  exec imv-wl "$@"
else
  exec imv-x11 "$@"
fi
