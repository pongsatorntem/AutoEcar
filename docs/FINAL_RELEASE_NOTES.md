# Traffic Light — Final Release Notes

## Final display rule
- GREEN: full-screen green, no normal text.
- YELLOW: full-screen yellow with black `CAUTION`.
- RED: full-screen red with white `STOP`.
- RETURN: same as YELLOW.
- Sensor/link faults remain small corner/bottom warnings and never replace the traffic state.
- All displays in the junction show the same state simultaneously.

## Logic locked for Friday test
- TF-Mini Plus x4.
- Forward direction only: S1 -> S2 confirms Yellow, S3 -> S4 confirms Red.
- Timestamp window prevents raw simultaneous-AND dependency.
- Debounce + gap-hold merges cab/body/operator/dolly gaps into one convoy.
- Red overwrites Idle/Yellow immediately.
- RED fixed 3 s -> RETURN yellow fixed 5 s.
- If the yellow convoy is still active after RETURN, continue Yellow without a green flash.
- Sensor failure does not force the entire junction Red; it is displayed as a fault badge and logged.
- Special-junction priority logic is intentionally deferred.

## Friday rule
Do not change multiple timing parameters simultaneously. Verify sensor distance/strength first, then tune `gap_hold_s` only if the cab/dolly convoy is being split. Change `pair_window_s` only if a correct S1->S2 or S3->S4 passage is missed.

## 1.0.1-final — Raspberry Pi Bookworm install hotfix
- Removed duplicate Mosquitto `persistence_location` directives that prevented the broker from starting on Debian 12 Bookworm.
- Installer now updates `/etc/hosts` when hostname changes to `trafficlight`, preventing `sudo: unable to resolve host trafficlight`.
- Installer runs unit tests inside the production virtual environment after installing runtime dependencies.
- `scripts/self_test.sh` now prefers the production virtual environment and safely skips dependency-based tests before installation.

## v1.0.2-final — installer hardening from real Pi test

- Fixed Debian Bookworm Mosquitto `Duplicate persistence_location` failure permanently.
- Fixed hostname change order so `/etc/hosts` is updated before `hostnamectl`.
- `pytest` is installed inside the production virtualenv before tests run.
- Installer performs Python unit tests, JSON validation, shell syntax checks, Mosquitto active/listener checks, and a real MQTT pub/sub smoke test.
- Installation does not require sensors, USB-RS485 adapters, Ethernet cable, LAN switch, or ESP displays to be connected.
- `trafficlight.service` is enabled but deliberately left stopped until S1-S4 USB mapping is completed.
- Installer is safe to rerun and preserves an existing `/etc/trafficlight/settings.json`.
- Before sensor mapping, `trafficlight.service` is now disabled as well as stopped, so an accidental reboot cannot start the controller against unmapped `/dev/traffic-S*` devices.
- The USB mapping wizard now installs its own udev rules and enables/starts autostart only after all four stable sensor symlinks are verified.

## 1.0.3-final
- Fixes a Raspberry Pi Bookworm boot race found on real hardware: Mosquitto could start before NetworkManager assigned `10.77.0.1/24` to `eth0`, then fail because the listener is bound to that address.
- Installer now installs `/etc/systemd/system/mosquitto.service.d/trafficlight-network.conf` and waits (up to 60 s) for `10.77.0.1/24` before Mosquitto starts.
- MQTT remains bound only to the private display LAN instead of exposing the anonymous broker on maintenance Wi-Fi.
