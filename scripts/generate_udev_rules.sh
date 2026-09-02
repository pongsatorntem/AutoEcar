#!/usr/bin/env bash
set -euo pipefail
OUT="${1:-/tmp/99-sensors.rules}"
: > "$OUT"
echo "This wizard maps S1..S4. Keep each adapter in its FINAL USB-hub port."
echo "Physical USB path is preferred because identical USB-RS485 adapters may have no serial or duplicate serials."
for S in S1 S2 S3 S4; do
  echo
  echo "=== $S ==="
  echo "Identify the USB-RS485 adapter for $S, then press Enter."
  read -r
  mapfile -t DEVS < <(find /dev -maxdepth 1 \( -name 'ttyUSB*' -o -name 'ttyACM*' \) -printf '%p\n' | sort)
  printf 'Detected:\n'; printf '  %s\n' "${DEVS[@]}"
  read -r -p "Device path for $S (example /dev/ttyUSB0): " DEV
  [[ -e "$DEV" ]] || { echo "Not found: $DEV"; exit 1; }
  PROPS="$(udevadm info --query=property --name="$DEV")"
  IDPATH="$(grep '^ID_PATH=' <<<"$PROPS" | cut -d= -f2- || true)"
  SERIAL="$(grep '^ID_SERIAL_SHORT=' <<<"$PROPS" | cut -d= -f2- || true)"
  if [[ -n "$IDPATH" ]]; then
    echo "SUBSYSTEM==\"tty\", ENV{ID_PATH}==\"$IDPATH\", SYMLINK+=\"traffic-$S\", GROUP=\"dialout\", MODE=\"0660\"" >> "$OUT"
    echo "$S -> physical USB path $IDPATH"
  elif [[ -n "$SERIAL" ]]; then
    echo "SUBSYSTEM==\"tty\", ATTRS{serial}==\"$SERIAL\", SYMLINK+=\"traffic-$S\", GROUP=\"dialout\", MODE=\"0660\"" >> "$OUT"
    echo "$S -> serial $SERIAL"
  else
    echo "ERROR: cannot uniquely identify $DEV. Use a USB adapter/hub path that exposes ID_PATH."
    exit 1
  fi
done

echo
echo "Generated: $OUT"
cat "$OUT"
echo "Install with: sudo cp '$OUT' /etc/udev/rules.d/99-sensors.rules && sudo udevadm control --reload-rules && sudo udevadm trigger"
