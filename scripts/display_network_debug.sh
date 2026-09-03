#!/usr/bin/env bash
set -u

PASS=0
WARN=0
FAIL=0
BROKER="${BROKER:-10.77.0.1}"
BASE_TOPIC="${BASE_TOPIC:-factory/trafficlight}"
JUNCTION_ID="${JUNCTION_ID:-1}"
IFACE="${IFACE:-eth0}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

pass(){ printf '[PASS] %s\n' "$*"; PASS=$((PASS+1)); }
warn(){ printf '[WARN] %s\n' "$*"; WARN=$((WARN+1)); }
fail(){ printf '[FAIL] %s\n' "$*"; FAIL=$((FAIL+1)); }
section(){ printf '\n== %s ==\n' "$*"; }
have(){ command -v "$1" >/dev/null 2>&1; }

section "Host"
printf 'hostname: %s\n' "$(hostname 2>/dev/null || echo unknown)"
printf 'date: %s\n' "$(date -Is 2>/dev/null || date 2>/dev/null || echo unknown)"
if [[ -f "$ROOT/VERSION" ]]; then
  printf 'VERSION: %s\n' "$(tr -d '\r\n' < "$ROOT/VERSION")"
else
  warn "VERSION file missing"
fi
if git -C "$ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  printf 'git_commit: %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null)"
else
  warn "not a git checkout"
fi

section "Ethernet $IFACE"
if ip link show "$IFACE" >/dev/null 2>&1; then
  ip -br link show "$IFACE" || true
  ip -4 -br addr show "$IFACE" || true
  if ip -4 addr show "$IFACE" | grep -q '10\.77\.0\.1/24'; then
    pass "$IFACE has 10.77.0.1/24"
  else
    fail "$IFACE missing 10.77.0.1/24"
  fi
  if [[ -r "/sys/class/net/$IFACE/carrier" ]]; then
    carrier="$(cat "/sys/class/net/$IFACE/carrier" 2>/dev/null || echo unknown)"
    printf 'carrier: %s\n' "$carrier"
    [[ "$carrier" == "1" ]] && pass "$IFACE carrier on" || warn "$IFACE carrier is not on"
  else
    warn "$IFACE carrier file unavailable"
  fi
else
  fail "$IFACE not found"
fi

section "Routes"
ip route || warn "ip route failed"

section "Neighbor / ARP"
ip neigh show dev "$IFACE" 2>/dev/null || ip neigh show || warn "ip neigh failed"

section "Mosquitto"
if have systemctl; then
  if systemctl is-active --quiet mosquitto; then
    pass "mosquitto service active"
  else
    fail "mosquitto service inactive"
  fi
  systemctl --no-pager --full status mosquitto 2>/dev/null | sed -n '1,12p' || true
else
  warn "systemctl unavailable"
fi
if have ss; then
  if ss -ltn 2>/dev/null | awk '{print $4}' | grep -Eq '(^|:)1883$'; then
    pass "TCP port 1883 is listening"
  else
    fail "TCP port 1883 is not listening"
  fi
  ss -ltn 2>/dev/null | grep ':1883' || true
else
  warn "ss unavailable; cannot check listening ports"
fi

section "Display Ping"
for id in 1 2 3 4 5 6 7; do
  ipaddr="10.77.0.$((10 + id))"
  if ping -c 1 -W 1 "$ipaddr" >/dev/null 2>&1; then
    pass "D$id $ipaddr ping"
  else
    fail "D$id $ipaddr ping failed"
  fi
done

section "Display MQTT Status"
topic="$BASE_TOPIC/junction/$JUNCTION_ID/display/+/status"
printf 'topic: %s\n' "$topic"
if have mosquitto_sub; then
  output="$(timeout 4 mosquitto_sub -h "$BROKER" -t "$topic" -v -C 7 -W 3 2>&1 || true)"
  if [[ -n "$output" ]]; then
    printf '%s\n' "$output"
    pass "received display MQTT status messages"
  else
    warn "no display MQTT status messages received"
  fi
else
  warn "mosquitto_sub unavailable"
fi

section "Summary"
printf 'PASS=%d WARN=%d FAIL=%d\n' "$PASS" "$WARN" "$FAIL"
[[ $FAIL -eq 0 ]]