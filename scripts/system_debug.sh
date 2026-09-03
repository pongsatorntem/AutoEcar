#!/usr/bin/env bash
set -u

section(){ printf '\n== %s ==\n' "$*"; }

section "Host"
hostnamectl 2>/dev/null || hostname || true
date -Is 2>/dev/null || date || true

section "Services"
systemctl --no-pager --full status trafficlight mosquitto 2>/dev/null || true

section "Recent trafficlight journal"
journalctl -u trafficlight -n 120 --no-pager 2>/dev/null || true

section "Recent mosquitto journal"
journalctl -u mosquitto -n 80 --no-pager 2>/dev/null || true

section "Serial devices"
ls -l /dev/ttyUSB* /dev/ttyACM* /dev/traffic-S* 2>/dev/null || true

section "traffic-S symlinks"
for sensor in S1 S2 S3 S4; do
  if [[ -e "/dev/traffic-$sensor" ]]; then
    printf '[PASS] /dev/traffic-%s -> %s\n' "$sensor" "$(readlink -f "/dev/traffic-$sensor" 2>/dev/null || echo unknown)"
  else
    printf '[FAIL] /dev/traffic-%s missing\n' "$sensor"
  fi
done

section "USB serial adapter enumeration"
"$(dirname "$0")/detect_usb_rs485.sh" 2>/dev/null || true