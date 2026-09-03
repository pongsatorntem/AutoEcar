#!/usr/bin/env bash
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "ERROR: run with sudo: sudo $0"
  exit 1
fi

OUT="/etc/udev/rules.d/99-trafficlight-sensors.rules"
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
: > "$TMP"

echo "E-Car Traffic Light USB-RS485 mapping wizard"
echo "Maps S1..S4 to stable /dev/traffic-S1 .. /dev/traffic-S4 names."
echo "Keep each adapter in its FINAL USB-hub port."
echo "Physical USB path is preferred because identical adapters may have no serial or duplicate serials."

for S in S1 S2 S3 S4; do
  echo
  echo "=== $S ==="
  echo "Connect/identify the USB-RS485 adapter for $S, then press Enter."
  read -r
  mapfile -t DEVS < <(find /dev -maxdepth 1 \( -name 'ttyUSB*' -o -name 'ttyACM*' \) -printf '%p\n' | sort)
  if [[ ${#DEVS[@]} -eq 0 ]]; then
    echo "ERROR: no ttyUSB/ttyACM adapter detected."
    exit 1
  fi
  printf 'Detected:\n'; printf '  %s\n' "${DEVS[@]}"
  read -r -p "Device path for $S (example /dev/ttyUSB0): " DEV
  [[ -e "$DEV" ]] || { echo "ERROR: not found: $DEV"; exit 1; }

  PROPS="$(udevadm info --query=property --name="$DEV")"
  IDPATH="$(grep '^ID_PATH=' <<<"$PROPS" | cut -d= -f2- || true)"
  SERIAL="$(grep '^ID_SERIAL_SHORT=' <<<"$PROPS" | cut -d= -f2- || true)"

  if [[ -n "$IDPATH" ]]; then
    echo "SUBSYSTEM==\"tty\", ENV{ID_PATH}==\"$IDPATH\", SYMLINK+=\"traffic-$S\", GROUP=\"dialout\", MODE=\"0660\"" >> "$TMP"
    echo "$S -> physical USB path $IDPATH"
  elif [[ -n "$SERIAL" ]]; then
    echo "SUBSYSTEM==\"tty\", ATTRS{serial}==\"$SERIAL\", SYMLINK+=\"traffic-$S\", GROUP=\"dialout\", MODE=\"0660\"" >> "$TMP"
    echo "$S -> serial $SERIAL"
  else
    echo "ERROR: cannot uniquely identify $DEV. Use a USB adapter/hub path that exposes ID_PATH."
    exit 1
  fi
done

cp "$TMP" "$OUT"
chmod 644 "$OUT"
udevadm control --reload-rules
udevadm trigger --subsystem-match=tty
sleep 1

echo
echo "Installed rules: $OUT"
cat "$OUT"
echo

MISSING=0
for S in S1 S2 S3 S4; do
  if [[ -e "/dev/traffic-$S" ]]; then
    echo "[PASS] /dev/traffic-$S -> $(readlink -f "/dev/traffic-$S")"
  else
    echo "[WAIT] /dev/traffic-$S is not present yet. Replug that adapter in its mapped USB port."
    MISSING=1
  fi
done

if [[ $MISSING -ne 0 ]]; then
  echo
  echo "Rules are installed, but one or more adapters need to be replugged."
  echo "After all four /dev/traffic-S* links exist, rerun this command; you may select the same devices."
  echo "The trafficlight service remains disabled until all four mappings verify."
  exit 2
fi

systemctl daemon-reload
systemctl enable trafficlight >/dev/null
systemctl restart trafficlight

echo
echo "[PASS] All four sensor mappings verified."
echo "[PASS] trafficlight.service enabled for autostart and started now."
echo "Live log: sudo journalctl -u trafficlight -f"
