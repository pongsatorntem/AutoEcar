# E-Car Traffic Light Controller — Normal Junction V1

Ready-to-deploy repository for Raspberry Pi 4B + four TF-Mini Plus sensors + 1–7 independent 64×32 HUB75 displays.

## System behavior
- S1 -> S2 within `pair_window_s` confirms approach -> all displays YELLOW.
- S3 -> S4 confirms red zone -> all displays RED immediately.
- Reverse order is ignored.
- Debounce + gap hold merges cab/body/dolly gaps into one convoy.
- RED fixed 3 s; RETURN yellow fixed 5 s.
- If a yellow convoy is still active at end of RETURN, stay YELLOW with no green flash.
- Sensor offline does not force RED; all displays add `ERR:Sx` while continuing the main state.
- A display that loses controller MQTT adds `LINK ERR` while retaining its last state.
- Special junction is intentionally deferred.

See:
- [`docs/LOGIC.md`](docs/LOGIC.md)
- [`docs/OPERATOR_GUIDE.md`](docs/OPERATOR_GUIDE.md)
- [`docs/FRIDAY_1H_TEST_PLAN.md`](docs/FRIDAY_1H_TEST_PLAN.md)
- [`docs/REVIEW_AND_RISKS.md`](docs/REVIEW_AND_RISKS.md)

## Raspberry Pi target
Observed Pi: Debian 12 Bookworm, kernel 6.6 Raspberry Pi arm64. Login user may remain `trafficlight`; system service uses separate user `trafficlightsvc`. Installer sets hostname to `trafficlight`.

## Network
- Wi-Fi: maintenance only; optional profiles `TPCAP_AUTO-E-CAR`, `TOP`, `TOPTOP_5G`.
- Ethernet: isolated display network.
  - Pi: `10.77.0.1/24`
  - D1..D7: `10.77.0.11` .. `10.77.0.17`
  - MQTT: Pi port 1883

## Install

```bash
git clone <YOUR_REPO_URL> ~/trafficlight
cd ~/trafficlight
sudo ./install.sh
```

Optional Wi-Fi:

```bash
sudo /opt/trafficlight/scripts/setup_wifi.sh
```

Map USB-RS485 adapters:

```bash
sudo /opt/trafficlight/scripts/generate_udev_rules.sh
sudo cp /tmp/99-sensors.rules /etc/udev/rules.d/99-sensors.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
# replug USB adapters if symlinks do not appear
ls -l /dev/traffic-S*
```

Preflight and start:

```bash
sudo /opt/trafficlight/scripts/friday_preflight.sh
sudo systemctl start trafficlight
sudo journalctl -u trafficlight -f
```

Autostart is already enabled by the installer.

## ESP32 displays
PlatformIO has seven environments. Each environment derives its fixed IP from the display ID:

```bash
cd esp32_display
pio run -e display1
pio run -e display2
# ... display7
```

All displays subscribe to one command topic and therefore change state together.

### Mandatory bench check before Friday
The BOM identifies the exact controller as ArtronShop ESP-HUB75 product 03K26. Its published 64x32 example pin mapping is explicitly configured in firmware. Bench-test one board/panel before flashing all seven. See `docs/REVIEW_AND_RISKS.md`. W5500 uses CS=10, MOSI=11, SCK=12, MISO=13.

## Tests

```bash
./scripts/self_test.sh
```

Current repository unit suite covers directional pair logic, reverse lockout, offline pending reset, state timing, debounce, dolly gap hold, and new-vehicle gap behavior.
