# Deploy checklist

1. Confirm Pi is Debian 12 Bookworm arm64.
2. Clone repo to the Pi.
3. Run `sudo ./install.sh`.
4. Optional maintenance Wi-Fi: `sudo /opt/trafficlight/scripts/setup_wifi.sh`.
5. Plug the four USB-RS485 adapters one by one and run `/opt/trafficlight/scripts/detect_usb_rs485.sh`.
6. Build `/etc/udev/rules.d/99-sensors.rules` from the template using the real adapter serial numbers.
7. Edit `/etc/trafficlight/settings.json` if timing/thresholds differ.
8. Flash each ESP32 display with unique IP 10.77.0.11 through 10.77.0.17.
9. Start: `sudo systemctl start trafficlight`.
10. Observe: `sudo journalctl -u trafficlight -f` and `mosquitto_sub -h 10.77.0.1 -t 'factory/trafficlight/#' -v`.
11. Reboot and verify autostart.

## VNC note
The current Pi output shows RealVNC service files installed but service disabled/inactive. `XDG_CURRENT_DESKTOP` being blank in an SSH shell alone does not prove that a desktop environment is absent. The traffic-light service does not depend on VNC or a desktop.
