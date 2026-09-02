#!/usr/bin/env bash
set -euo pipefail
if [[ $EUID -ne 0 ]]; then echo "Run: sudo ./install.sh"; exit 1; fi
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP=/opt/trafficlight
CONF=/etc/trafficlight
LOG=/var/log/trafficlight
SVC_USER=trafficlightsvc

apt-get update
apt-get install -y python3 python3-venv python3-pip mosquitto mosquitto-clients git python3-pytest
if ! command -v nmcli >/dev/null 2>&1; then
  apt-get install -y network-manager
fi

if ! id "$SVC_USER" >/dev/null 2>&1; then
  useradd --system --create-home --home-dir /var/lib/trafficlight --shell /usr/sbin/nologin "$SVC_USER"
fi
usermod -a -G dialout "$SVC_USER"

# Copy through a staging directory so rerunning from /opt/trafficlight cannot delete the source mid-install.
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp -a "$ROOT"/. "$STAGE"/
rm -rf "$APP"
mkdir -p "$APP" "$CONF" "$LOG"
cp -a "$STAGE"/. "$APP"/
rm -rf "$APP/.git"

python3 -m venv "$APP/.venv"
"$APP/.venv/bin/pip" install --upgrade pip
"$APP/.venv/bin/pip" install -r "$APP/requirements.txt"
[[ -f "$CONF/settings.json" ]] || cp "$APP/config/settings.example.json" "$CONF/settings.json"
chown -R "$SVC_USER":"$SVC_USER" "$LOG"
chmod 750 "$LOG"

hostnamectl set-hostname trafficlight
"$APP/scripts/setup_ethernet.sh"

cp "$APP/install/mosquitto/trafficlight.conf" /etc/mosquitto/conf.d/trafficlight.conf
cp "$APP/install/trafficlight.service" /etc/systemd/system/trafficlight.service
systemctl daemon-reload
systemctl enable mosquitto
systemctl restart mosquitto
systemctl enable trafficlight

echo
echo "INSTALL COMPLETE (service intentionally not started until USB mapping is done)."
echo "1) Optional Wi-Fi: sudo $APP/scripts/setup_wifi.sh"
echo "2) Map USB-RS485: sudo $APP/scripts/generate_udev_rules.sh"
echo "3) Install generated udev rules and replug adapters"
echo "4) Verify: $APP/scripts/friday_preflight.sh"
echo "5) Start: sudo systemctl start trafficlight"
echo "6) Logs: sudo journalctl -u trafficlight -f"
echo "7) Display-only test: $APP/scripts/display_test.sh"
