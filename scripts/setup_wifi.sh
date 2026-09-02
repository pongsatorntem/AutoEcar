#!/usr/bin/env bash
set -euo pipefail
if ! command -v nmcli >/dev/null 2>&1; then
  echo "NetworkManager/nmcli not found. Configure Wi-Fi manually."
  exit 1
fi

add_profile() {
  local ssid="$1" priority="$2" pass
  read -r -s -p "Password for ${ssid} (leave blank to skip): " pass; echo
  [[ -n "$pass" ]] || return 0
  if nmcli -t -f NAME con show | grep -Fxq "$ssid"; then
    sudo nmcli con modify "$ssid" wifi.ssid "$ssid" wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$pass" connection.autoconnect yes connection.autoconnect-priority "$priority"
  else
    sudo nmcli con add type wifi ifname wlan0 con-name "$ssid" ssid "$ssid"
    sudo nmcli con modify "$ssid" wifi-sec.key-mgmt wpa-psk wifi-sec.psk "$pass" ipv4.method auto ipv6.method auto connection.autoconnect yes connection.autoconnect-priority "$priority"
  fi
}

add_profile "TPCAP_AUTO-E-CAR" 30
add_profile "TOP" 20
add_profile "TOPTOP_5G" 10
sudo nmcli radio wifi on
sudo nmcli con up "TPCAP_AUTO-E-CAR" 2>/dev/null || sudo nmcli con up "TOP" 2>/dev/null || sudo nmcli con up "TOPTOP_5G" 2>/dev/null || true

echo "Wi-Fi profiles saved only on this Pi. No password is written to the Git repo."
